// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/autofill/valuables_data_manager_factory.h"
#include "chrome/browser/sync/test/integration/sync_datatype_helper.h"
#include "chrome/browser/sync/test/integration/sync_service_impl_harness.h"
#include "chrome/browser/sync/test/integration/sync_test.h"
#include "components/autofill/core/browser/data_manager/valuables/valuables_data_manager.h"
#include "components/autofill/core/browser/data_manager/valuables/valuables_data_manager_test_utils.h"
#include "components/autofill/core/browser/data_model/valuables/loyalty_card.h"
#include "components/autofill/core/browser/test_utils/valuables_data_test_utils.h"
#include "components/autofill/core/browser/webdata/valuables/valuables_sync_util.h"
#include "components/autofill/core/common/autofill_features.h"
#include "components/sync/base/features.h"
#include "components/sync/protocol/entity_data.h"
#include "content/public/test/browser_test.h"
#include "testing/gmock/include/gmock/gmock.h"

using autofill::LoyaltyCard;
using autofill::ValuablesDataChangedWaiter;
using autofill::ValuablesDataManager;
using autofill::ValuablesDataManagerFactory;
using autofill::test::CreateLoyaltyCard;
using autofill::test::CreateLoyaltyCard2;
using sync_datatype_helper::test;

namespace {

sync_pb::SyncEntity LoyaltyCardToSyncEntity(const LoyaltyCard& loyalty_card) {
  sync_pb::SyncEntity entity;
  entity.set_name(std::string(loyalty_card.id()));
  entity.set_id_string(std::string(loyalty_card.id()));
  entity.set_version(0);  // Will be overridden by the fake server.
  entity.set_ctime(12345);
  entity.set_mtime(12345);
  sync_pb::AutofillValuableSpecifics* valuable_specifics =
      entity.mutable_specifics()->mutable_autofill_valuable();
  valuable_specifics->set_id(std::string(loyalty_card.id()));

  sync_pb::AutofillValuableSpecifics::LoyaltyCard* loyalty_card_specifics =
      valuable_specifics->mutable_loyalty_card();
  loyalty_card_specifics->set_merchant_name(loyalty_card.merchant_name());
  loyalty_card_specifics->set_program_name(loyalty_card.program_name());
  loyalty_card_specifics->set_program_logo(loyalty_card.program_logo().spec());
  loyalty_card_specifics->set_loyalty_card_number(
      loyalty_card.loyalty_card_number());
  for (const GURL& url : loyalty_card.merchant_domains()) {
    *loyalty_card_specifics->add_merchant_domains() = url.spec();
  }
  return entity;
}

class SingleClientValuablesSyncTest : public SyncTest {
 public:
  SingleClientValuablesSyncTest() : SyncTest(SINGLE_CLIENT) {
    feature_list_.InitWithFeatures(
        /*enabled_features=*/{autofill::features::
                                  kAutofillEnableLoyaltyCardsFilling,
                              syncer::kSyncAutofillLoyaltyCard},
        /*disabled_features=*/{});
  }
  SingleClientValuablesSyncTest(const SingleClientValuablesSyncTest&) = delete;
  SingleClientValuablesSyncTest& operator=(
      const SingleClientValuablesSyncTest&) = delete;

  ~SingleClientValuablesSyncTest() override = default;

  ValuablesDataManager* GetValuablesDataManager(int index) {
    return ValuablesDataManagerFactory::GetForProfile(
        test()->GetProfile(index));
  }

 protected:
  void WaitForNumberOfCards(size_t expected_count, ValuablesDataManager* vdm) {
    while (vdm->GetLoyaltyCards().size() != expected_count ||
           vdm->HasPendingQueries()) {
      ValuablesDataChangedWaiter(vdm).Wait();
    }
  }
  base::test::ScopedFeatureList feature_list_;
};

// Valuables data should get loaded on initial sync.
IN_PROC_BROWSER_TEST_F(SingleClientValuablesSyncTest, InitialSync) {
  const LoyaltyCard loyalty_card = CreateLoyaltyCard();
  GetFakeServer()->SetValuableData({LoyaltyCardToSyncEntity(loyalty_card)});
  ASSERT_TRUE(SetupSync());
  ValuablesDataManager* vdm = GetValuablesDataManager(0);
  ASSERT_NE(nullptr, vdm);
  // Make sure the data & metadata is in the DB.
  EXPECT_THAT(vdm->GetLoyaltyCards(), testing::ElementsAre(loyalty_card));
}

// ChromeOS does not support late signin after profile creation, so the test
// below does not apply, at least in the current form.
#if !BUILDFLAG(IS_CHROMEOS)
// Valuables data should get cleared from the database when the user signs out.
IN_PROC_BROWSER_TEST_F(SingleClientValuablesSyncTest, ClearOnSignOut) {
  const LoyaltyCard loyalty_card = CreateLoyaltyCard();
  GetFakeServer()->SetValuableData({LoyaltyCardToSyncEntity(loyalty_card)});
  ASSERT_TRUE(SetupSync());
  ValuablesDataManager* vdm = GetValuablesDataManager(0);
  ASSERT_NE(nullptr, vdm);
  // Make sure the data & metadata is in the DB.
  EXPECT_THAT(vdm->GetLoyaltyCards(), testing::ElementsAre(loyalty_card));

  // Signout, the data & metadata should be gone.
  GetClient(0)->SignOutPrimaryAccount();
  WaitForNumberOfCards(0, vdm);

  EXPECT_EQ(0uL, vdm->GetLoyaltyCards().size());
}
#endif  // !BUILDFLAG(IS_CHROMEOS)

// Valuables data should get cleared from the database when the user enters the
// sync paused state (e.g. persistent auth error).
IN_PROC_BROWSER_TEST_F(SingleClientValuablesSyncTest, ClearOnSyncPaused) {
  const LoyaltyCard loyalty_card = CreateLoyaltyCard();
  GetFakeServer()->SetValuableData({LoyaltyCardToSyncEntity(loyalty_card)});
  ASSERT_TRUE(SetupSync());
  ValuablesDataManager* vdm = GetValuablesDataManager(0);
  ASSERT_NE(nullptr, vdm);
  // Make sure the data & metadata is in the DB.
  EXPECT_THAT(vdm->GetLoyaltyCards(), testing::ElementsAre(loyalty_card));

  // Enter sync paused state, the data & metadata should be gone.
  GetClient(0)->EnterSyncPausedStateForPrimaryAccount();
  WaitForNumberOfCards(0, vdm);
  EXPECT_EQ(0uL, vdm->GetLoyaltyCards().size());

  // When exiting the sync paused state, the data should be redownloaded.
  GetClient(0)->ExitSyncPausedStateForPrimaryAccount();
  WaitForNumberOfCards(1, vdm);
  EXPECT_EQ(1uL, vdm->GetLoyaltyCards().size());
}

// Valuables are not using incremental updates. Make sure existing data gets
// replaced when synced down.
IN_PROC_BROWSER_TEST_F(SingleClientValuablesSyncTest,
                       NewSyncDataShouldReplaceExistingData) {
  const LoyaltyCard loyalty_card = CreateLoyaltyCard();
  GetFakeServer()->SetValuableData({LoyaltyCardToSyncEntity(loyalty_card)});
  ASSERT_TRUE(SetupSync());
  ValuablesDataManager* vdm = GetValuablesDataManager(0);
  ASSERT_NE(nullptr, vdm);
  EXPECT_THAT(vdm->GetLoyaltyCards(), testing::ElementsAre(loyalty_card));

  ValuablesDataChangedWaiter waiter(vdm);
  // Put some completely new data in the sync server.
  const LoyaltyCard loyalty_card2 = CreateLoyaltyCard2();
  GetFakeServer()->SetValuableData({LoyaltyCardToSyncEntity(loyalty_card2)});
  waiter.Wait();
  EXPECT_THAT(vdm->GetLoyaltyCards(), testing::ElementsAre(loyalty_card2));
}

IN_PROC_BROWSER_TEST_F(SingleClientValuablesSyncTest,
                       ClearOnDisablePaymentsSync) {
  const LoyaltyCard loyalty_card = CreateLoyaltyCard();
  GetFakeServer()->SetValuableData({LoyaltyCardToSyncEntity(loyalty_card)});
  ASSERT_TRUE(SetupSync());
  ValuablesDataManager* vdm = GetValuablesDataManager(0);
  ASSERT_NE(nullptr, vdm);
  EXPECT_THAT(vdm->GetLoyaltyCards(), testing::ElementsAre(loyalty_card));

  // Turn off payments sync, the data & metadata should be gone.
  ASSERT_TRUE(
      GetClient(0)->DisableSyncForType(syncer::UserSelectableType::kPayments));

  WaitForNumberOfCards(0, vdm);
  EXPECT_THAT(vdm->GetLoyaltyCards(), testing::IsEmpty());
}

IN_PROC_BROWSER_TEST_F(SingleClientValuablesSyncTest,
                       ClearOnDisableWalletAutofill) {
  const LoyaltyCard loyalty_card = CreateLoyaltyCard();
  GetFakeServer()->SetValuableData({LoyaltyCardToSyncEntity(loyalty_card)});
  ASSERT_TRUE(SetupSync());
  ValuablesDataManager* vdm = GetValuablesDataManager(0);
  ASSERT_NE(nullptr, vdm);
  EXPECT_THAT(vdm->GetLoyaltyCards(), testing::ElementsAre(loyalty_card));

  // Turn off the wallet autofill pref, the data & metadata should be gone as a
  // side effect of the wallet data type controller noticing.
  GetSyncService(0)->GetUserSettings()->SetSelectedTypes(
      /*sync_everything=*/false, /*types=*/{});

  WaitForNumberOfCards(0, vdm);
  EXPECT_THAT(vdm->GetLoyaltyCards(), testing::IsEmpty());
}

}  // namespace
