// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "build/build_config.h"
#include "build/chromeos_buildflags.h"
#include "chrome/browser/sync/test/integration/autofill_helper.h"
#include "chrome/browser/sync/test/integration/offer_helper.h"
#include "chrome/browser/sync/test/integration/sync_service_impl_harness.h"
#include "chrome/browser/sync/test/integration/sync_test.h"
#include "chrome/browser/sync/test/integration/wallet_helper.h"
#include "components/autofill/core/browser/autofill_test_utils.h"
#include "components/autofill/core/browser/data_model/autofill_offer_data.h"
#include "components/autofill/core/browser/personal_data_manager.h"
#include "components/autofill/core/browser/personal_data_manager_observer.h"
#include "components/sync/base/model_type.h"
#include "components/sync/driver/sync_service.h"
#include "components/sync/protocol/model_type_state.pb.h"
#include "components/sync/test/fake_server.h"
#include "content/public/test/browser_test.h"
#include "testing/gmock/include/gmock/gmock.h"

using autofill::AutofillOfferData;
using autofill::test::GetCardLinkedOfferData1;
using autofill::test::GetCardLinkedOfferData2;
using offer_helper::CreateDefaultSyncCardLinkedOffer;
using offer_helper::CreateSyncCardLinkedOffer;
using wallet_helper::GetPersonalDataManager;
using wallet_helper::GetWalletModelTypeState;

namespace {

ACTION_P(QuitMessageLoop, loop) {
  loop->Quit();
}

const syncer::SyncFirstSetupCompleteSource kSetSourceFromTest =
    syncer::SyncFirstSetupCompleteSource::BASIC_FLOW;

}  // namespace

class SingleClientOfferSyncTest : public SyncTest {
 public:
  SingleClientOfferSyncTest() : SyncTest(SINGLE_CLIENT) {}

  ~SingleClientOfferSyncTest() override = default;

  SingleClientOfferSyncTest(const SingleClientOfferSyncTest&) = delete;
  SingleClientOfferSyncTest& operator=(const SingleClientOfferSyncTest&) =
      delete;

 protected:
  void WaitForOnPersonalDataChanged(autofill::PersonalDataManager* pdm) {
    testing::NiceMock<PersonalDataLoadedObserverMock> personal_data_observer;
    pdm->AddObserver(&personal_data_observer);
    base::RunLoop run_loop;
    EXPECT_CALL(personal_data_observer, OnPersonalDataChanged())
        .WillOnce(QuitMessageLoop(&run_loop));
    run_loop.Run();
    pdm->RemoveObserver(&personal_data_observer);
  }

  void WaitForNumberOfOffers(size_t expected_count,
                             autofill::PersonalDataManager* pdm) {
    while (pdm->GetAutofillOffers().size() != expected_count ||
           pdm->HasPendingQueriesForTesting()) {
      WaitForOnPersonalDataChanged(pdm);
    }
  }

  bool TriggerGetUpdatesAndWait() {
    const base::Time now = base::Time::Now();
    // Trigger a sync and wait for the new data to arrive.
    TriggerSyncForModelTypes(
        0, syncer::ModelTypeSet(syncer::AUTOFILL_WALLET_OFFER));
    return FullUpdateTypeProgressMarkerChecker(now, GetSyncService(0),
                                               syncer::AUTOFILL_WALLET_OFFER)
        .Wait();
  }
};

// Ensures that the offer sync type is enabled by default.
IN_PROC_BROWSER_TEST_F(SingleClientOfferSyncTest, EnabledByDefault) {
  ASSERT_TRUE(SetupSync());
  ASSERT_TRUE(GetClient(0)->service()->GetActiveDataTypes().Has(
      syncer::AUTOFILL_WALLET_OFFER));
}

// Ensures that offer data should get cleared from the database when sync is
// disabled.
IN_PROC_BROWSER_TEST_F(SingleClientOfferSyncTest, ClearOnDisableSync) {
  GetFakeServer()->SetOfferData({CreateDefaultSyncCardLinkedOffer()});
  ASSERT_TRUE(SetupSync());
  autofill::PersonalDataManager* pdm = GetPersonalDataManager(0);
  ASSERT_NE(nullptr, pdm);
  // Make sure the offer data is in the DB.
  ASSERT_EQ(1uL, pdm->GetAutofillOffers().size());

  // Disable sync, the offer data should be gone.
  GetSyncService(0)->StopAndClear();
  WaitForNumberOfOffers(0, pdm);
  EXPECT_EQ(0uL, pdm->GetAutofillOffers().size());

  // Turn sync on again, the data should come back.
  GetSyncService(0)->GetUserSettings()->SetSyncRequested(true);
  // StopAndClear() also clears the "first setup complete" flag, so set it
  // again.
  GetSyncService(0)->GetUserSettings()->SetFirstSetupComplete(
      kSetSourceFromTest);
  // Wait until Sync restores the card and it arrives at PDM.
  WaitForNumberOfOffers(1, pdm);
  EXPECT_EQ(1uL, pdm->GetAutofillOffers().size());
}

// Ensures that offer data should get cleared from the database when sync is
// (temporarily) stopped, e.g. due to the Sync feature toggle in Android
// settings.
IN_PROC_BROWSER_TEST_F(SingleClientOfferSyncTest, ClearOnStopSync) {
  GetFakeServer()->SetOfferData({CreateDefaultSyncCardLinkedOffer()});
  ASSERT_TRUE(SetupSync());

  autofill::PersonalDataManager* pdm = GetPersonalDataManager(0);
  ASSERT_NE(nullptr, pdm);
  // Make sure the offer data is in the DB.
  ASSERT_EQ(1uL, pdm->GetAutofillOffers().size());

  // Stop sync, the offer data should be gone.
  GetSyncService(0)->GetUserSettings()->SetSyncRequested(false);
  WaitForNumberOfOffers(0, pdm);
  EXPECT_EQ(0uL, pdm->GetAutofillOffers().size());

  // Turn sync on again, the data should come back.
  GetSyncService(0)->GetUserSettings()->SetSyncRequested(true);
  // Wait until Sync restores the card and it arrives at PDM.
  WaitForNumberOfOffers(1, pdm);
  EXPECT_EQ(1uL, pdm->GetAutofillOffers().size());
}

// ChromeOS does not sign out, so the test below does not apply.
#if !BUILDFLAG(IS_CHROMEOS_ASH)
// Offer data should get cleared from the database when the user signs out.
IN_PROC_BROWSER_TEST_F(SingleClientOfferSyncTest, ClearOnSignOut) {
  GetFakeServer()->SetOfferData({CreateDefaultSyncCardLinkedOffer()});
  ASSERT_TRUE(SetupSync());
  autofill::PersonalDataManager* pdm = GetPersonalDataManager(0);
  ASSERT_NE(nullptr, pdm);
  // Make sure the data & metadata is in the DB.
  ASSERT_EQ(1uL, pdm->GetAutofillOffers().size());

  // Signout, the data & metadata should be gone.
  GetClient(0)->SignOutPrimaryAccount();
  WaitForNumberOfOffers(0, pdm);
  EXPECT_EQ(0uL, pdm->GetAutofillOffers().size());
}
#endif  // !BUILDFLAG(IS_CHROMEOS_ASH)

// Offer is not using incremental updates. Make sure existing data gets
// replaced when synced down.
IN_PROC_BROWSER_TEST_F(SingleClientOfferSyncTest,
                       NewSyncDataShouldReplaceExistingData) {
  AutofillOfferData offer1 = GetCardLinkedOfferData1(/*offer_id=*/999);
  GetFakeServer()->SetOfferData({CreateSyncCardLinkedOffer(offer1)});
  ASSERT_TRUE(SetupSync());

  // Make sure the data is in the DB.
  autofill::PersonalDataManager* pdm = GetPersonalDataManager(0);
  ASSERT_NE(nullptr, pdm);
  std::vector<AutofillOfferData*> offers = pdm->GetAutofillOffers();
  ASSERT_EQ(1uL, offers.size());
  EXPECT_EQ(999, offers[0]->GetOfferId());

  // Put some completely new data in the sync server.
  AutofillOfferData offer2 = GetCardLinkedOfferData2(/*offer_id=*/888);
  GetFakeServer()->SetOfferData({CreateSyncCardLinkedOffer(offer2)});
  WaitForOnPersonalDataChanged(pdm);

  // Make sure only the new data is present.
  offers = pdm->GetAutofillOffers();
  ASSERT_EQ(1uL, offers.size());
  EXPECT_EQ(888, offers[0]->GetOfferId());
}

// Offer is not using incremental updates. The server either sends a non-empty
// update with deletion gc directives and with the (possibly empty) full data
// set, or (more often) an empty update.
IN_PROC_BROWSER_TEST_F(SingleClientOfferSyncTest, EmptyUpdatesAreIgnored) {
  AutofillOfferData offer1 = GetCardLinkedOfferData1(/*offer_id=*/999);
  GetFakeServer()->SetOfferData({CreateSyncCardLinkedOffer(offer1)});
  ASSERT_TRUE(SetupSync());

  // Make sure the card is in the DB.
  autofill::PersonalDataManager* pdm = GetPersonalDataManager(0);
  ASSERT_NE(nullptr, pdm);
  std::vector<AutofillOfferData*> offers = pdm->GetAutofillOffers();
  ASSERT_EQ(1uL, offers.size());
  EXPECT_EQ(999, offers[0]->GetOfferId());

  // Trigger a sync and wait for the new data to arrive.
  sync_pb::ModelTypeState state_before =
      GetWalletModelTypeState(syncer::AUTOFILL_WALLET_OFFER, 0);
  ASSERT_TRUE(TriggerGetUpdatesAndWait());

  // Check that the new progress marker is stored for empty updates. This is a
  // regression check for crbug.com/924447.
  sync_pb::ModelTypeState state_after =
      GetWalletModelTypeState(syncer::AUTOFILL_WALLET_OFFER, 0);
  EXPECT_NE(state_before.progress_marker().token(),
            state_after.progress_marker().token());

  // Refresh the pdm to make sure we are checking its state after any potential
  // changes from sync in the DB propagate into pdm. As we don't expect anything
  // to change, we have no better specific condition to wait for.
  pdm->Refresh();
  while (pdm->HasPendingQueriesForTesting()) {
    WaitForOnPersonalDataChanged(pdm);
  }

  // Make sure the same data is present on the client.
  offers = pdm->GetAutofillOffers();
  ASSERT_EQ(1uL, offers.size());
  EXPECT_EQ(999, offers[0]->GetOfferId());
}

// If the server sends the same offers with changed data, they should change on
// the client.
IN_PROC_BROWSER_TEST_F(SingleClientOfferSyncTest, ChangedEntityGetsUpdated) {
  AutofillOfferData offer = GetCardLinkedOfferData1(/*offer_id=*/999);
  offer.SetEligibleInstrumentIdForTesting({111111});
  GetFakeServer()->SetOfferData({CreateSyncCardLinkedOffer(offer)});
  ASSERT_TRUE(SetupSync());

  // Make sure the card is in the DB.
  autofill::PersonalDataManager* pdm = GetPersonalDataManager(0);
  ASSERT_NE(nullptr, pdm);
  std::vector<AutofillOfferData*> offers = pdm->GetAutofillOffers();
  ASSERT_EQ(1uL, offers.size());
  EXPECT_EQ(999, offers[0]->GetOfferId());
  EXPECT_EQ(1U, offers[0]->GetEligibleInstrumentIds().size());

  // Update the data.
  offer.SetEligibleInstrumentIdForTesting({111111, 222222});
  GetFakeServer()->SetOfferData({CreateSyncCardLinkedOffer(offer)});
  WaitForOnPersonalDataChanged(pdm);

  // Make sure the data is present on the client.
  pdm = GetPersonalDataManager(0);
  ASSERT_NE(nullptr, pdm);
  offers = pdm->GetAutofillOffers();
  ASSERT_EQ(1uL, offers.size());
  EXPECT_EQ(999, offers[0]->GetOfferId());
  EXPECT_EQ(2U, offers[0]->GetEligibleInstrumentIds().size());
}

// Offer data should get cleared from the database when the Autofill sync type
// flag is disabled.
IN_PROC_BROWSER_TEST_F(SingleClientOfferSyncTest, ClearOnDisableWalletSync) {
  GetFakeServer()->SetOfferData({CreateDefaultSyncCardLinkedOffer()});
  ASSERT_TRUE(SetupSync());

  autofill::PersonalDataManager* pdm = GetPersonalDataManager(0);
  ASSERT_NE(nullptr, pdm);
  // Make sure the data is in the DB.
  ASSERT_EQ(1uL, pdm->GetAutofillOffers().size());

  // Turn off autofill sync, the data should be gone.
  ASSERT_TRUE(
      GetClient(0)->DisableSyncForType(syncer::UserSelectableType::kAutofill));
  WaitForNumberOfOffers(0, pdm);
  EXPECT_EQ(0uL, pdm->GetAutofillOffers().size());
}
