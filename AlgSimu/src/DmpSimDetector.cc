/*
 *  $Id: DmpSimDetector.cc, 2014-09-30 14:07:44 DAMPE $
 *  Author(s):
 *    Chi WANG (chiwang@mail.ustc.edu.cn) 26/02/2014
 *    Yifeng Wei (weiyf@mail.ustc.edu.cn) 28/09/2014
*/

#include <stdlib.h>     // getenv()

//#include "G4RotationMatrix.hh"
#include "G4GDMLParser.hh"
#include "G4SDManager.hh"
#include "G4FieldManager.hh"
#include "G4VisAttributes.hh"
#include "G4LogicalVolumeStore.hh"
#include "G4PhysicalVolumeStore.hh"

#include "DmpLog.h"
#include "DmpDataBuffer.h"
#include "DmpMetadata.h"
#include "DmpSimMagneticField.h"

#include "DmpSimDetector.h"
//#include "DmpSimPsdSD.h"
//#include "DmpSimStkSD.h"
#include "DmpSimBgoSD.h"
//#include "DmpSimNudSD.h"

//-------------------------------------------------------------------
DmpSimDetector::DmpSimDetector()
 :fParser(0),
  fPhyVolume(0),
//  fPsdSD(0),
//  fStkSD(0),
  fBgoSD(0)
//  fNudSD(0)
{
  fParser = new G4GDMLParser();
//  fPsdSD = new DmpSimPsdSD();
//  fStkSD = new DmpSimStkSD();
  fBgoSD = new DmpSimBgoSD();
//  fNudSD = new DmpSimNudSD();
  fMetadata = dynamic_cast<DmpMetadata*>(gDataBuffer->ReadObject("Metadata/MCTruth/JobOpt"));
}

//-------------------------------------------------------------------
DmpSimDetector::~DmpSimDetector(){
  delete fParser;
//  delete fPsdSD;
//  delete fStkSD;
  delete fBgoSD;
//  delete fNudSD;
}

//-------------------------------------------------------------------
#include <boost/filesystem.hpp>     // path
G4VPhysicalVolume* DmpSimDetector::Construct(){
  boost::filesystem::path   gdmlFile=fMetadata->Option["Gdml"];
  if(gdmlFile.extension().string() != ".gdml"){    // argv = sub-directory
    gdmlFile = (std::string)getenv("DMPSWSYS")+"/share/Geometry/"+gdmlFile.string()+"/DAMPE.gdml";
  }
  char *dirTmp = getcwd(NULL,NULL);
  DmpLogInfo<<"Reading GDML:\t"<<gdmlFile.string()<<DmpLogEndl;
  chdir(gdmlFile.parent_path().string().c_str());
  fParser->Read(gdmlFile.filename().string().c_str());
  fPhyVolume = fParser->GetWorldVolume();
  fPhyVolume->GetLogicalVolume()->SetVisAttributes(G4VisAttributes::Invisible);
  chdir(dirTmp);

  Adjustment();

  // *
  // *  TODO: set SD of SubDet at here
  // *
  G4SDManager *mgrSD = G4SDManager::GetSDMpointer();
  if(fParser->GetVolume("Psd_Det")){
    DmpLogInfo<<"Setting Sensitive Detector of Psd"<<DmpLogEndl;
    //mgrSD->AddNewDetector(fPsdSD);
    //fParser->GetVolume("Psd_Strip0")->SetSensitiveDetector(fPsdSD);
    //fParser->GetVolume("Psd_Strip1")->SetSensitiveDetector(fPsdSD);
  }
  if(fParser->GetVolume("Stk_Det")){
          /*
    DmpLogInfo<<" Setting Sensitive Detector of Stk"<<DmpLogEndl;
    mgrSD->AddNewDetector(fStkSD);
    fParser->GetVolume("Stk_Strip")->SetSensitiveDetector(fStkSD);
    */
  }
  if(fParser->GetVolume("Bgo_Det")){
    DmpLogInfo<<"Setting Sensitive Detector of Bgo"<<DmpLogEndl;
    mgrSD->AddNewDetector(fBgoSD);
    fParser->GetVolume("Bgo_Bar")->SetSensitiveDetector(fBgoSD);
  }
  if(fParser->GetVolume("Nud_Det")){
    DmpLogInfo<<"Setting Sensitive Detector of Nud"<<DmpLogEndl;
    //mgrSD->AddNewDetector(fNudSD);
    //fParser->GetVolume("Nud_Block0")->SetSensitiveDetector(fNudSD);
    //fParser->GetVolume("Nud_Block1")->SetSensitiveDetector(fNudSD);
    //fParser->GetVolume("Nud_Block2")->SetSensitiveDetector(fNudSD);
    //fParser->GetVolume("Nud_Block3")->SetSensitiveDetector(fNudSD);
  }

  return fPhyVolume;
}

//-------------------------------------------------------------------
void DmpSimDetector::Adjustment()const{
  G4LogicalVolume *auxiliary_LV = G4LogicalVolumeStore::GetInstance()->GetVolume("AuxDet",false);
  if(auxiliary_LV){
std::cout<<"DEBUG: "<<__FILE__<<"("<<__LINE__<<")"<<std::endl;
    auxiliary_LV->SetVisAttributes(G4VisAttributes::Invisible);
    ResetMagnetic();
  }
  G4VPhysicalVolume *payload_PV = G4PhysicalVolumeStore::GetInstance()->GetVolume("DAMPE_PV",false);
  if(payload_PV){
    payload_PV->GetLogicalVolume()->SetVisAttributes(G4VisAttributes::Invisible);
    DAMPETranslation(payload_PV);
    DAMPERotation(payload_PV);
  }
}


//-------------------------------------------------------------------
void DmpSimDetector::DAMPETranslation(G4VPhysicalVolume *PV)const{
  DmpLogInfo<<"Translating DAMPE"<<DmpLogEndl;
  double x=0,y=0.,z=0.0;
  std::string cmd = fMetadata->Option["BT/DAMPE/Translation"];
  std::istringstream iss(cmd);
  iss>>x>>y>>z;
  G4ThreeVector par = PV->GetTranslation();
  par.setX(par.x()+x);
  par.setY(par.y()+y);
  par.setZ(par.z()+z);
  PV->SetTranslation(par);
}

//-------------------------------------------------------------------
void DmpSimDetector::DAMPERotation(G4VPhysicalVolume *PV)const{
  DmpLogInfo<<"Rotating DAMPE"<<DmpLogEndl;
  double degree = 0.0;
  std::string cmd = fMetadata->Option["BT/DAMPE/Rotation"];
  std::istringstream iss(cmd);
  iss>>degree;
  static G4RotationMatrix rot;
  rot.rotateY(degree/180*3.141592653);
  PV->SetRotation(&rot); // TODO: set me, gMetadataSim->SourceDirection
}

//-------------------------------------------------------------------
void DmpSimDetector::ResetMagnetic()const{
  G4LogicalVolume *LV = G4LogicalVolumeStore::GetInstance()->GetVolume("B_field");
  if(LV){
    DmpLogInfo<<"Setting magnetic filed"<<DmpLogEndl;
    double x=0,y=0.,z=0.0;
    std::string cmd = fMetadata->Option["BT/Magnetic"];
    std::istringstream iss(cmd);
    iss>>x>>y>>z;
    static DmpSimMagneticField magneticField(x,y,z);
    static G4FieldManager fieldMgr;
    fieldMgr.SetDetectorField(&magneticField);
    fieldMgr.CreateChordFinder(&magneticField);
    LV->SetFieldManager(&fieldMgr,true);
  }
}

