// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/notreached.h"
#include "base/test/test_future.h"
#include "build/build_config.h"
#include "chrome/browser/sync/test/integration/fake_server_match_status_checker.h"
#include "chrome/browser/sync/test/integration/sync_service_impl_harness.h"
#include "chrome/browser/sync/test/integration/sync_test.h"
#include "chrome/browser/sync/test/integration/wallet_helper.h"
#include "components/autofill/core/browser/data_manager/payments/payments_data_manager.h"
#include "components/autofill/core/browser/data_manager/payments/payments_data_manager_test_utils.h"
#include "components/autofill/core/browser/data_model/payments/credit_card.h"
#include "components/autofill/core/common/autofill_payments_features.h"
#include "components/sync/base/data_type.h"
#include "components/sync/base/features.h"
#include "components/sync/engine/loopback_server/persistent_tombstone_entity.h"
#include "content/public/test/browser_test.h"

namespace {

using autofill::PaymentsDataChangedWaiter;
using autofill::PaymentsDataManager;
using autofill::ServerCvc;
using syncer::kSyncAutofillWalletCredentialData;
using wallet_helper::CreateDefaultSyncWalletCard;
using wallet_helper::CreateDefaultSyncWalletCredential;
using wallet_helper::CreateSyncPaymentsCustomerData;
using wallet_helper::CreateSyncWalletCard;
using wallet_helper::CreateSyncWalletCredential;
using wallet_helper::ExpectDefaultWalletCredentialValues;
using wallet_helper::GetAccountWebDataService;
using wallet_helper::GetPaymentsDataManager;
using wallet_helper::GetProfileWebDataService;
using wallet_helper::kDefaultBillingAddressID;

// A helper class that waits for `AUTOFILL_WALLET_CREDENTIAL` to have the
// expected entries on the FakeServer.
class ServerCvcChecker : public fake_server::FakeServerMatchStatusChecker {
 public:
  explicit ServerCvcChecker(const size_t expected_count);
  ~ServerCvcChecker() override;
  ServerCvcChecker(const ServerCvcChecker&) = delete;
  ServerCvcChecker& operator=(const ServerCvcChecker&) = delete;

  // StatusChangeChecker overrides.
  bool IsExitConditionSatisfied(std::ostream* os) override;

 private:
  const size_t expected_count_;
};

ServerCvcChecker::ServerCvcChecker(const size_t expected_count)
    : expected_count_(expected_count) {}

ServerCvcChecker::~ServerCvcChecker() = default;

bool ServerCvcChecker::IsExitConditionSatisfied(std::ostream* os) {
  return fake_server()
             ->GetSyncEntitiesByDataType(syncer::AUTOFILL_WALLET_CREDENTIAL)
             .size() == expected_count_;
}

#if !BUILDFLAG(IS_CHROMEOS)
std::vector<std::unique_ptr<autofill::CreditCard>> GetServerCards(
    scoped_refptr<autofill::AutofillWebDataService> service) {
  base::test::TestFuture<WebDataServiceBase::Handle,
                         std::unique_ptr<WDTypedResult>>
      future;
  service->GetServerCreditCards(future.GetCallback());
  return static_cast<
             WDResult<std::vector<std::unique_ptr<autofill::CreditCard>>>&>(
             *future.Get<1>())
      .GetValue();
}
#endif  // !BUILDFLAG(IS_CHROMEOS)

}  // namespace

class SingleClientWalletCredentialSyncTest
    : public SyncTest,
      public testing::WithParamInterface<SyncTest::SetupSyncMode> {
 public:
  SingleClientWalletCredentialSyncTest() : SyncTest(SINGLE_CLIENT) {
    std::vector<base::test::FeatureRef> enabled_features = {
        kSyncAutofillWalletCredentialData,
        autofill::features::kAutofillEnableCvcStorageAndFilling};
    if (GetSetupSyncMode() == SetupSyncMode::kSyncTransportOnly) {
      enabled_features.push_back(syncer::kReplaceSyncPromosWithSignInPromos);
    }
    features_.InitWithFeatures(enabled_features, /*disabled_features=*/{});
  }

  ~SingleClientWalletCredentialSyncTest() override = default;

  SingleClientWalletCredentialSyncTest(
      const SingleClientWalletCredentialSyncTest&) = delete;
  SingleClientWalletCredentialSyncTest& operator=(
      const SingleClientWalletCredentialSyncTest&) = delete;

  SyncTest::SetupSyncMode GetSetupSyncMode() const override {
    return GetParam();
  }

 protected:
  void WaitForNumberOfCards(size_t expected_count, PaymentsDataManager* paydm) {
    while (paydm->GetCreditCards().size() != expected_count ||
           paydm->HasPendingPaymentQueries()) {
      PaymentsDataChangedWaiter(paydm).Wait();
    }
  }

  void WaitForNoPaymentsCustomerData(PaymentsDataManager* paydm) {
    while (paydm->GetPaymentsCustomerData() != nullptr ||
           paydm->HasPendingPaymentQueries()) {
      PaymentsDataChangedWaiter(paydm).Wait();
    }
  }

  void WaitForCvcOnCard(PaymentsDataManager* paydm) {
    while (!IsCvcAvailableOnAnyCard(paydm)) {
      PaymentsDataChangedWaiter(paydm).Wait();
    }
  }

  bool IsCvcAvailableOnAnyCard(PaymentsDataManager* paydm) {
    for (const autofill::CreditCard* credit_card : paydm->GetCreditCards()) {
      if (!credit_card->cvc().empty()) {
        return true;
      }
    }
    return false;
  }

  void SetDefaultWalletCredentialOnFakeServer() {
    sync_pb::EntitySpecifics entity_specifics =
        CreateDefaultSyncWalletCredential().specifics();

    GetFakeServer()->InjectEntity(
        syncer::PersistentUniqueClientEntity::CreateFromSpecificsForTesting(
            /*non_unique_name=*/"credential",
            entity_specifics.mutable_autofill_wallet_credential()
                ->instrument_id(),
            entity_specifics, /*creation_time=*/0, /*last_modified_time=*/0));
  }

  void SetWalletCredentialOnFakeServer(const ServerCvc& server_cvc) {
    sync_pb::EntitySpecifics entity_specifics =
        CreateSyncWalletCredential(server_cvc).specifics();

    GetFakeServer()->InjectEntity(
        syncer::PersistentUniqueClientEntity::CreateFromSpecificsForTesting(
            /*non_unique_name=*/"credential",
            entity_specifics.mutable_autofill_wallet_credential()
                ->instrument_id(),
            entity_specifics, /*creation_time=*/1000,
            /*last_modified_time=*/1000));
  }

 private:
  base::test::ScopedFeatureList features_;
};

INSTANTIATE_TEST_SUITE_P(
    /* no prefix */,
    SingleClientWalletCredentialSyncTest,
    GetSyncTestModes(),
    testing::PrintToStringParamName());

// Ensures that the `AUTOFILL_WALLET_CREDENTIAL` sync type is enabled by
// default.
IN_PROC_BROWSER_TEST_P(SingleClientWalletCredentialSyncTest, EnabledByDefault) {
  ASSERT_TRUE(SetupSync());
  ASSERT_TRUE(GetSyncService(0)->GetActiveDataTypes().Has(
      syncer::AUTOFILL_WALLET_CREDENTIAL));
}

// ChromeOS does not support late signin after profile creation, so the test
// below does not apply.
#if !BUILDFLAG(IS_CHROMEOS)
// Verify card and CVC data is synced when the user signs in.
IN_PROC_BROWSER_TEST_P(SingleClientWalletCredentialSyncTest,
                       DownloadCardCredential) {
  SetDefaultWalletCredentialOnFakeServer();
  GetFakeServer()->SetWalletData({CreateDefaultSyncWalletCard()});

  ASSERT_TRUE(SetupClients());
  ASSERT_TRUE(GetClient(0)->SignInPrimaryAccount());
  ASSERT_TRUE(GetClient(0)->AwaitSyncTransportActive());
  WaitForCvcOnCard(GetPaymentsDataManager(0));

  ASSERT_EQ(syncer::SyncService::TransportState::ACTIVE,
            GetSyncService(0)->GetTransportState());
  ASSERT_TRUE(GetSyncService(0)->GetActiveDataTypes().Has(
      syncer::AUTOFILL_WALLET_CREDENTIAL));

  scoped_refptr<autofill::AutofillWebDataService> profile_data =
      GetProfileWebDataService(0);
  ASSERT_NE(nullptr, profile_data);
  scoped_refptr<autofill::AutofillWebDataService> account_data =
      GetAccountWebDataService(0);
  ASSERT_NE(nullptr, account_data);

  // Check that no data is stored in the profile storage.
  EXPECT_EQ(0U, GetServerCards(profile_data).size());

  // Check that one card is stored in the account storage.
  EXPECT_EQ(1U, GetServerCards(account_data).size());

  PaymentsDataManager* paydm = GetPaymentsDataManager(0);
  ASSERT_NE(nullptr, paydm);

  std::vector<const autofill::CreditCard*> cards = paydm->GetCreditCards();
  ASSERT_EQ(1uL, cards.size());
  EXPECT_FALSE(cards[0]->cvc().empty());
  ExpectDefaultWalletCredentialValues(*cards[0]);
}

// Card and CVC data should get cleared from the database when the user signs
// out and different data should get downstreamed when the user signs in with a
// different account.
IN_PROC_BROWSER_TEST_P(SingleClientWalletCredentialSyncTest,
                       ClearOnSignOutAndDownstreamOnSignIn) {
  SetDefaultWalletCredentialOnFakeServer();
  GetFakeServer()->SetWalletData({CreateDefaultSyncWalletCard()});

  ASSERT_TRUE(SetupClients());
  ASSERT_TRUE(GetClient(0)->SignInPrimaryAccount());
  ASSERT_TRUE(GetClient(0)->AwaitSyncTransportActive());
  WaitForCvcOnCard(GetPaymentsDataManager(0));

  ASSERT_EQ(syncer::SyncService::TransportState::ACTIVE,
            GetSyncService(0)->GetTransportState());
  ASSERT_TRUE(GetSyncService(0)->GetActiveDataTypes().Has(
      syncer::AUTOFILL_WALLET_CREDENTIAL));

  scoped_refptr<autofill::AutofillWebDataService> profile_data =
      GetProfileWebDataService(0);
  ASSERT_NE(nullptr, profile_data);
  scoped_refptr<autofill::AutofillWebDataService> account_data =
      GetAccountWebDataService(0);
  ASSERT_NE(nullptr, account_data);

  // Check that no data is stored in the profile storage.
  EXPECT_EQ(0U, GetServerCards(profile_data).size());

  // Check that one card is stored in the account storage.
  EXPECT_EQ(1U, GetServerCards(account_data).size());

  PaymentsDataManager* paydm = GetPaymentsDataManager(0);
  ASSERT_NE(nullptr, paydm);

  std::vector<const autofill::CreditCard*> cards = paydm->GetCreditCards();
  ASSERT_EQ(1uL, cards.size());
  EXPECT_FALSE(cards[0]->cvc().empty());
  ExpectDefaultWalletCredentialValues(*cards[0]);

  GetClient(0)->SignOutPrimaryAccount();
  WaitForNoPaymentsCustomerData(paydm);
  EXPECT_EQ(0U, GetServerCards(profile_data).size());
  EXPECT_EQ(0U, GetServerCards(account_data).size());

  // Verify that sync is stopped.
  ASSERT_EQ(syncer::SyncService::TransportState::DISABLED,
            GetSyncService(0)->GetTransportState());
  ASSERT_FALSE(GetSyncService(0)->GetActiveDataTypes().Has(
      syncer::AUTOFILL_WALLET_CREDENTIAL));

  // Set a different set of cards on the server, then sign in again (this is a
  // good enough approximation of signing in with a different Google account).
  GetFakeServer()->DeleteAllEntitiesForDataType(
      syncer::AUTOFILL_WALLET_CREDENTIAL);
  sync_pb::EntitySpecifics entity_specifics_2;
  *entity_specifics_2.mutable_autofill_wallet_credential() =
      *CreateSyncWalletCredential(
           ServerCvc{/*instrument_id=*/9, /*cvc=*/u"720",
                     /*last_updated_timestamp=*/base::Time::UnixEpoch() +
                         base::Milliseconds(50000)})
           .mutable_specifics()
           ->mutable_autofill_wallet_credential();

  GetFakeServer()->SetWalletData(
      {CreateSyncWalletCard(
           /*name=*/"new-card", /*last_four=*/"0002", kDefaultBillingAddressID,
           /*nickname=*/"", /*instrument_id=*/9),
       CreateSyncPaymentsCustomerData(
           /*customer_id=*/"different")});
  GetFakeServer()->InjectEntity(
      syncer::PersistentUniqueClientEntity::CreateFromSpecificsForTesting(
          /*non_unique_name=*/"credential_2",
          entity_specifics_2.mutable_autofill_wallet_credential()
              ->instrument_id(),
          entity_specifics_2, /*creation_time=*/1000,
          /*last_modified_time=*/0));

  ASSERT_TRUE(GetClient(0)->SignInPrimaryAccount());
  ASSERT_TRUE(GetClient(0)->AwaitSyncTransportActive());
  WaitForCvcOnCard(GetPaymentsDataManager(0));

  ASSERT_EQ(syncer::SyncService::TransportState::ACTIVE,
            GetSyncService(0)->GetTransportState());
  ASSERT_TRUE(GetSyncService(0)->GetActiveDataTypes().Has(
      syncer::AUTOFILL_WALLET_CREDENTIAL));

  scoped_refptr<autofill::AutofillWebDataService> profile_data_2 =
      GetProfileWebDataService(0);
  ASSERT_NE(nullptr, profile_data_2);
  scoped_refptr<autofill::AutofillWebDataService> account_data_2 =
      GetAccountWebDataService(0);
  ASSERT_NE(nullptr, account_data_2);

  // Check that no data is stored in the profile storage.
  EXPECT_EQ(0U, GetServerCards(profile_data_2).size());

  // Check that one card is stored in the account storage.
  EXPECT_EQ(1U, GetServerCards(account_data_2).size());

  std::vector<const autofill::CreditCard*> cards_2 = paydm->GetCreditCards();
  ASSERT_EQ(1uL, cards_2.size());

  // Check for CVC data on the correct card with the `instrument_id` of 9.
  ASSERT_EQ(9, cards_2[0]->instrument_id());
  EXPECT_FALSE(cards_2[0]->cvc().empty());
  ASSERT_EQ(u"720", cards_2[0]->cvc());
}

// Verify if 2 cards are synced down along with a single wallet credential
// entity, the credential entity is attached to the correct card.
IN_PROC_BROWSER_TEST_P(SingleClientWalletCredentialSyncTest,
                       CorrectCvcSyncAttachedToCardEntity) {
  SetDefaultWalletCredentialOnFakeServer();
  GetFakeServer()->SetWalletData(
      {CreateDefaultSyncWalletCard(),
       CreateSyncWalletCard(
           /*name=*/"new-card", /*last_four=*/"0002", kDefaultBillingAddressID,
           /*nickname=*/"", /*instrument_id=*/123),
       CreateSyncPaymentsCustomerData(
           /*customer_id=*/"different")});

  ASSERT_TRUE(SetupClients());
  ASSERT_TRUE(GetClient(0)->SignInPrimaryAccount());
  ASSERT_TRUE(GetClient(0)->AwaitSyncTransportActive());
  WaitForCvcOnCard(GetPaymentsDataManager(0));

  ASSERT_EQ(syncer::SyncService::TransportState::ACTIVE,
            GetSyncService(0)->GetTransportState());
  ASSERT_TRUE(GetSyncService(0)->GetActiveDataTypes().Has(
      syncer::AUTOFILL_WALLET_CREDENTIAL));

  scoped_refptr<autofill::AutofillWebDataService> profile_data =
      GetProfileWebDataService(0);
  ASSERT_NE(nullptr, profile_data);
  scoped_refptr<autofill::AutofillWebDataService> account_data =
      GetAccountWebDataService(0);
  ASSERT_NE(nullptr, account_data);

  // Check that no data is stored in the profile storage.
  EXPECT_EQ(0U, GetServerCards(profile_data).size());

  // Check that two cards are stored in the account storage.
  EXPECT_EQ(2U, GetServerCards(account_data).size());

  PaymentsDataManager* paydm = GetPaymentsDataManager(0);
  ASSERT_NE(nullptr, paydm);
  std::vector<const autofill::CreditCard*> cards = paydm->GetCreditCards();
  ASSERT_EQ(2uL, cards.size());

  const autofill::CreditCard* card_with_cvc =
      (cards[0]->instrument_id() != 123) ? cards[0] : cards[1];
  const autofill::CreditCard* card_without_cvc =
      (cards[0]->instrument_id() != 123) ? cards[1] : cards[0];

  // Check for CVC data on the correct card where the `instrument_id` != 123.
  EXPECT_FALSE(card_with_cvc->cvc().empty());
  EXPECT_EQ(u"098", card_with_cvc->cvc());
  EXPECT_TRUE(card_without_cvc->cvc().empty());
}

// Verify that card and CVC data should get cleared from the database when the
// user signs out.
IN_PROC_BROWSER_TEST_P(SingleClientWalletCredentialSyncTest, ClearOnSignOut) {
  SetDefaultWalletCredentialOnFakeServer();
  GetFakeServer()->SetWalletData({CreateDefaultSyncWalletCard()});
  ASSERT_TRUE(SetupSync());

  PaymentsDataManager* paydm = GetPaymentsDataManager(0);
  ASSERT_NE(nullptr, paydm);

  // Make sure the wallet and credential data is in the DB.
  ASSERT_EQ(1uL, paydm->GetCreditCards().size());
  EXPECT_FALSE(paydm->GetCreditCards()[0]->cvc().empty());
  ExpectDefaultWalletCredentialValues(*paydm->GetCreditCards()[0]);

  // Signout, the wallet and credential data should be gone.
  GetClient(0)->SignOutPrimaryAccount();
  WaitForNumberOfCards(0, paydm);

  EXPECT_EQ(0uL, paydm->GetCreditCards().size());
}

// Verify that card and CVC data should get cleared from the database when the
// user signs out from Transport mode.
IN_PROC_BROWSER_TEST_P(SingleClientWalletCredentialSyncTest,
                       ClearOnSignOutFromTransportMode) {
  SetDefaultWalletCredentialOnFakeServer();
  GetFakeServer()->SetWalletData({CreateDefaultSyncWalletCard()});

  ASSERT_TRUE(SetupClients());
  ASSERT_TRUE(GetClient(0)->SignInPrimaryAccount());
  ASSERT_TRUE(GetClient(0)->AwaitSyncTransportActive());
  WaitForCvcOnCard(GetPaymentsDataManager(0));

  ASSERT_EQ(syncer::SyncService::TransportState::ACTIVE,
            GetSyncService(0)->GetTransportState());
  ASSERT_TRUE(GetSyncService(0)->GetActiveDataTypes().Has(
      syncer::AUTOFILL_WALLET_CREDENTIAL));

  PaymentsDataManager* paydm = GetPaymentsDataManager(0);
  ASSERT_NE(nullptr, paydm);

  // Make sure the wallet and credential data is in the DB.
  ASSERT_EQ(1uL, paydm->GetCreditCards().size());
  EXPECT_FALSE(paydm->GetCreditCards()[0]->cvc().empty());
  ExpectDefaultWalletCredentialValues(*paydm->GetCreditCards()[0]);

  // Signout, the wallet and credential data should be gone.
  GetClient(0)->SignOutPrimaryAccount();
  WaitForNumberOfCards(0, paydm);

  EXPECT_EQ(0uL, paydm->GetCreditCards().size());
}

#endif  // !BUILDFLAG(IS_CHROMEOS)

// Verify that card and CVC data should get cleared from the database when the
// sync for Payments is disabled.
IN_PROC_BROWSER_TEST_P(SingleClientWalletCredentialSyncTest,
                       ClearOnDisablePaymentsSync) {
  SetDefaultWalletCredentialOnFakeServer();
  GetFakeServer()->SetWalletData({CreateDefaultSyncWalletCard()});
  ASSERT_TRUE(SetupSync());

  PaymentsDataManager* paydm = GetPaymentsDataManager(0);
  ASSERT_NE(nullptr, paydm);

  // Make sure the wallet and credential data is in the DB.
  ASSERT_EQ(1uL, paydm->GetCreditCards().size());
  EXPECT_FALSE(paydm->GetCreditCards()[0]->cvc().empty());
  ExpectDefaultWalletCredentialValues(*paydm->GetCreditCards()[0]);

  // Disable sync for `kPayments`, the wallet and credential data should be
  // gone.
  ASSERT_TRUE(GetClient(0)->DisableSelectableType(
      syncer::UserSelectableType::kPayments));
  WaitForNumberOfCards(0, paydm);

  EXPECT_EQ(0uL, paydm->GetCreditCards().size());

  // Enable sync for `kPayments`, the wallet and credential data should come
  // back.
  ASSERT_TRUE(GetClient(0)->EnableSelectableType(
      syncer::UserSelectableType::kPayments));

  // Wait until Sync restores the card and it arrives at paydm.
  WaitForNumberOfCards(1, paydm);

  EXPECT_EQ(1uL, paydm->GetCreditCards().size());
}

// Card and CVC data should get cleared from the database when the user enters
// the sync paused state (e.g. persistent auth error).
IN_PROC_BROWSER_TEST_P(SingleClientWalletCredentialSyncTest,
                       ClearOnSyncPaused) {
  SetDefaultWalletCredentialOnFakeServer();
  GetFakeServer()->SetWalletData({CreateDefaultSyncWalletCard()});
  ASSERT_TRUE(SetupSync());

  PaymentsDataManager* paydm = GetPaymentsDataManager(0);
  ASSERT_NE(nullptr, paydm);

  // Make sure the wallet and credential data is in the DB.
  ASSERT_EQ(1uL, paydm->GetCreditCards().size());
  EXPECT_FALSE(paydm->GetCreditCards()[0]->cvc().empty());
  ExpectDefaultWalletCredentialValues(*paydm->GetCreditCards()[0]);

  // Enter sync paused state, the wallet and credential data should be gone.
  if (GetSetupSyncMode() == SetupSyncMode::kSyncTheFeature) {
    GetClient(0)->EnterSyncPausedStateForPrimaryAccount();
  } else {
    GetClient(0)->EnterSignInPendingStateForPrimaryAccount();
  }

  WaitForNumberOfCards(0, paydm);

  EXPECT_EQ(0uL, paydm->GetCreditCards().size());

  if (GetSetupSyncMode() == SetupSyncMode::kSyncTheFeature) {
    GetClient(0)->ExitSyncPausedStateForPrimaryAccount();
  } else {
    GetClient(0)->ExitSignInPendingStateForPrimaryAccount();
  }

  WaitForNumberOfCards(1, paydm);

  ASSERT_EQ(1uL, paydm->GetCreditCards().size());
  EXPECT_FALSE(paydm->GetCreditCards()[0]->cvc().empty());
  ExpectDefaultWalletCredentialValues(*paydm->GetCreditCards()[0]);
}

// CVC data is using incremental updates. Make sure existing data doesn't get
// replaced when new data is synced down.
IN_PROC_BROWSER_TEST_P(SingleClientWalletCredentialSyncTest,
                       NewSyncDataShouldBeIncremental) {
  SetDefaultWalletCredentialOnFakeServer();
  GetFakeServer()->SetWalletData(
      {CreateDefaultSyncWalletCard(),
       CreateSyncWalletCard(/*name=*/"card-2", /*last_four=*/"0001",
                            kDefaultBillingAddressID,
                            /*nickname=*/"",
                            /*instrument_id=*/123)});
  ASSERT_TRUE(SetupSync());

  // Make sure the data is in the DB.
  PaymentsDataManager* paydm = GetPaymentsDataManager(0);
  ASSERT_NE(nullptr, paydm);

  std::vector<const autofill::CreditCard*> cards = paydm->GetCreditCards();
  ASSERT_EQ(2uL, cards.size());

  const autofill::CreditCard* card_with_cvc_1 =
      (cards[1]->instrument_id() == 123) ? cards[0] : cards[1];
  EXPECT_FALSE(card_with_cvc_1->cvc().empty());
  ExpectDefaultWalletCredentialValues(*card_with_cvc_1);

  // Add credentials for the second card to the server.
  sync_pb::EntitySpecifics entity_specifics_2;
  *entity_specifics_2.mutable_autofill_wallet_credential() =
      *CreateSyncWalletCredential(
           ServerCvc{/*instrument_id=*/123, /*cvc=*/u"720",
                     /*last_updated_timestamp=*/base::Time::UnixEpoch() +
                         base::Milliseconds(50000)})
           .mutable_specifics()
           ->mutable_autofill_wallet_credential();

  GetFakeServer()->InjectEntity(
      syncer::PersistentUniqueClientEntity::CreateFromSpecificsForTesting(
          /*non_unique_name=*/"credential_2",
          entity_specifics_2.mutable_autofill_wallet_credential()
              ->instrument_id(),
          entity_specifics_2, /*creation_time=*/1000,
          /*last_modified_time=*/0));

  PaymentsDataChangedWaiter(paydm).Wait();

  // Make sure both the credential data is present.
  cards = paydm->GetCreditCards();
  ASSERT_EQ(2uL, cards.size());

  card_with_cvc_1 = (cards[1]->instrument_id() == 123) ? cards[0] : cards[1];
  const autofill::CreditCard* card_with_cvc_2 =
      (cards[1]->instrument_id() == 123) ? cards[1] : cards[0];

  EXPECT_FALSE(card_with_cvc_1->cvc().empty());
  EXPECT_FALSE(card_with_cvc_2->cvc().empty());
  ExpectDefaultWalletCredentialValues(*card_with_cvc_1);
  EXPECT_EQ(u"720", card_with_cvc_2->cvc());
}

// Verify that card and CVC data should get cleared from the database when the
// wallet sync is disabled.
IN_PROC_BROWSER_TEST_P(SingleClientWalletCredentialSyncTest,
                       ClearOnDisableWalletSync) {
  SetDefaultWalletCredentialOnFakeServer();
  GetFakeServer()->SetWalletData({CreateDefaultSyncWalletCard()});
  ASSERT_TRUE(SetupSync());

  PaymentsDataManager* paydm = GetPaymentsDataManager(0);
  ASSERT_NE(nullptr, paydm);

  // Make sure the wallet and credential data is in the DB.
  ASSERT_EQ(1uL, paydm->GetCreditCards().size());
  EXPECT_FALSE(paydm->GetCreditCards()[0]->cvc().empty());
  ExpectDefaultWalletCredentialValues(*paydm->GetCreditCards()[0]);

  // Turn off payments sync, the wallet and credential data should be gone.
  ASSERT_TRUE(GetClient(0)->DisableSelectableType(
      syncer::UserSelectableType::kPayments));

  WaitForNumberOfCards(0, paydm);

  EXPECT_EQ(0uL, paydm->GetCreditCards().size());
}

// Verify when the corresponding card of a CVC is deleted from pay.google.com
// and wallet data sync is triggered, it will delete the orphaned CVC from local
// DB and Chrome sync server.
IN_PROC_BROWSER_TEST_P(SingleClientWalletCredentialSyncTest,
                       ReconcileServerCvcForWalletCards) {
  // Set a wallet card on the fake server. This card will be synced first to the
  // client.
  GetFakeServer()->SetWalletData(
      {CreateSyncWalletCard(/*name=*/"card-1", /*last_four=*/"0001",
                            kDefaultBillingAddressID, /*nickname=*/"",
                            /*instrument_id=*/1)});
  ASSERT_TRUE(SetupSync());
  ASSERT_EQ(syncer::SyncService::TransportState::ACTIVE,
            GetSyncService(0)->GetTransportState());

  PaymentsDataManager* paydm = GetPaymentsDataManager(0);
  ASSERT_NE(nullptr, paydm);
  ASSERT_EQ(1uL, paydm->GetCreditCards().size());

  // Add 2 wallet credential entities (CVC) on the fake server. One of them is
  // linked to the card created above and the other credential has no linkage to
  // any cards on the client aka orphaned.
  SetDefaultWalletCredentialOnFakeServer();
  SetWalletCredentialOnFakeServer(
      ServerCvc{/*instrument_id=*/9, /*cvc=*/u"720",
                /*last_updated_timestamp=*/base::Time::UnixEpoch() +
                    base::Milliseconds(50000)});
  WaitForCvcOnCard(paydm);
  EXPECT_FALSE(paydm->GetCreditCards()[0]->cvc().empty());

  // The count for CVCs on the fake server is still 2 as we reconcile the CVC
  // data on the wallet sync bridge.
  EXPECT_TRUE(ServerCvcChecker(/*expected_count=*/2ul).Wait());

  // Creating an update for the card to force a sync of the new data and trigger
  // the reconcile flow on the wallet sync bridge.
  GetFakeServer()->SetWalletData(
      {CreateSyncWalletCard(/*name=*/"card-1-updated", /*last_four=*/"0001",
                            kDefaultBillingAddressID, /*nickname=*/"nickname",
                            /*instrument_id=*/1),
       CreateSyncPaymentsCustomerData(
           /*customer_id=*/"different")});

  // Verify that Chrome sync server has deleted the orphaned CVC by verifying
  // the count and the CVC value on the card.
  EXPECT_TRUE(ServerCvcChecker(/*expected_count=*/1ul).Wait());
  EXPECT_FALSE(paydm->GetCreditCards()[0]->cvc().empty());
}
