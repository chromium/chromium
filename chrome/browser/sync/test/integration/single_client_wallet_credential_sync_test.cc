// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/notreached.h"
#include "chrome/browser/sync/test/integration/fake_server_match_status_checker.h"
#include "chrome/browser/sync/test/integration/sync_service_impl_harness.h"
#include "chrome/browser/sync/test/integration/sync_test.h"
#include "chrome/browser/sync/test/integration/wallet_helper.h"
#include "components/autofill/core/browser/data_model/credit_card.h"
#include "components/autofill/core/browser/payments_data_manager.h"
#include "components/autofill/core/browser/personal_data_manager.h"
#include "components/autofill/core/browser/personal_data_manager_observer.h"
#include "components/autofill/core/browser/personal_data_manager_test_utils.h"
#include "components/autofill/core/common/autofill_payments_features.h"
#include "components/sync/base/data_type.h"
#include "components/sync/base/features.h"
#include "components/sync/engine/loopback_server/persistent_tombstone_entity.h"
#include "content/public/test/browser_test.h"

using autofill::ServerCvc;
using syncer::kSyncAutofillWalletCredentialData;
using wallet_helper::CreateDefaultSyncWalletCard;
using wallet_helper::CreateDefaultSyncWalletCredential;
using wallet_helper::CreateSyncPaymentsCustomerData;
using wallet_helper::CreateSyncWalletCard;
using wallet_helper::CreateSyncWalletCredential;
using wallet_helper::ExpectDefaultWalletCredentialValues;
using wallet_helper::GetAccountWebDataService;
using wallet_helper::GetPersonalDataManager;
using wallet_helper::GetProfileWebDataService;
using wallet_helper::kDefaultBillingAddressID;

namespace {

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

class AutofillWebDataServiceConsumer : public WebDataServiceConsumer {
 public:
  AutofillWebDataServiceConsumer() = default;

  AutofillWebDataServiceConsumer(const AutofillWebDataServiceConsumer&) =
      delete;
  AutofillWebDataServiceConsumer& operator=(
      const AutofillWebDataServiceConsumer&) = delete;

  ~AutofillWebDataServiceConsumer() override = default;

  void OnWebDataServiceRequestDone(
      WebDataServiceBase::Handle handle,
      std::unique_ptr<WDTypedResult> result) override {
    CHECK(result->GetType() == AUTOFILL_CREDITCARDS_RESULT);
    result_ =
        static_cast<
            WDResult<std::vector<std::unique_ptr<autofill::CreditCard>>>*>(
            result.get())
            ->GetValue();
    run_loop_.Quit();
  }

  void Wait() { run_loop_.Run(); }

  std::vector<std::unique_ptr<autofill::CreditCard>>& result() {
    return result_;
  }

 private:
  base::RunLoop run_loop_;
  std::vector<std::unique_ptr<autofill::CreditCard>> result_;
};

#if !BUILDFLAG(IS_CHROMEOS_ASH)
std::vector<std::unique_ptr<autofill::CreditCard>> GetServerCards(
    scoped_refptr<autofill::AutofillWebDataService> service) {
  AutofillWebDataServiceConsumer consumer;
  service->GetServerCreditCards(&consumer);
  consumer.Wait();
  return std::move(consumer.result());
}
#endif  // !BUILDFLAG(IS_CHROMEOS_ASH)

}  // namespace

class SingleClientWalletCredentialSyncTest : public SyncTest {
 public:
  SingleClientWalletCredentialSyncTest() : SyncTest(SINGLE_CLIENT) {
    features_.InitWithFeatures(
        /*enabled_features=*/{kSyncAutofillWalletCredentialData,
                              autofill::features::
                                  kAutofillEnableCvcStorageAndFilling},
        /*disabled_features=*/{});
  }

  ~SingleClientWalletCredentialSyncTest() override = default;

  SingleClientWalletCredentialSyncTest(
      const SingleClientWalletCredentialSyncTest&) = delete;
  SingleClientWalletCredentialSyncTest& operator=(
      const SingleClientWalletCredentialSyncTest&) = delete;

 protected:
  void WaitForOnPersonalDataChanged(autofill::PersonalDataManager* pdm) {
    testing::NiceMock<autofill::PersonalDataLoadedObserverMock>
        personal_data_observer;
    pdm->AddObserver(&personal_data_observer);
    base::RunLoop run_loop;
    EXPECT_CALL(personal_data_observer, OnPersonalDataChanged())
        .WillRepeatedly([&]() { run_loop.Quit(); });
    run_loop.Run();
    pdm->RemoveObserver(&personal_data_observer);
  }

  void WaitForNumberOfCards(size_t expected_count,
                            autofill::PersonalDataManager* pdm) {
    while (pdm->payments_data_manager().GetCreditCards().size() !=
               expected_count ||
           pdm->payments_data_manager().HasPendingPaymentQueries()) {
      WaitForOnPersonalDataChanged(pdm);
    }
  }

  void WaitForNoPaymentsCustomerData(autofill::PersonalDataManager* pdm) {
    while (pdm->payments_data_manager().GetPaymentsCustomerData() != nullptr ||
           pdm->payments_data_manager().HasPendingPaymentQueries()) {
      WaitForOnPersonalDataChanged(pdm);
    }
  }

  void WaitForCvcOnCard(autofill::PersonalDataManager* pdm) {
    while (!IsCvcAvailableOnAnyCard(pdm)) {
      WaitForOnPersonalDataChanged(pdm);
    }
  }

  bool IsCvcAvailableOnAnyCard(autofill::PersonalDataManager* pdm) {
    for (const autofill::CreditCard* credit_card :
         pdm->payments_data_manager().GetCreditCards()) {
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

// Ensures that the `AUTOFILL_WALLET_CREDENTIAL` sync type is enabled by
// default.
IN_PROC_BROWSER_TEST_F(SingleClientWalletCredentialSyncTest, EnabledByDefault) {
  ASSERT_TRUE(SetupSync());
  ASSERT_TRUE(GetSyncService(0)->GetActiveDataTypes().Has(
      syncer::AUTOFILL_WALLET_CREDENTIAL));
}

// ChromeOS does not support late signin after profile creation, so the test
// below does not apply.
#if !BUILDFLAG(IS_CHROMEOS_ASH)
// Verify card and CVC data is synced when the user signs in.
IN_PROC_BROWSER_TEST_F(SingleClientWalletCredentialSyncTest,
                       DownloadCardCredential) {
  SetDefaultWalletCredentialOnFakeServer();
  GetFakeServer()->SetWalletData({CreateDefaultSyncWalletCard()});

  ASSERT_TRUE(SetupClients());
  ASSERT_TRUE(GetClient(0)->SignInPrimaryAccount());
  ASSERT_TRUE(GetClient(0)->AwaitSyncTransportActive());
  WaitForCvcOnCard(GetPersonalDataManager(0));

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

  autofill::PersonalDataManager* pdm = GetPersonalDataManager(0);
  ASSERT_NE(nullptr, pdm);

  std::vector<autofill::CreditCard*> cards =
      pdm->payments_data_manager().GetCreditCards();
  ASSERT_EQ(1uL, cards.size());
  EXPECT_FALSE(cards[0]->cvc().empty());
  ExpectDefaultWalletCredentialValues(*cards[0]);
}

// Card and CVC data should get cleared from the database when the user signs
// out and different data should get downstreamed when the user signs in with a
// different account.
IN_PROC_BROWSER_TEST_F(SingleClientWalletCredentialSyncTest,
                       ClearOnSignOutAndDownstreamOnSignIn) {
  SetDefaultWalletCredentialOnFakeServer();
  GetFakeServer()->SetWalletData({CreateDefaultSyncWalletCard()});

  ASSERT_TRUE(SetupClients());
  ASSERT_TRUE(GetClient(0)->SignInPrimaryAccount());
  ASSERT_TRUE(GetClient(0)->AwaitSyncTransportActive());
  WaitForCvcOnCard(GetPersonalDataManager(0));

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

  autofill::PersonalDataManager* pdm = GetPersonalDataManager(0);
  ASSERT_NE(nullptr, pdm);

  std::vector<autofill::CreditCard*> cards =
      pdm->payments_data_manager().GetCreditCards();
  ASSERT_EQ(1uL, cards.size());
  EXPECT_FALSE(cards[0]->cvc().empty());
  ExpectDefaultWalletCredentialValues(*cards[0]);

  GetClient(0)->SignOutPrimaryAccount();
  WaitForNoPaymentsCustomerData(pdm);
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
  WaitForCvcOnCard(GetPersonalDataManager(0));

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

  std::vector<autofill::CreditCard*> cards_2 =
      pdm->payments_data_manager().GetCreditCards();
  ASSERT_EQ(1uL, cards_2.size());

  // Check for CVC data on the correct card with the `instrument_id` of 9.
  ASSERT_EQ(9, cards_2[0]->instrument_id());
  EXPECT_FALSE(cards_2[0]->cvc().empty());
  ASSERT_EQ(u"720", cards_2[0]->cvc());
}

// Verify if 2 cards are synced down along with a single wallet credential
// entity, the credential entity is attached to the correct card.
IN_PROC_BROWSER_TEST_F(SingleClientWalletCredentialSyncTest,
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
  WaitForCvcOnCard(GetPersonalDataManager(0));

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

  autofill::PersonalDataManager* pdm = GetPersonalDataManager(0);
  ASSERT_NE(nullptr, pdm);
  std::vector<autofill::CreditCard*> cards =
      pdm->payments_data_manager().GetCreditCards();
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
IN_PROC_BROWSER_TEST_F(SingleClientWalletCredentialSyncTest, ClearOnSignOut) {
  SetDefaultWalletCredentialOnFakeServer();
  GetFakeServer()->SetWalletData({CreateDefaultSyncWalletCard()});
  ASSERT_TRUE(SetupSync());

  autofill::PersonalDataManager* pdm = GetPersonalDataManager(0);
  ASSERT_NE(nullptr, pdm);

  // Make sure the wallet and credential data is in the DB.
  ASSERT_EQ(1uL, pdm->payments_data_manager().GetCreditCards().size());
  EXPECT_FALSE(pdm->payments_data_manager().GetCreditCards()[0]->cvc().empty());
  ExpectDefaultWalletCredentialValues(
      *pdm->payments_data_manager().GetCreditCards()[0]);

  // Signout, the wallet and credential data should be gone.
  GetClient(0)->SignOutPrimaryAccount();
  WaitForNumberOfCards(0, pdm);

  EXPECT_EQ(0uL, pdm->payments_data_manager().GetCreditCards().size());
}

// Verify that card and CVC data should get cleared from the database when the
// user signs out from Transport mode.
IN_PROC_BROWSER_TEST_F(SingleClientWalletCredentialSyncTest,
                       ClearOnSignOutFromTransportMode) {
  SetDefaultWalletCredentialOnFakeServer();
  GetFakeServer()->SetWalletData({CreateDefaultSyncWalletCard()});

  ASSERT_TRUE(SetupClients());
  ASSERT_TRUE(GetClient(0)->SignInPrimaryAccount());
  ASSERT_TRUE(GetClient(0)->AwaitSyncTransportActive());
  WaitForCvcOnCard(GetPersonalDataManager(0));

  ASSERT_EQ(syncer::SyncService::TransportState::ACTIVE,
            GetSyncService(0)->GetTransportState());
  ASSERT_TRUE(GetSyncService(0)->GetActiveDataTypes().Has(
      syncer::AUTOFILL_WALLET_CREDENTIAL));

  autofill::PersonalDataManager* pdm = GetPersonalDataManager(0);
  ASSERT_NE(nullptr, pdm);

  // Make sure the wallet and credential data is in the DB.
  ASSERT_EQ(1uL, pdm->payments_data_manager().GetCreditCards().size());
  EXPECT_FALSE(pdm->payments_data_manager().GetCreditCards()[0]->cvc().empty());
  ExpectDefaultWalletCredentialValues(
      *pdm->payments_data_manager().GetCreditCards()[0]);

  // Signout, the wallet and credential data should be gone.
  GetClient(0)->SignOutPrimaryAccount();
  WaitForNumberOfCards(0, pdm);

  EXPECT_EQ(0uL, pdm->payments_data_manager().GetCreditCards().size());
}

#endif  // !BUILDFLAG(IS_CHROMEOS_ASH)

// Verify that card and CVC data should get cleared from the database when the
// sync for Payments is disabled.
IN_PROC_BROWSER_TEST_F(SingleClientWalletCredentialSyncTest,
                       ClearOnDisablePaymentsSync) {
  SetDefaultWalletCredentialOnFakeServer();
  GetFakeServer()->SetWalletData({CreateDefaultSyncWalletCard()});
  ASSERT_TRUE(SetupSync());

  autofill::PersonalDataManager* pdm = GetPersonalDataManager(0);
  ASSERT_NE(nullptr, pdm);

  // Make sure the wallet and credential data is in the DB.
  ASSERT_EQ(1uL, pdm->payments_data_manager().GetCreditCards().size());
  EXPECT_FALSE(pdm->payments_data_manager().GetCreditCards()[0]->cvc().empty());
  ExpectDefaultWalletCredentialValues(
      *pdm->payments_data_manager().GetCreditCards()[0]);

  // Disable sync for `kPayments`, the wallet and credential data should be
  // gone.
  ASSERT_TRUE(
      GetClient(0)->DisableSyncForType(syncer::UserSelectableType::kPayments));
  WaitForNumberOfCards(0, pdm);

  EXPECT_EQ(0uL, pdm->payments_data_manager().GetCreditCards().size());

  // Enable sync for `kPayments`, the wallet and credential data should come
  // back.
  ASSERT_TRUE(
      GetClient(0)->EnableSyncForType(syncer::UserSelectableType::kPayments));

  // Wait until Sync restores the card and it arrives at PDM.
  WaitForNumberOfCards(1, pdm);

  EXPECT_EQ(1uL, pdm->payments_data_manager().GetCreditCards().size());
}

// Card and CVC data should get cleared from the database when the user enters
// the sync paused state (e.g. persistent auth error).
IN_PROC_BROWSER_TEST_F(SingleClientWalletCredentialSyncTest,
                       ClearOnSyncPaused) {
  SetDefaultWalletCredentialOnFakeServer();
  GetFakeServer()->SetWalletData({CreateDefaultSyncWalletCard()});
  ASSERT_TRUE(SetupSync());

  autofill::PersonalDataManager* pdm = GetPersonalDataManager(0);
  ASSERT_NE(nullptr, pdm);

  // Make sure the wallet and credential data is in the DB.
  ASSERT_EQ(1uL, pdm->payments_data_manager().GetCreditCards().size());
  EXPECT_FALSE(pdm->payments_data_manager().GetCreditCards()[0]->cvc().empty());
  ExpectDefaultWalletCredentialValues(
      *pdm->payments_data_manager().GetCreditCards()[0]);

  // Enter sync paused state, the wallet and credential data should be gone.
  GetClient(0)->EnterSyncPausedStateForPrimaryAccount();
  WaitForNumberOfCards(0, pdm);

  EXPECT_EQ(0uL, pdm->payments_data_manager().GetCreditCards().size());

  GetClient(0)->ExitSyncPausedStateForPrimaryAccount();
  WaitForNumberOfCards(1, pdm);

  ASSERT_EQ(1uL, pdm->payments_data_manager().GetCreditCards().size());
  EXPECT_FALSE(pdm->payments_data_manager().GetCreditCards()[0]->cvc().empty());
  ExpectDefaultWalletCredentialValues(
      *pdm->payments_data_manager().GetCreditCards()[0]);
}

// CVC data is using incremental updates. Make sure existing data doesn't get
// replaced when new data is synced down.
IN_PROC_BROWSER_TEST_F(SingleClientWalletCredentialSyncTest,
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
  autofill::PersonalDataManager* pdm = GetPersonalDataManager(0);
  ASSERT_NE(nullptr, pdm);

  std::vector<autofill::CreditCard*> cards =
      pdm->payments_data_manager().GetCreditCards();
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

  WaitForOnPersonalDataChanged(pdm);

  // Make sure both the credential data is present.
  cards = pdm->payments_data_manager().GetCreditCards();
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
IN_PROC_BROWSER_TEST_F(SingleClientWalletCredentialSyncTest,
                       ClearOnDisableWalletSync) {
  SetDefaultWalletCredentialOnFakeServer();
  GetFakeServer()->SetWalletData({CreateDefaultSyncWalletCard()});
  ASSERT_TRUE(SetupSync());

  autofill::PersonalDataManager* pdm = GetPersonalDataManager(0);
  ASSERT_NE(nullptr, pdm);

  // Make sure the wallet and credential data is in the DB.
  ASSERT_EQ(1uL, pdm->payments_data_manager().GetCreditCards().size());
  EXPECT_FALSE(pdm->payments_data_manager().GetCreditCards()[0]->cvc().empty());
  ExpectDefaultWalletCredentialValues(
      *pdm->payments_data_manager().GetCreditCards()[0]);

  // Turn off payments sync, the wallet and credential data should be gone.
  ASSERT_TRUE(
      GetClient(0)->DisableSyncForType(syncer::UserSelectableType::kPayments));

  WaitForNumberOfCards(0, pdm);

  EXPECT_EQ(0uL, pdm->payments_data_manager().GetCreditCards().size());
}

// Verify when the corresponding card of a CVC is deleted from pay.google.com
// and wallet data sync is triggered, it will delete the orphaned CVC from local
// DB and Chrome sync server.
IN_PROC_BROWSER_TEST_F(SingleClientWalletCredentialSyncTest,
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

  autofill::PersonalDataManager* pdm = GetPersonalDataManager(0);
  ASSERT_NE(nullptr, pdm);
  ASSERT_EQ(1uL, pdm->payments_data_manager().GetCreditCards().size());

  // Add 2 wallet credential entities (CVC) on the fake server. One of them is
  // linked to the card created above and the other credential has no linkage to
  // any cards on the client aka orphaned.
  SetDefaultWalletCredentialOnFakeServer();
  SetWalletCredentialOnFakeServer(
      ServerCvc{/*instrument_id=*/9, /*cvc=*/u"720",
                /*last_updated_timestamp=*/base::Time::UnixEpoch() +
                    base::Milliseconds(50000)});
  WaitForCvcOnCard(pdm);
  EXPECT_FALSE(pdm->payments_data_manager().GetCreditCards()[0]->cvc().empty());

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
  EXPECT_FALSE(pdm->payments_data_manager().GetCreditCards()[0]->cvc().empty());
}
