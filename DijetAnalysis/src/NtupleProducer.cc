// system include files
#include <iostream>
#include <bitset>
#include <exception>

#include "Karma/DijetAnalysis/interface/NtupleProducer.h"

#include "boost/filesystem/path.hpp"
#include "boost/filesystem/operations.hpp"

// -- constructor
dijet::NtupleProducer::NtupleProducer(const edm::ParameterSet& config, const dijet::NtupleProducerGlobalCache* globalCache) : m_configPSet(config) {
    // -- register products
    produces<dijet::NtupleEntry>();

    // -- process configuration

    /// m_triggerEfficienciesProvider = std::unique_ptr<karma::TriggerEfficienciesProvider>(
    ///     new TriggerEfficienciesProvider(m_configPSet.getParameter<std::string>("triggerEfficienciesFile"))
    /// );

    /// std::cout << "Read trigger efficiencies for paths:" << std::endl;
    /// for (const auto& mapIter : m_triggerEfficienciesProvider->triggerEfficiencies()) {
    ///     std::cout << "    " << mapIter.first << " -> " << &(*mapIter.second) << std::endl;
    /// }

    // set a flag if we are running on (real) data
    m_isData = m_configPSet.getParameter<bool>("isData");
    m_weightForStitching = m_configPSet.getParameter<double>("weightForStitching");

    // load external file to get `nPUMean` in data
    if (m_isData) {
        m_npuMeanProvider = std::unique_ptr<karma::NPUMeanProvider>(
            new karma::NPUMeanProvider(
                m_configPSet.getParameter<std::string>("npuMeanFile"),
                m_configPSet.getParameter<double>("minBiasCrossSection")
            )
        );
        std::cout << "Reading nPUMean information from file: " << m_configPSet.getParameter<std::string>("npuMeanFile") << std::endl;
    }

    // load external file to get `pileupWeight` in MC
    if (!m_isData) {
        if (!m_configPSet.getParameter<std::string>("pileupWeightFile").empty()) {
            m_puWeightProvider = std::unique_ptr<karma::PileupWeightProvider>(
                new karma::PileupWeightProvider(
                    m_configPSet.getParameter<std::string>("pileupWeightFile"),
                    m_configPSet.getParameter<std::string>("pileupWeightHistogramName")
                )
            );
        }
        /*
        // can provide an alternative pileup weight file
        if (!m_configPSet.getParameter<std::string>("pileupWeightFileAlt").empty()) {
            m_puWeightProviderAlt = std::unique_ptr<karma::PileupWeightProvider>(
                new karma::PileupWeightProvider(
                    m_configPSet.getParameter<std::string>("pileupWeightFileAlt"),
                    m_configPSet.getParameter<std::string>("pileupWeightHistogramName")
                )
            );
        }
        */
        // can provide pileup weight files for each HLT path
        auto pileupWeightByHLTFileBasename = m_configPSet.getParameter<std::string>("pileupWeightByHLTFileBasename");
        if (!pileupWeightByHLTFileBasename.empty()) {
            m_puWeightProvidersByHLT.resize(globalCache->hltPaths_.size());
            for (size_t iHLTPath = 0; iHLTPath < globalCache->hltPaths_.size(); ++iHLTPath) {
                std::string pileupWeightFileName = pileupWeightByHLTFileBasename + "_" + globalCache->hltPaths_.at(iHLTPath) + ".root";

                if (!boost::filesystem::exists(pileupWeightFileName)) {
                    std::cout << "No HLT-dependent pileup weight information found for trigger path: " << globalCache->hltPaths_.at(iHLTPath) << std::endl;
                    continue;
                }

                std::cout << "Reading HLT-dependent pileup weight information from file: " << pileupWeightFileName << std::endl;
                m_puWeightProvidersByHLT[iHLTPath] = new karma::PileupWeightProvider(
                    pileupWeightByHLTFileBasename + "_" + globalCache->hltPaths_[iHLTPath] + ".root",
                    m_configPSet.getParameter<std::string>("pileupWeightHistogramName")
                );
            }
        }
    }


    // -- construct FlexGrid bin finders with final analysis binning
    if (globalCache->flexGridDijetPtAverage_) {
        m_flexGridBinProviderDijetPtAve = std::unique_ptr<karma::FlexGridBinProvider>(new karma::FlexGridBinProvider(*globalCache->flexGridDijetPtAverage_));
    }
    if (globalCache->flexGridDijetDijetMass_) {
        m_flexGridBinProviderDijetMass = std::unique_ptr<karma::FlexGridBinProvider>(new karma::FlexGridBinProvider(*globalCache->flexGridDijetDijetMass_));
    }

    // -- declare which collections are consumed and create tokens
    karmaEventToken = consumes<karma::Event>(m_configPSet.getParameter<edm::InputTag>("karmaEventSrc"));
    karmaRunToken = consumes<karma::Run, edm::InRun>(m_configPSet.getParameter<edm::InputTag>("karmaRunSrc"));
    karmaVertexCollectionToken = consumes<karma::VertexCollection>(m_configPSet.getParameter<edm::InputTag>("karmaVertexCollectionSrc"));
    karmaJetCollectionToken = consumes<karma::JetCollection>(m_configPSet.getParameter<edm::InputTag>("karmaJetCollectionSrc"));
    karmaMETCollectionToken = consumes<karma::METCollection>(m_configPSet.getParameter<edm::InputTag>("karmaMETCollectionSrc"));
    karmaJetTriggerObjectsMapToken = consumes<karma::JetTriggerObjectsMap>(m_configPSet.getParameter<edm::InputTag>("karmaJetTriggerObjectMapSrc"));
    if (!m_isData) {
        karmaGenJetCollectionToken = consumes<karma::LVCollection>(m_configPSet.getParameter<edm::InputTag>("karmaGenJetCollectionSrc"));
        karmaGeneratorQCDInfoToken = consumes<karma::GeneratorQCDInfo>(m_configPSet.getParameter<edm::InputTag>("karmaGeneratorQCDInfoSrc"));
        karmaJetGenJetMapToken = consumes<karma::JetGenJetMap>(m_configPSet.getParameter<edm::InputTag>("karmaJetGenJetMapSrc"));
        karmaGenParticleCollectionToken = consumes<karma::GenParticleCollection>(m_configPSet.getParameter<edm::InputTag>("karmaGenParticleCollectionSrc"));
    }
    else {
        karmaPrefiringWeightToken = consumes<double>(m_configPSet.getParameter<edm::InputTag>("karmaPrefiringWeightSrc"));
        karmaPrefiringWeightUpToken = consumes<double>(m_configPSet.getParameter<edm::InputTag>("karmaPrefiringWeightUpSrc"));
        karmaPrefiringWeightDownToken = consumes<double>(m_configPSet.getParameter<edm::InputTag>("karmaPrefiringWeightDownSrc"));
    }

}


// -- destructor
dijet::NtupleProducer::~NtupleProducer() {
}

// -- static member functions

/*static*/ std::unique_ptr<dijet::NtupleProducerGlobalCache> dijet::NtupleProducer::initializeGlobalCache(const edm::ParameterSet& pSet) {
    // -- create the GlobalCache
    return std::unique_ptr<dijet::NtupleProducerGlobalCache>(new dijet::NtupleProducerGlobalCache(pSet));
}


/*static*/ std::shared_ptr<dijet::NtupleProducerRunCache> dijet::NtupleProducer::globalBeginRun(const edm::Run& run, const edm::EventSetup& setup, const dijet::NtupleProducer::GlobalCache* globalCache) {
    // -- create the RunCache
    auto runCache = std::make_shared<dijet::NtupleProducerRunCache>(globalCache->pSet_);

    typename edm::Handle<karma::Run> runHandle;
    run.getByLabel(globalCache->pSet_.getParameter<edm::InputTag>("karmaRunSrc"), runHandle);

    // compute the unversioned HLT path names
    // (needed later to get the trigger efficiencies)

    runCache->triggerPathsUnversionedNames_.resize(runHandle->triggerPathInfos.size());
    runCache->triggerPathsIndicesInConfig_.resize(runHandle->triggerPathInfos.size(), -1);
    for (size_t iPath = 0; iPath < runHandle->triggerPathInfos.size(); ++iPath) {
        const std::string& pathName = runHandle->triggerPathInfos[iPath].name_;
        ///std::cout << "[Check iPath " << iPath << "] " << pathName << std::endl;

        boost::smatch matched_substrings;
        if (boost::regex_match(pathName, matched_substrings, globalCache->hltVersionPattern_) && matched_substrings.size() > 1) {
            // need matched_substrings[1] because matched_substrings[0] is always the entire string
            runCache->triggerPathsUnversionedNames_[iPath] = matched_substrings[1];
            ///std::cout << " -> unversioned name: " << matched_substrings[1] << std::endl;
        }

        for (size_t iRequestedPath = 0; iRequestedPath < globalCache->hltPaths_.size(); ++iRequestedPath) {
            ///std::cout << "[Check iRequestedPath " << iRequestedPath << "] " << globalCache->hltPaths_[iRequestedPath] << std::endl;
            if (runCache->triggerPathsUnversionedNames_[iPath] == globalCache->hltPaths_[iRequestedPath]) {
                ///std::cout << " -> matched! " << iPath << " = " << iRequestedPath << std::endl;
                runCache->triggerPathsIndicesInConfig_[iPath] = iRequestedPath;
                break;
            }
        }
    }

    // -- retrieve names of MET filters in skim

    // retrieve process name from Karma Run InputTag
    std::string processName = globalCache->pSet_.getParameter<edm::InputTag>("karmaEventSrc").process();

    // if no specific process name, use previous process name
    if (processName.empty()) {
        const auto& processHistory = run.processHistory();
        processName = processHistory.at(processHistory.size() - 2).processName();
    }

    // retrieve the MET filter names from skim
    try {
        const auto& metFiltersInSkim = karma::util::getModuleParameterFromHistory<std::vector<std::string>>(
            run, processName, "karmaEvents", "metFilterNames");

        for (const auto& requestedMETFilterName : globalCache->metFilterNames_) {
            const auto& it = std::find(
                metFiltersInSkim.begin(),
                metFiltersInSkim.end(),
                requestedMETFilterName
            );

            // throw if MET filter not found for a requested label!
            if (it == metFiltersInSkim.end()) {
                edm::Exception exception(edm::errors::NotFound);
                exception
                    << "Cannot find MET filter for name '" << requestedMETFilterName << "' in skim!";
                throw exception;
            }

            // retrieve index
            int index = std::distance(metFiltersInSkim.begin(), it);
            runCache->metFilterIndicesInSkim_.emplace_back(index);
        }
    }
    catch (edm::Exception& e) {
        std::cout << "[WARNING] Could not retrieve MET filters from skim!" << std::endl;
    }

    return runCache;
}

// -- member functions

void dijet::NtupleProducer::produce(edm::Event& event, const edm::EventSetup& setup) {
    std::unique_ptr<dijet::NtupleEntry> outputNtupleEntry(new dijet::NtupleEntry());

    // -- get object collections for event

    // run data
    karma::util::getByTokenOrThrow(event.getRun(), this->karmaRunToken, this->karmaRunHandle);
    // pileup density
    karma::util::getByTokenOrThrow(event, this->karmaEventToken, this->karmaEventHandle);
    // jet collection
    karma::util::getByTokenOrThrow(event, this->karmaJetCollectionToken, this->karmaJetCollectionHandle);
    // MET collection
    karma::util::getByTokenOrThrow(event, this->karmaMETCollectionToken, this->karmaMETCollectionHandle);
    // jet trigger objects map
    karma::util::getByTokenOrThrow(event, this->karmaJetTriggerObjectsMapToken, this->karmaJetTriggerObjectsMapHandle);
    if (!m_isData) {
        // QCD generator information
        karma::util::getByTokenOrThrow(event, this->karmaGeneratorQCDInfoToken, this->karmaGeneratorQCDInfoHandle);
        // genParticle collection
        karma::util::getByTokenOrThrow(event, this->karmaGenParticleCollectionToken, this->karmaGenParticleCollectionHandle);
        // gen jet collection
        karma::util::getByTokenOrThrow(event, this->karmaGenJetCollectionToken, this->karmaGenJetCollectionHandle);
        // jet genJet map
        karma::util::getByTokenOrThrow(event, this->karmaJetGenJetMapToken, this->karmaJetGenJetMapHandle);
    }
    else {
        // prefiring weights
        karma::util::getByTokenOrThrow(event, this->karmaPrefiringWeightToken, this->karmaPrefiringWeightHandle);
        karma::util::getByTokenOrThrow(event, this->karmaPrefiringWeightUpToken, this->karmaPrefiringWeightUpHandle);
        karma::util::getByTokenOrThrow(event, this->karmaPrefiringWeightDownToken, this->karmaPrefiringWeightDownHandle);
    }

    assert(this->karmaMETCollectionHandle->size() == 1);  // only allow MET collections containing a single MET object

    // get random number generator engine
    edm::Service<edm::RandomNumberGenerator> rng;
    CLHEP::HepRandomEngine& rngEngine = rng->getEngine(event.streamID());

    // -- populate outputs

    // event metadata
    outputNtupleEntry->run = event.id().run();
    outputNtupleEntry->lumi = event.id().luminosityBlock();
    outputNtupleEntry->event = event.id().event();
    outputNtupleEntry->bx = event.bunchCrossing();
    outputNtupleEntry->randomUniform = CLHEP::RandFlat::shoot(&rngEngine, 0, 1);

    // -- copy event content to ntuple
    outputNtupleEntry->rho     = this->karmaEventHandle->rho;
    if (m_isData) {
        // nPUMean estimate in data, taken from external file
        if (m_npuMeanProvider) {
            outputNtupleEntry->nPUMean = m_npuMeanProvider->getNPUMean(
                outputNtupleEntry->run,
                outputNtupleEntry->lumi
            );
        }
    }
    else {
        outputNtupleEntry->nPU     = this->karmaEventHandle->nPU;
        outputNtupleEntry->nPUMean = this->karmaEventHandle->nPUTrue;
        if (m_puWeightProvider) {
            outputNtupleEntry->pileupWeight = this->m_puWeightProvider->getPileupWeight(outputNtupleEntry->nPUMean);
        }
        /*if (m_puWeightProviderAlt) {
            outputNtupleEntry->pileupWeightAlt = this->m_puWeightProviderAlt->getPileupWeight(outputNtupleEntry->nPUMean);
        }*/
    }

    // write information related to primary vertices

    // TEMPORARY: make PV collection will be standard in newer skims
    // obtain primary vertex collection (if available)
    bool hasPVCollection = event.getByToken(this->karmaVertexCollectionToken, this->karmaVertexCollectionHandle);
    if (hasPVCollection) {
        // fill from PV collection
        outputNtupleEntry->npv     = this->karmaVertexCollectionHandle->size();
        outputNtupleEntry->npvGood = std::count_if(
            this->karmaVertexCollectionHandle->begin(),
            this->karmaVertexCollectionHandle->end(),
            [](const karma::Vertex& vtx) {return vtx.isGoodOfflineVertex();}
        );
    }
    else {
        // fill from karma::Event
        outputNtupleEntry->npv     = this->karmaEventHandle->npv;
        outputNtupleEntry->npvGood = this->karmaEventHandle->npvGood;
    }

    // weights
    outputNtupleEntry->weightForStitching = m_weightForStitching;

    // prefiring weights
    if (m_isData) {
        outputNtupleEntry->prefiringWeight = *(this->karmaPrefiringWeightHandle);
        outputNtupleEntry->prefiringWeightUp = *(this->karmaPrefiringWeightUpHandle);
        outputNtupleEntry->prefiringWeightDown = *(this->karmaPrefiringWeightDownHandle);
    }

    // -- trigger results and prescales
    // number of triggers requested in analysis config
    //outputNtupleEntry->nTriggers = globalCache()->hltPaths_.size();
    if (globalCache()->doPrescales_) {
        outputNtupleEntry->triggerPrescales.resize(globalCache()->hltPaths_.size(), 0);
    }
    // store trigger results as `dijet::TriggerBits`
    dijet::TriggerBits bitsetHLTBits;
    int indexHighestFiredTriggerPath = -1;  // keep track (mainly for simulated PU weight application)
    // go through all triggers in skim
    for (size_t iBit = 0; iBit < this->karmaEventHandle->hltBits.size(); ++iBit) {
        // the index is < `globalCache()->hltPaths_.size()` by definition
        const int idxInConfig = runCache()->triggerPathsIndicesInConfig_[iBit];

        if (idxInConfig >= 0) {
            // store prescale value
            if (globalCache()->doPrescales_) {
                outputNtupleEntry->triggerPrescales[idxInConfig] = this->karmaEventHandle->triggerPathHLTPrescales[iBit] *
                                                                   this->karmaEventHandle->triggerPathL1Prescales[iBit];
            }
            // if trigger fired
            if (this->karmaEventHandle->hltBits[iBit]) {
                // set bit
                bitsetHLTBits[idxInConfig] = true;
                indexHighestFiredTriggerPath = idxInConfig;
            }
        }
    }

    // -- MET filter bits
    dijet::TriggerBits bitsetMETFilterBits;
    for (size_t iBit = 0; iBit < runCache()->metFilterIndicesInSkim_.size(); ++iBit) {
        bitsetMETFilterBits[iBit] = false;
        if (this->karmaEventHandle->metFilterBits.at(runCache()->metFilterIndicesInSkim_[iBit])) {
            bitsetMETFilterBits[iBit] = true;
        }
    }

    // encode bitsets as 'unsigned long'
    outputNtupleEntry->hltBits = bitsetHLTBits.to_ulong();
    outputNtupleEntry->metFilterBits = bitsetMETFilterBits.to_ulong();

    // -- generator data (MC-only)
    if (!m_isData) {
        outputNtupleEntry->generatorWeight = this->karmaGeneratorQCDInfoHandle->weight;
        //outputNtupleEntry->generatorWeightProduct = this->karmaGeneratorQCDInfoHandle->weightProduct;
        if (this->karmaGeneratorQCDInfoHandle->binningValues.size() > 0)
            outputNtupleEntry->binningValue = this->karmaGeneratorQCDInfoHandle->binningValues[0];
        //if (this->karmaGeneratorQCDInfoHandle->binningValues.size() > 1)
        //    outputNtupleEntry->binningValue2 = this->karmaGeneratorQCDInfoHandle->binningValues[1];

        /* -- not needed for now
        outputNtupleEntry->incomingParton1Flavor = this->karmaGeneratorQCDInfoHandle->parton1PdgId;
        outputNtupleEntry->incomingParton2Flavor = this->karmaGeneratorQCDInfoHandle->parton2PdgId;
        outputNtupleEntry->incomingParton1x = this->karmaGeneratorQCDInfoHandle->parton1x;
        outputNtupleEntry->incomingParton2x = this->karmaGeneratorQCDInfoHandle->parton2x;
        outputNtupleEntry->scalePDF = this->karmaGeneratorQCDInfoHandle->scalePDF;
        outputNtupleEntry->alphaQCD = this->karmaGeneratorQCDInfoHandle->alphaQCD;
        */

        // gen jets
        const auto& genJets = this->karmaGenJetCollectionHandle;  // convenience
        if (genJets->size() > 0) {
            const karma::LV* genJet1 = &genJets->at(0);

            outputNtupleEntry->genJet1Pt = genJet1->p4.pt();
            outputNtupleEntry->genJet1Phi = genJet1->p4.phi();
            outputNtupleEntry->genJet1Eta = genJet1->p4.eta();
            outputNtupleEntry->genJet1Y = genJet1->p4.Rapidity();

            if (genJets->size() > 1) {
                const karma::LV* genJet2 = &genJets->at(1);

                outputNtupleEntry->genJet2Pt = genJet2->p4.pt();
                outputNtupleEntry->genJet2Phi = genJet2->p4.phi();
                outputNtupleEntry->genJet2Eta = genJet2->p4.eta();
                outputNtupleEntry->genJet2Y = genJet2->p4.Rapidity();

                // leading gen jet pair
                outputNtupleEntry->genJet12Mass = (genJet1->p4 + genJet2->p4).M();
                outputNtupleEntry->genJet12PtAve = 0.5 * (genJet1->p4.pt() + genJet2->p4.pt());
                outputNtupleEntry->genJet12YStar = 0.5 * (genJet1->p4.Rapidity() - genJet2->p4.Rapidity());
                outputNtupleEntry->genJet12YBoost = 0.5 * (genJet1->p4.Rapidity() + genJet2->p4.Rapidity());

                double absYStar = std::abs(outputNtupleEntry->genJet12YStar);
                double absYBoost = std::abs(outputNtupleEntry->genJet12YBoost);
                if (m_flexGridBinProviderDijetPtAve) {
                    outputNtupleEntry->binIndexGenJet12PtAve = m_flexGridBinProviderDijetPtAve->getFlexGridBin({
                        absYStar, absYBoost, outputNtupleEntry->genJet12PtAve
                    });
                }
                if (m_flexGridBinProviderDijetMass) {
                    outputNtupleEntry->binIndexGenJet12Mass = m_flexGridBinProviderDijetMass->getFlexGridBin({
                        absYStar, absYBoost, outputNtupleEntry->genJet12Mass
                    });
                }
            }
        }
    }

    const auto& jets = this->karmaJetCollectionHandle;  // convenience
    // leading jet kinematics
    if (jets->size() > 0) {
        const karma::Jet* jet1 = &jets->at(0);
        // write out jetID (1 if pass, 0 if fail, -1 if not requested/not available)
        if (globalCache()->jetIDProvider_)
            outputNtupleEntry->jet1id = (globalCache()->jetIDProvider_->getJetID(*jet1));

        outputNtupleEntry->jet1pt = jet1->p4.pt();
        outputNtupleEntry->jet1phi = jet1->p4.phi();
        outputNtupleEntry->jet1eta = jet1->p4.eta();
        outputNtupleEntry->jet1y = jet1->p4.Rapidity();

        /* -- not needed for now
        // PF energy fractions (jet 1)
        outputNtupleEntry->jet1NeutralHadronFraction = jet1->neutralHadronFraction;
        outputNtupleEntry->jet1ChargedHadronFraction = jet1->chargedHadronFraction;
        outputNtupleEntry->jet1MuonFraction = jet1->muonFraction;
        outputNtupleEntry->jet1PhotonFraction = jet1->photonFraction;
        outputNtupleEntry->jet1ElectronFraction = jet1->electronFraction;
        outputNtupleEntry->jet1HFHadronFraction = jet1->hfHadronFraction;
        outputNtupleEntry->jet1HFEMFraction = jet1->hfEMFraction;
        */

        // matched genJet (MC-only)
        const karma::LV* jet1MatchedGenJet = nullptr;
        if (!m_isData) {
            jet1MatchedGenJet = getMatchedGenJet(0);
            if (jet1MatchedGenJet) {
                outputNtupleEntry->jet1MatchedGenJetPt = jet1MatchedGenJet->p4.pt();
                outputNtupleEntry->jet1MatchedGenJetPhi = jet1MatchedGenJet->p4.phi();
                outputNtupleEntry->jet1MatchedGenJetEta = jet1MatchedGenJet->p4.eta();
                outputNtupleEntry->jet1MatchedGenJetY = jet1MatchedGenJet->p4.Rapidity();
            }
            /* -- not needed for now
            // flavor information (jet 1)
            outputNtupleEntry->jet1PartonFlavor = jet1->partonFlavor;
            outputNtupleEntry->jet1HadronFlavor = jet1->hadronFlavor;
            */
        }

        // trigger bitsets
        dijet::TriggerBitsets jet1TriggerBitsets = getTriggerBitsetsForJet(0);
        outputNtupleEntry->hltJet1Match = ((jet1TriggerBitsets.hltMatches | globalCache()->hltZeroThresholdMask_) & (jet1TriggerBitsets.l1Matches | globalCache()->l1ZeroThresholdMask_)).to_ulong();
        outputNtupleEntry->hltJet1PtPassThresholdsL1 = jet1TriggerBitsets.l1PassThresholds.to_ulong();
        outputNtupleEntry->hltJet1PtPassThresholdsHLT = jet1TriggerBitsets.hltPassThresholds.to_ulong();

        // second-leading jet kinematics
        if (jets->size() > 1) {
            const karma::Jet* jet2 = &jets->at(1);
            // write out jetID (1 if pass, 0 if fail, -1 if not requested/not available)
            if (globalCache()->jetIDProvider_)
                outputNtupleEntry->jet2id = (globalCache()->jetIDProvider_->getJetID(*jet2));

            outputNtupleEntry->jet2pt = jet2->p4.pt();
            outputNtupleEntry->jet2phi = jet2->p4.phi();
            outputNtupleEntry->jet2eta = jet2->p4.eta();
            outputNtupleEntry->jet2y = jet2->p4.Rapidity();

            /* -- not needed for now
            // PF energy fractions (jet 2)
            outputNtupleEntry->jet2NeutralHadronFraction = jet2->neutralHadronFraction;
            outputNtupleEntry->jet2ChargedHadronFraction = jet2->chargedHadronFraction;
            outputNtupleEntry->jet2MuonFraction = jet2->muonFraction;
            outputNtupleEntry->jet2PhotonFraction = jet2->photonFraction;
            outputNtupleEntry->jet2ElectronFraction = jet2->electronFraction;
            outputNtupleEntry->jet2HFHadronFraction = jet2->hfHadronFraction;
            outputNtupleEntry->jet2HFEMFraction = jet2->hfEMFraction;
            */

            // matched genJet (MC-only)
            const karma::LV* jet2MatchedGenJet = nullptr;
            if (!m_isData) {
                jet2MatchedGenJet = getMatchedGenJet(1);
                if (jet2MatchedGenJet) {
                    outputNtupleEntry->jet2MatchedGenJetPt = jet2MatchedGenJet->p4.pt();
                    outputNtupleEntry->jet2MatchedGenJetPhi = jet2MatchedGenJet->p4.phi();
                    outputNtupleEntry->jet2MatchedGenJetEta = jet2MatchedGenJet->p4.eta();
                    outputNtupleEntry->jet2MatchedGenJetY = jet2MatchedGenJet->p4.Rapidity();
                }
                /* -- not needed for now
                // flavor information (jet 2)
                outputNtupleEntry->jet2PartonFlavor = jet2->partonFlavor;
                outputNtupleEntry->jet2HadronFlavor = jet2->hadronFlavor;
                */
            }

            // trigger bitsets
            dijet::TriggerBitsets jet2TriggerBitsets = getTriggerBitsetsForJet(1);
            outputNtupleEntry->hltJet2Match = ((jet2TriggerBitsets.hltMatches | globalCache()->hltZeroThresholdMask_) & (jet2TriggerBitsets.l1Matches | globalCache()->l1ZeroThresholdMask_)).to_ulong();
            outputNtupleEntry->hltJet2PtPassThresholdsL1 = jet2TriggerBitsets.l1PassThresholds.to_ulong();
            outputNtupleEntry->hltJet2PtPassThresholdsHLT = jet2TriggerBitsets.hltPassThresholds.to_ulong();

            // leading jet pair kinematics
            outputNtupleEntry->jet12mass = (jet1->p4 + jet2->p4).M();
            outputNtupleEntry->jet12ptave = 0.5 * (jet1->p4.pt() + jet2->p4.pt());
            outputNtupleEntry->jet12ystar = 0.5 * (jet1->p4.Rapidity() - jet2->p4.Rapidity());
            outputNtupleEntry->jet12yboost = 0.5 * (jet1->p4.Rapidity() + jet2->p4.Rapidity());

            double absYStar = std::abs(outputNtupleEntry->jet12ystar);
            double absYBoost = std::abs(outputNtupleEntry->jet12yboost);
            if (m_flexGridBinProviderDijetPtAve) {
                outputNtupleEntry->binIndexJet12PtAve = m_flexGridBinProviderDijetPtAve->getFlexGridBin({
                    absYStar, absYBoost, outputNtupleEntry->jet12ptave
                });
                try {
                    outputNtupleEntry->indexActiveTriggerPathJet12PtAve = m_flexGridBinProviderDijetPtAve->getFlexGridBinMetadata("DiPFJetAveTriggers.activeTriggerPathIndex", {
                        absYStar, absYBoost, outputNtupleEntry->jet12ptave
                    }).as<int>();
                    //outputNtupleEntry->prescaleActiveTriggerPathJet12PtAve = this->karmaEventHandle->triggerPathHLTPrescales[outputNtupleEntry->indexActiveTriggerPathJet12PtAve] *
                    //                                                         this->karmaEventHandle->triggerPathL1Prescales[outputNtupleEntry->indexActiveTriggerPathJet12PtAve];
                }
                catch (const std::out_of_range& err) {
                    // do nothing (leave at default value)
                }
            }
            if (m_flexGridBinProviderDijetMass) {
                outputNtupleEntry->binIndexJet12Mass = m_flexGridBinProviderDijetMass->getFlexGridBin({
                    absYStar, absYBoost, outputNtupleEntry->jet12mass
                });
                try {
                    outputNtupleEntry->indexActiveTriggerPathJet12Mass = m_flexGridBinProviderDijetMass->getFlexGridBinMetadata("DiPFJetAveTriggers.activeTriggerPathIndex", {
                        absYStar, absYBoost, outputNtupleEntry->jet12mass
                    }).as<int>();
                    //outputNtupleEntry->prescaleActiveTriggerPathJet12Mass = this->karmaEventHandle->triggerPathHLTPrescales[outputNtupleEntry->indexActiveTriggerPathJet12Mass] *
                    //                                                        this->karmaEventHandle->triggerPathL1Prescales[outputNtupleEntry->indexActiveTriggerPathJet12Mass];
                }
                catch (const std::out_of_range& err) {
                    // do nothing (leave at default value)
                }
            }

            // leading jet pair bitsets
            dijet::TriggerBitsets jet12PairTriggerBitsets = getTriggerBitsetsForLeadingJetPair();
            outputNtupleEntry->hltJet12Match = ((jet12PairTriggerBitsets.hltMatches | globalCache()->hltZeroThresholdMask_) & (jet12PairTriggerBitsets.l1Matches | globalCache()->l1ZeroThresholdMask_)).to_ulong();
            outputNtupleEntry->hltJet12PtAvePassThresholdsL1 = jet12PairTriggerBitsets.l1PassThresholds.to_ulong();
            outputNtupleEntry->hltJet12PtAvePassThresholdsHLT = jet12PairTriggerBitsets.hltPassThresholds.to_ulong();

            // matched genJet pair kinematics (MC-only)
            if (jet1MatchedGenJet && jet2MatchedGenJet) {
                outputNtupleEntry->jet12MatchedGenJetPairMass = (jet1MatchedGenJet->p4 + jet2MatchedGenJet->p4).M();
                outputNtupleEntry->jet12MatchedGenJetPairPtAve = 0.5 * (jet1MatchedGenJet->p4.pt() + jet2MatchedGenJet->p4.pt());
                outputNtupleEntry->jet12MatchedGenJetPairYStar = 0.5 * (jet1MatchedGenJet->p4.Rapidity() - jet2MatchedGenJet->p4.Rapidity());
                outputNtupleEntry->jet12MatchedGenJetPairYBoost = 0.5 * (jet1MatchedGenJet->p4.Rapidity() + jet2MatchedGenJet->p4.Rapidity());

                double absYStarGenMatched = std::abs(outputNtupleEntry->jet12MatchedGenJetPairYStar);
                double absYBoostGenMatched = std::abs(outputNtupleEntry->jet12MatchedGenJetPairYBoost);
                if (m_flexGridBinProviderDijetPtAve) {
                    outputNtupleEntry->binIndexMatchedGenJet12PtAve = m_flexGridBinProviderDijetPtAve->getFlexGridBin({
                        absYStarGenMatched, absYBoostGenMatched, outputNtupleEntry->jet12MatchedGenJetPairPtAve
                    });
                }
                if (m_flexGridBinProviderDijetMass) {
                    outputNtupleEntry->binIndexMatchedGenJet12Mass = m_flexGridBinProviderDijetMass->getFlexGridBin({
                        absYStarGenMatched, absYBoostGenMatched, outputNtupleEntry->jet12MatchedGenJetPairMass
                    });
                }
            }
        }
    }

    outputNtupleEntry->met = this->karmaMETCollectionHandle->at(0).p4.Pt();
    outputNtupleEntry->sumEt = this->karmaMETCollectionHandle->at(0).sumEt;
    outputNtupleEntry->metRaw = this->karmaMETCollectionHandle->at(0).uncorP4.Pt();
    outputNtupleEntry->sumEtRaw = this->karmaMETCollectionHandle->at(0).uncorSumEt;

    // determine PU weight as a function of the active HLT path
    if (!m_isData) {
        // determine PU weight by ptave
        outputNtupleEntry->pileupWeightActiveHLTByJet12PtAve = -1.0;
        if (outputNtupleEntry->indexActiveTriggerPathJet12PtAve >= 0) {
            auto* puWeightByHLTProvider = m_puWeightProvidersByHLT.at(outputNtupleEntry->indexActiveTriggerPathJet12PtAve);
            if (puWeightByHLTProvider) {
                outputNtupleEntry->pileupWeightActiveHLTByJet12PtAve = puWeightByHLTProvider->getPileupWeight(outputNtupleEntry->nPUMean);
            }
        }

        // determine PU weight by mass
        outputNtupleEntry->pileupWeightActiveHLTByJet12Mass = -1.0;
        if (outputNtupleEntry->indexActiveTriggerPathJet12Mass >= 0) {
            auto* puWeightByHLTProvider = m_puWeightProvidersByHLT.at(outputNtupleEntry->indexActiveTriggerPathJet12Mass);
            if (puWeightByHLTProvider) {
                outputNtupleEntry->pileupWeightActiveHLTByJet12Mass = puWeightByHLTProvider->getPileupWeight(outputNtupleEntry->nPUMean);
            }
        }

        // determine PU weight by simulated trigger
        outputNtupleEntry->pileupWeightSimulatedHLT = -1.0;
        if (indexHighestFiredTriggerPath >= 0) {
            auto* puWeightByHLTProvider = m_puWeightProvidersByHLT.at(indexHighestFiredTriggerPath);
            if (puWeightByHLTProvider) {
                outputNtupleEntry->pileupWeightSimulatedHLT = puWeightByHLTProvider->getPileupWeight(outputNtupleEntry->nPUMean);
            }
        }
    }

    // move outputs to event tree
    event.put(std::move(outputNtupleEntry));
}


void dijet::NtupleProducer::fillDescriptions(edm::ConfigurationDescriptions& descriptions) {
    // The following says we do not know what parameters are allowed so do no validation
    // Please change this to state exactly what you do use, even if it is no parameters
    edm::ParameterSetDescription desc;
    desc.setUnknown();
    descriptions.addDefault(desc);
}

/**
 * Helper function to determine which HLT path (if any) can be assigned
 * to a reconstructed jet with a particular index.
 */
dijet::HLTAssignment dijet::NtupleProducer::getHLTAssignment(unsigned int jetIndex) {

    dijet::HLTAssignment hltAssignment;

    // -- obtain the collection of trigger objects matched to jet with index `jetIndex`
    const auto& jetMatchedTriggerObjects = this->karmaJetTriggerObjectsMapHandle->find(
        edm::Ref<karma::JetCollection>(this->karmaJetCollectionHandle, jetIndex)
    );

    // if there is at least one trigger object match for the jet
    if (jetMatchedTriggerObjects != this->karmaJetTriggerObjectsMapHandle->end()) {

        std::set<int> seenPathIndices;
        std::set<double> seenObjectPts;
        unsigned int numValidMatchedTriggerObjects = 0;
        unsigned int numUniqueMatchedTriggerObjects = 0;

        // loop over all trigger objects matched to the jet
        for (const auto& jetMatchedTriggerObject : jetMatchedTriggerObjects->val) {

            // ignore L1 objects
            if (!jetMatchedTriggerObject->isHLT()) continue;

            // ignore objects that aren't assigned to a trigger path
            if (!jetMatchedTriggerObject->assignedPathIndices.size()) continue;

            // accumulate all path indices encountered
            seenPathIndices.insert(
                jetMatchedTriggerObject->assignedPathIndices.begin(),
                jetMatchedTriggerObject->assignedPathIndices.end()
            );

            // use insertion function to count "pT-unique" trigger objects
            const auto insertStatus = seenObjectPts.insert(jetMatchedTriggerObject->p4.pt());
            if (insertStatus.second) ++numUniqueMatchedTriggerObjects;

            // keep track of number of matched trigger objects
            ++numValidMatchedTriggerObjects;
        }

        // -- populate return struct
        if (numValidMatchedTriggerObjects) {
            hltAssignment.numMatchedTriggerObjects = numValidMatchedTriggerObjects;
            hltAssignment.numUniqueMatchedTriggerObjects = numUniqueMatchedTriggerObjects;

            // assign highest-threshold trigger path (assume ordered)
            hltAssignment.assignedPathIndex = *std::max_element(
                seenPathIndices.begin(),
                seenPathIndices.end()
            );

            // assign highest pt
            hltAssignment.assignedObjectPt = *std::max_element(
                seenObjectPts.begin(),
                seenObjectPts.end()
            );
        }
    }

    // if no matches, the default struct is returned
    return hltAssignment;
}

/**
 * Helper function to determine which HLT path (if any) can be assigned
 * to a reconstructed jet with a particular index.
 */
const karma::LV* dijet::NtupleProducer::getMatchedGenJet(unsigned int jetIndex) {

    // -- obtain the collection of trigger objects matched to jet with index `jetIndex`
    const auto& jetMatchedJenJet = this->karmaJetGenJetMapHandle->find(
        edm::Ref<karma::JetCollection>(this->karmaJetCollectionHandle, jetIndex)
    );

    // if there is a gen jet match, return it
    if (jetMatchedJenJet != this->karmaJetGenJetMapHandle->end()) {
        return &(*jetMatchedJenJet->val);
    }

    // if no match, a nullptr is returned
    return nullptr;
}

/**
 * Helper function to determine if a jet has L1 and/or HLT matches, and to check if those matches pass the configured thresholds.
 * This is typically needed for measuring the trigger efficiency.
 */
dijet::TriggerBitsets dijet::NtupleProducer::getTriggerBitsetsForJet(unsigned int jetIndex) {

    dijet::TriggerBitsets triggerBitsets;
    /* explanation of the bitsets in `triggerBitsets`:
     * Bit                                              will be true iff
     * ---                                              -----------------
     * triggerBitsets.hltMatches[hltPathIndex]          jet with index `jetIndex` has a matched HLT object assigned to the HLT path with index `hltPathIndex`
     * triggerBitsets.l1Matches[hltPathIndex]           jet with index `jetIndex` has a matched L1  object assigned to the HLT path with index `hltPathIndex`
     * triggerBitsets.hltPassThresholds[hltPathIndex]   there exists an HLT object matched to the jet with index `jetIndex` whose pT is above the HLT threshold configured for the HLT path with index `hltPathIndex`
     * triggerBitsets.l1PassThresholds[hltPathIndex]    there exists an L1  object matched to the jet with index `jetIndex` whose pT is above the L1  threshold configured for the HLT path with index `hltPathIndex`
     * */

    // -- obtain the collection of trigger objects matched to jet with index `jetIndex`
    const auto& jetMatchedTriggerObjects = this->karmaJetTriggerObjectsMapHandle->find(
        edm::Ref<karma::JetCollection>(this->karmaJetCollectionHandle, jetIndex)
    );

    // if there is at least one trigger object match for the jet
    if (jetMatchedTriggerObjects != this->karmaJetTriggerObjectsMapHandle->end()) {

        // loop over all trigger objects matched to the jet
        for (const auto& jetMatchedTriggerObject : jetMatchedTriggerObjects->val) {

            // set L1 and HLT matching trigger bits if jet is assigned the corresponding HLT path
            for (const auto& assignedPathIdx : jetMatchedTriggerObject->assignedPathIndices) {
                // get the index of trigger in analysis config
                const int idxInConfig = runCache()->triggerPathsIndicesInConfig_[assignedPathIdx];

                // skip unrequested trigger paths
                if (idxInConfig < 0)
                    continue;

                if (jetMatchedTriggerObject->isHLT()) {
                    // HLT trigger object
                    triggerBitsets.hltMatches[idxInConfig] = true;
                }
                else {
                    // L1 trigger object
                    triggerBitsets.l1Matches[idxInConfig] = true;
                }
            }

            // set L1 and HLT emulation trigger bits if jet passes preset thresholds
            for (size_t idxInConfig = 0; idxInConfig < globalCache()->hltPaths_.size(); ++idxInConfig) {
                if (jetMatchedTriggerObject->isHLT() && (jetMatchedTriggerObject->p4.Pt() >= globalCache()->hltThresholds_[idxInConfig])) {
                    triggerBitsets.hltPassThresholds[idxInConfig] = true;
                }
                else if (!jetMatchedTriggerObject->isHLT() && (jetMatchedTriggerObject->p4.Pt() >= globalCache()->l1Thresholds_[idxInConfig])) {
                    triggerBitsets.l1PassThresholds[idxInConfig] = true;
                }
            }
        }
    }
    else {
        ///std::cout << "Event has NO matches for jet " << jetIndex << std::endl;
    }

    return triggerBitsets;
}


/**
 * Helper function to determine if both leading jets have been matched to an HLT trigger path,
 * and to check if those matches pass the configured thresholds. This is typically needed for
 * measuring the trigger efficiency of dijet triggers.
 */
dijet::TriggerBitsets dijet::NtupleProducer::getTriggerBitsetsForLeadingJetPair() {

    dijet::TriggerBitsets triggerBitsets;
    /* explanation of the bitsets in `triggerBitsets`:
     * Bit                                              will be true iff
     * ---                                              -----------------
     * triggerBitsets.hltMatches[hltPathIndex]          jet with indices 0 and 1 each have a matched HLT object assigned to the HLT path with index `hltPathIndex`
     * triggerBitsets.l1Matches[hltPathIndex]           jet with indices 0 and 1 each have a matched L1  object assigned to the HLT path with index `hltPathIndex`
     * triggerBitsets.hltPassThresholds[hltPathIndex]   there exists a pair of HLT objects matched to the leading jet pair whose average pT is above the HLT threshold configured for the HLT path with index `hltPathIndex`
     * triggerBitsets.l1PassThresholds[hltPathIndex]    there exists a pair of L1  objects matched to the leading jet pair whose **individual** pTs are both above the L1 threshold configured for the HLT path with index `hltPathIndex`
     *
     * Note the difference between 'hltPassThresholds' and 'l1PassThresholds'!
     * */

    // return immediately if event has less than 2 jets
    if (this->karmaJetCollectionHandle->size() < 2)
        return triggerBitsets;

    // -- obtain the collection of trigger objects matched to leading jet
    const auto& jet1MatchedTriggerObjects = this->karmaJetTriggerObjectsMapHandle->find(
        edm::Ref<karma::JetCollection>(this->karmaJetCollectionHandle, 0)
    );
    // -- obtain the collection of trigger objects matched to subleading jet
    const auto& jet2MatchedTriggerObjects = this->karmaJetTriggerObjectsMapHandle->find(
        edm::Ref<karma::JetCollection>(this->karmaJetCollectionHandle, 1)
    );

    // if there is at least one trigger object match for each jet
    if ((jet1MatchedTriggerObjects != this->karmaJetTriggerObjectsMapHandle->end()) && (jet2MatchedTriggerObjects != this->karmaJetTriggerObjectsMapHandle->end())) {

        // loop over all pairs trigger objects matched to the leading jet pair
        for (const auto& jet1MatchedTriggerObject : jet1MatchedTriggerObjects->val) {
            for (const auto& jet2MatchedTriggerObject : jet2MatchedTriggerObjects->val) {

                // skip pairs of trigger objects of "mixed" type
                if (jet1MatchedTriggerObject->isHLT() != jet2MatchedTriggerObject->isHLT())
                    continue;

                // set L1 and HLT matching trigger bits if jet pair is assigned the corresponding HLT path
                for (const auto& jet1AssignedPathIdx : jet1MatchedTriggerObject->assignedPathIndices) {
                    // get the index of trigger in analysis config
                    const int jet1AssignedPathIdxInConfig = runCache()->triggerPathsIndicesInConfig_[jet1AssignedPathIdx];

                    // skip unrequested trigger paths
                    if (jet1AssignedPathIdxInConfig < 0)
                        continue;

                    for (const auto& jet2AssignedPathIdx : jet2MatchedTriggerObject->assignedPathIndices) {
                        // get the index of trigger in analysis config
                        const int jet2AssignedPathIdxInConfig = runCache()->triggerPathsIndicesInConfig_[jet2AssignedPathIdx];

                        // skip unrequested trigger paths
                        if (jet2AssignedPathIdxInConfig < 0)
                            continue;

                        // skip if objects not matched to the same trigger path
                        if (jet1AssignedPathIdxInConfig != jet2AssignedPathIdxInConfig)
                            continue;

                        if ((jet1MatchedTriggerObject->isHLT()) && (jet2MatchedTriggerObject->isHLT())) {
                            // both matches are HLT trigger objects
                            triggerBitsets.hltMatches[jet1AssignedPathIdxInConfig] = true;
                        }
                        else if ((!jet1MatchedTriggerObject->isHLT()) && (!jet2MatchedTriggerObject->isHLT())) {
                            // both matches are L1 trigger objects
                            triggerBitsets.l1Matches[jet1AssignedPathIdxInConfig] = true;
                        }
                        // no else, since no pairs of trigger objects of "mixed" type
                    }
                }

                // set L1 and HLT emulation trigger bits if jets (jet pair) pass(es) preset thresholds
                for (size_t idxInConfig = 0; idxInConfig < globalCache()->hltPaths_.size(); ++idxInConfig) {

                    // if HLT objects
                    if (((jet1MatchedTriggerObject->isHLT()) && (jet2MatchedTriggerObject->isHLT()))) {
                        // compute average pt on HLT level
                        const double jet12HLTPtAve = 0.5 * (jet1MatchedTriggerObject->p4.Pt() + jet2MatchedTriggerObject->p4.Pt());
                        // check threshold and set HLT bit
                        if (jet12HLTPtAve >= globalCache()->hltThresholds_[idxInConfig])
                            triggerBitsets.hltPassThresholds[idxInConfig] = true;
                    }
                    // else, if L1 objects
                    else if ((!jet1MatchedTriggerObject->isHLT()) && (!jet2MatchedTriggerObject->isHLT())) {
                        // check thresholds separately for each L1 object
                        if ((jet1MatchedTriggerObject->p4.Pt() >= globalCache()->l1Thresholds_[idxInConfig]) && (jet1MatchedTriggerObject->p4.Pt() >= globalCache()->l1Thresholds_[idxInConfig])) {
                            triggerBitsets.l1PassThresholds[idxInConfig] = true;
                        }
                    }
                }
            }
        }
    }


    return triggerBitsets;
}


//define this as a plug-in
using DijetNtupleProducer = dijet::NtupleProducer;
DEFINE_FWK_MODULE(DijetNtupleProducer);
