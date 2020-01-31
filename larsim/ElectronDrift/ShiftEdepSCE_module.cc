////////////////////////////////////////////////////////////////////////
// Class:       ShiftEdepSCE
// Plugin Type: producer (art v2_05_01)
// File:        ShiftEdepSCE_module.cc
//
// Generated at Thu Apr 19 00:41:18 2018 by Wesley Ketchum using cetskelgen
// from cetlib version v1_21_00.
////////////////////////////////////////////////////////////////////////

#include "art/Framework/Core/EDProducer.h"
#include "art/Framework/Core/ModuleMacros.h"
#include "art/Framework/Principal/Event.h"
#include "art/Framework/Principal/Handle.h"
#include "canvas/Utilities/InputTag.h"
#include "fhiclcpp/ParameterSet.h"
#include "art_root_io/TFileService.h"

#include <memory>

#include "lardataobj/Simulation/SimEnergyDeposit.h"
#include "larevt/SpaceChargeServices/SpaceChargeService.h"
#include "lardata/DetectorInfoServices/DetectorPropertiesService.h"
#include "larsim/Simulation/LArG4Parameters.h"
#include "lardata/DetectorInfoServices/LArPropertiesService.h"
#include "larsim/IonizationScintillation/ISCalcSeparate.h"
#include "TNtuple.h"

namespace spacecharge {
  class ShiftEdepSCE;
}


class spacecharge::ShiftEdepSCE : public art::EDProducer {
public:
  explicit ShiftEdepSCE(fhicl::ParameterSet const & p);
  // The compiler-generated destructor is fine for non-base
  // classes without bare pointers or other resource use.

  // Plugins should not be copied or assigned.
  ShiftEdepSCE(ShiftEdepSCE const &) = delete;
  ShiftEdepSCE(ShiftEdepSCE &&) = delete;
  ShiftEdepSCE & operator = (ShiftEdepSCE const &) = delete;
  ShiftEdepSCE & operator = (ShiftEdepSCE &&) = delete;

  // Required functions.
  void produce(art::Event & e) override;
  void beginJob() override;

private:

  // Declare member data here.
  art::InputTag fEDepTag;
  bool          fMakeAnaTree;

  //IS calculationg
  larg4::ISCalcSeparate fISAlg;

  TTree*      fEdepAnaTree;
  typedef struct {

    float energy;
    
    float orig_x;
    float orig_y;
    float orig_z;

    float shift_x;
    float shift_y;
    float shift_z;
    
    float efield_bulk;
    float efield_off_x;
    float efield_off_y;
    float efield_off_z;
    float efield_sce;
    
    float scint_yield;

    int orig_el;
    int orig_ph_f;
    int orig_ph_s;

    int shift_el;
    int shift_ph_f;
    int shift_ph_s;

    int trkid;
    int pdg;

  } EdepAna_t;
  EdepAna_t fEdepAna;

};


spacecharge::ShiftEdepSCE::ShiftEdepSCE(fhicl::ParameterSet const & p)
  : EDProducer{p}
  , fEDepTag(p.get<art::InputTag>("EDepTag")),
    fMakeAnaTree(p.get<bool>("MakeAnaTree",true))
{
  produces< std::vector<sim::SimEnergyDeposit> >();
}

void spacecharge::ShiftEdepSCE::beginJob()
{
  if(fMakeAnaTree){
    art::ServiceHandle<art::TFileService const> tfs;
    fEdepAnaTree = tfs->make<TTree>("nt_edep_ana","Edep PosDiff Ana Ntuple");
    fEdepAnaTree->Branch("edep",&fEdepAna,
			 "energy:orig_x:orig_y:orig_z:shift_x:shift_y:shift_z:efield_bulk:efield_off_x:efield_off_y:efield_off_z:efield_sce:scint_yield:orig_el:orig_ph_f:orig_ph_s:shift_el:shift_ph_f:shift_ph_s:trkid:pdg");
  }

  art::ServiceHandle<sim::LArG4Parameters const> lg4paramHandle;
  fISAlg.Initialize();
}

void spacecharge::ShiftEdepSCE::produce(art::Event & e)
{
  art::ServiceHandle<sim::LArG4Parameters const> lg4paramHandle;
  fISAlg.Initialize();

  auto sce = lar::providerFrom<spacecharge::SpaceChargeService>();

  std::unique_ptr< std::vector<sim::SimEnergyDeposit> >
    outEdepVecPtr(new std::vector<sim::SimEnergyDeposit>() );
  auto & outEdepVec(*outEdepVecPtr);

  art::Handle< std::vector<sim::SimEnergyDeposit> > inEdepHandle;
  e.getByLabel(fEDepTag,inEdepHandle);
  auto const& inEdepVec(*inEdepHandle);

  outEdepVec.reserve(inEdepVec.size());

  geo::Vector_t posOffsetsStart{0.0,0.0,0.0};
  geo::Vector_t posOffsetsEnd{0.0,0.0,0.0};
  for(auto const& edep : inEdepVec){
    if(sce->EnableSimSpatialSCE()){
      posOffsetsStart = sce->GetPosOffsets({edep.StartX(),edep.StartY(),edep.StartZ()});
      posOffsetsEnd = sce->GetPosOffsets({edep.EndX(),edep.EndY(),edep.EndZ()});
    }
    fISAlg.Reset();
    fISAlg.CalcIonAndScint(edep);
    outEdepVec.emplace_back(fISAlg.NumOfPhotons(),
			    fISAlg.NumOfElectrons(),
			    edep.ScintYieldRatio(),
			    edep.Energy(),
			    geo::Point_t{(float)(edep.StartX()-posOffsetsStart.X()), //x should be subtracted
				(float)(edep.StartY()+posOffsetsStart.Y()),
				(float)(edep.StartZ()+posOffsetsStart.Z())},
			    geo::Point_t{(float)(edep.EndX()-posOffsetsEnd.X()), //x should be subtracted
				(float)(edep.EndY()+posOffsetsEnd.Y()),
				(float)(edep.EndZ()+posOffsetsEnd.Z())},
			    edep.StartT(),
			    edep.EndT(),
			    edep.TrackID(),
			    edep.PdgCode());
    if(fMakeAnaTree){

      auto detp = lar::providerFrom<detinfo::DetectorPropertiesService>();

      double efield = detp->Efield();
      geo::Vector_t efieldOffsets{0.0,1.0,1.0};
      if(sce->EnableSimEfieldSCE())
	efieldOffsets = fSCE->GetEfieldOffsets(geo::Point_t{x,y,z});
      double efield_sce = std::sqrt( (efield + efield*efieldOffsets.X())*(efield + efield*efieldOffsets.X()) +
				     (efield*efieldOffsets.Y()*efield*efieldOffsets.Y()) +
				     (efield*efieldOffsets.Z()*efield*efieldOffsets.Z()) );

      fEdepAna.energy = edep.Energy();
      fEdepAna.orig_x=edep.X();
      fEdepAna.orig_y=edep.Y();
      fEdepAna.orig_z=edep.Z();
      fEdepAna.shift_x=outEdepVec.back().X();
      fEdepAna.shift_y=outEdepVec.back().Y();
      fEdepAna.shift_z=outEdepVec.back().Z();
      fEdepAna.efield_bulk = detp->Efield();
      fEdepAna.efield_off_x = efieldOffsets.X();
      fEdepAna.efield_off_y = efieldOffsets.Y();
      fEdepAna.efield_off_z = efieldOffsets.Z();
      fEdepAna.efield_sce = efield_sce;
      fEdepAna.scint_yield = edep.ScintYieldRatio();
      fEdepAna.orig_el = edep.NumElectrons();
      fEdepAna.orig_ph_f = edep.NumFPhotons();
      fEdepAna.orig_ph_s = edep.NumSPhotons();
      fEdepAna.shift_el = outEdepVec.back().NumElectrons();
      fEdepAna.shift_ph_f = outEdepVec.back().NumFPhotons();
      fEdepAna.shift_ph_s = outEdepVec.back().NumSPhotons();

      fNtEdepAna->Fill(edep.Energy(),
		       edep.X(),edep.Y(),edep.Z(),edep.NumElectrons(),edep.NumPhotons(),
		       outEdepVec.back().X(),outEdepVec.back().Y(),outEdepVec.back().Z(),
		       outEdepVec.back().NumElectrons(),outEdepVec.back().NumPhotons());
    }
	//std::cout << "space charge position: (" << edep.X() << ", " << edep.Y() << ", " << edep.Z() << ") --> (" << outEdepVec.back().X() << ", " << outEdepVec.back().Y() << ", " << outEdepVec.back().Z() << ")" << std::endl;
  }


  e.put(std::move(outEdepVecPtr));
}

DEFINE_ART_MODULE(spacecharge::ShiftEdepSCE)
