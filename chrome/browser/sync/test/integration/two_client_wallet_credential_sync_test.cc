// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/sync/test/integration/sync_service_impl_harness.h"
#include "chrome/browser/sync/test/integration/sync_test.h"
#include "chrome/browser/sync/test/integration/wallet_helper.h"
#include "components/autofill/core/browser/data_model/payments/credit_card.h"
#include "components/autofill/core/common/autofill_payments_features.h"
#include "components/sync/base/data_type.h"
#include "components/sync/base/features.h"
#include "components/sync/engine/loopback_server/persistent_tombstone_entity.h"
#include "content/public/test/browser_test.h"

using syncer::kSyncAutofillWalletCredentialData;
using wallet_helper::CreateDefaultSyncWalletCard;
using wallet_helper::CreateDefaultSyncWalletCredential;
using wallet_helper::ExpectDefaultWalletCredentialValues;
using wallet_helper::GetServerCreditCards;
using wallet_helper::RemoveServerCardCredentialData;
using wallet_helper::SetServerCardCredentialData;
using wallet_helper::UpdateServerCardCredentialData;

namespace {

class TwoClientWalletCredentialSyncTest
    : public SyncTest,
      public testing::WithParamInterface<SyncTest::SetupSyncMode> {
 public:
  TwoClientWalletCredentialSyncTest() : SyncTest(TWO_CLIENT) {
    std::vector<base::test::FeatureRef> enabled_features = {
        kSyncAutofillWalletCredentialData,
        autofill::features::kAutofillEnableCvcStorageAndFilling};
    if (GetSetupSyncMode() == SetupSyncMode::kSyncTransportOnly) {
      enabled_features.push_back(syncer::kReplaceSyncPromosWithSignInPromos);
    }
    features_.InitWithFeatures(enabled_features, {});
  }

  TwoClientWalletCredentialSyncTest(const TwoClientWalletCredentialSyncTest&) =
      delete;
  TwoClientWalletCredentialSyncTest& operator=(
      const TwoClientWalletCredentialSyncTest&) = delete;

  ~TwoClientWalletCredentialSyncTest() override = default;

  SyncTest::SetupSyncMode GetSetupSyncMode() const override {
    return GetParam();
  }

  bool TestUsesSelfNotifications() override { return false; }

  bool SetUpSyncAndInitialize() {
    if (!SetupSync()) {
      return false;
    }

    // As this test does not use self-notifications, wait for the metadata to
    // converge with the specialized wallet checker.
    return AutofillWalletChecker(/*profile_a=*/0, /*profile_b=*/1).Wait();
  }

  void SetDefaultWalletCredentialOnFakeServer() {
    sync_pb::EntitySpecifics entity_specifics;
    *entity_specifics.mutable_autofill_wallet_credential() =
        *CreateDefaultSyncWalletCredential()
             .mutable_specifics()
             ->mutable_autofill_wallet_credential();

    GetFakeServer()->InjectEntity(
        syncer::PersistentUniqueClientEntity::CreateFromSpecificsForTesting(
            /*non_unique_name=*/"credential",
            entity_specifics.mutable_autofill_wallet_credential()
                ->instrument_id(),
            entity_specifics, /*creation_time=*/0, /*last_modified_time=*/0));
  }

  wallet_helper::StoreType GetStoreType() const {
    return GetSetupSyncMode() == SyncTest::SetupSyncMode::kSyncTransportOnly
               ? wallet_helper::StoreType::kAccountStore
               : wallet_helper::StoreType::kProfileStore;
  }

 private:
  base::test::ScopedFeatureList features_;
};

INSTANTIATE_TEST_SUITE_P(,
                       TwoClientWalletCredentialSyncTest,
                       GetSyncTestModes(),
                       testing::PrintToStringParamName());

IN_PROC_BROWSER_TEST_P(TwoClientWalletCredentialSyncTest, AddCvcToCreditCard) {
  GetFakeServer()->SetWalletData({CreateDefaultSyncWalletCard()});
  ASSERT_TRUE(SetUpSyncAndInitialize());

  // Grab the current card on the first client.
  std::vector<const autofill::CreditCard*> credit_cards =
      GetServerCreditCards(/*profile=*/0);
  ASSERT_EQ(1u, credit_cards.size());
  EXPECT_TRUE(credit_cards[0]->cvc().empty());
  autofill::CreditCard card = *credit_cards[0];

  card.set_cvc(u"123");
  SetServerCardCredentialData(/*profile=*/0, /*credit_card=*/card,
                              GetStoreType());

  // Wait for the change to propagate.
  EXPECT_TRUE(AutofillWalletChecker(0, 1).Wait());

  // Grab the server cards from both the clients. Verify that CVC is not empty
  // and has the correct value.
  for (int profile_id = 0; profile_id < 2; profile_id++) {
    credit_cards = GetServerCreditCards(/*profile=*/profile_id);
    EXPECT_EQ(1U, credit_cards.size());
    EXPECT_EQ(u"123", credit_cards[0]->cvc());
  }
}

IN_PROC_BROWSER_TEST_P(TwoClientWalletCredentialSyncTest,
                       UpdateCvcForCreditCard) {
  SetDefaultWalletCredentialOnFakeServer();
  GetFakeServer()->SetWalletData({CreateDefaultSyncWalletCard()});
  ASSERT_TRUE(SetUpSyncAndInitialize());

  // Grab the server cards from both the clients. Verify that CVC is not empty
  // and has the default value.
  std::vector<const autofill::CreditCard*> credit_cards;
  for (int profile_id = 0; profile_id < 2; profile_id++) {
    credit_cards = GetServerCreditCards(/*profile=*/profile_id);
    ASSERT_EQ(1u, credit_cards.size());
    ExpectDefaultWalletCredentialValues(*credit_cards[0]);
  }

  autofill::CreditCard card = *GetServerCreditCards(/*profile=*/0)[0];
  card.set_cvc(u"963");
  UpdateServerCardCredentialData(/*profile=*/0, /*credit_card=*/card,
                                 GetStoreType());

  // Wait for the change to propagate.
  EXPECT_TRUE(AutofillWalletChecker(/*profile_a=*/0, /*profile_b=*/1).Wait());

  // Grab the server cards from both the clients. Verify that CVC is not empty
  // and has been updated.
  for (int profile_id = 0; profile_id < 2; profile_id++) {
    credit_cards = GetServerCreditCards(/*profile=*/profile_id);
    EXPECT_EQ(1U, credit_cards.size());
    EXPECT_EQ(u"963", credit_cards[0]->cvc());
  }
}

IN_PROC_BROWSER_TEST_P(TwoClientWalletCredentialSyncTest,
                       RemoveCvcForCreditCard) {
  SetDefaultWalletCredentialOnFakeServer();
  GetFakeServer()->SetWalletData({CreateDefaultSyncWalletCard()});
  ASSERT_TRUE(SetUpSyncAndInitialize());

  // Grab the server cards from both the clients. Verify that CVC is not empty
  // and has the default value.
  std::vector<const autofill::CreditCard*> credit_cards;
  for (int profile_id = 0; profile_id < 2; profile_id++) {
    credit_cards = GetServerCreditCards(/*profile=*/profile_id);
    ASSERT_EQ(1u, credit_cards.size());
    ExpectDefaultWalletCredentialValues(*credit_cards[0]);
  }

  autofill::CreditCard card = *GetServerCreditCards(/*profile=*/0)[0];
  RemoveServerCardCredentialData(/*profile=*/0, /*credit_card=*/card,
                                 GetStoreType());

  // Wait for the change to propagate.
  EXPECT_TRUE(AutofillWalletChecker(/*profile_a=*/0, /*profile_b=*/1).Wait());

  // Grab the server cards from both the clients. Verify that CVC is empty and
  // deleted.
  for (int profile_id = 0; profile_id < 2; profile_id++) {
    credit_cards = GetServerCreditCards(/*profile=*/profile_id);
    EXPECT_EQ(1U, credit_cards.size());
    EXPECT_TRUE(credit_cards[0]->cvc().empty());
  }
}

}  // namespace
