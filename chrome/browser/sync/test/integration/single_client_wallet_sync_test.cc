// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/command_line.h"
#include "base/feature_list.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/utf_string_conversions.h"
#include "base/test/metrics/histogram_tester.h"
#include "build/build_config.h"
#include "build/chromeos_buildflags.h"
#include "chrome/browser/signin/identity_manager_factory.h"
#include "chrome/browser/sync/test/integration/autofill_helper.h"
#include "chrome/browser/sync/test/integration/bookmarks_helper.h"
#include "chrome/browser/sync/test/integration/secondary_account_helper.h"
#include "chrome/browser/sync/test/integration/single_client_status_change_checker.h"
#include "chrome/browser/sync/test/integration/sync_integration_test_util.h"
#include "chrome/browser/sync/test/integration/sync_service_impl_harness.h"
#include "chrome/browser/sync/test/integration/sync_test.h"
#include "chrome/browser/sync/test/integration/updated_progress_marker_checker.h"
#include "chrome/browser/sync/test/integration/wallet_helper.h"
#include "chrome/browser/web_data_service_factory.h"
#include "components/autofill/core/browser/autofill_data_util.h"
#include "components/autofill/core/browser/data_model/autofill_metadata.h"
#include "components/autofill/core/browser/data_model/credit_card.h"
#include "components/autofill/core/browser/data_model/credit_card_cloud_token_data.h"
#include "components/autofill/core/browser/field_types.h"
#include "components/autofill/core/browser/metrics/autofill_metrics.h"
#include "components/autofill/core/browser/payments/payments_customer_data.h"
#include "components/autofill/core/browser/personal_data_manager.h"
#include "components/autofill/core/browser/personal_data_manager_observer.h"
#include "components/autofill/core/browser/test_autofill_clock.h"
#include "components/autofill/core/browser/webdata/autofill_sync_bridge_util.h"
#include "components/autofill/core/common/autofill_switches.h"
#include "components/prefs/pref_service.h"
#include "components/signin/public/identity_manager/identity_test_utils.h"
#include "components/sync/base/model_type.h"
#include "components/sync/protocol/autofill_specifics.pb.h"
#include "components/sync/protocol/entity_specifics.pb.h"
#include "components/sync/protocol/model_type_state.pb.h"
#include "components/sync/protocol/sync_entity.pb.h"
#include "components/sync/service/sync_service.h"
#include "components/sync/service/sync_token_status.h"
#include "components/sync/test/entity_builder_factory.h"
#include "components/sync/test/fake_server.h"
#include "components/webdata/common/web_data_service_consumer.h"
#include "content/public/test/browser_test.h"
#include "testing/gmock/include/gmock/gmock.h"

using autofill::AutofillMetadata;
using autofill::AutofillMetrics;
using autofill::AutofillProfile;
using autofill::CreditCard;
using autofill::CreditCardCloudTokenData;
using autofill::data_util::TruncateUTF8;
using testing::Contains;
using wallet_helper::CreateDefaultSyncCreditCardCloudTokenData;
using wallet_helper::CreateDefaultSyncPaymentsCustomerData;
using wallet_helper::CreateDefaultSyncWalletAddress;
using wallet_helper::CreateDefaultSyncWalletCard;
using wallet_helper::CreateSyncCreditCardCloudTokenData;
using wallet_helper::CreateSyncPaymentsCustomerData;
using wallet_helper::CreateSyncWalletAddress;
using wallet_helper::CreateSyncWalletCard;
using wallet_helper::ExpectDefaultCreditCardValues;
using wallet_helper::ExpectDefaultProfileValues;
using wallet_helper::GetAccountWebDataService;
using wallet_helper::GetDefaultCreditCard;
using wallet_helper::GetPersonalDataManager;
using wallet_helper::GetProfileWebDataService;
using wallet_helper::GetServerAddressesMetadata;
using wallet_helper::GetServerCardsMetadata;
using wallet_helper::GetWalletModelTypeState;
using wallet_helper::kDefaultBillingAddressID;
using wallet_helper::kDefaultCardID;
using wallet_helper::kDefaultCustomerID;
using wallet_helper::UnmaskServerCard;

namespace {

ACTION_P(QuitMessageLoop, loop) {
  loop->Quit();
}

MATCHER(AddressHasConverted, "") {
  return arg.specifics().wallet_metadata().address_has_converted();
}

const char kLocalGuidA[] = "EDC609ED-7EEE-4F27-B00C-423242A9C44A";
const char kDifferentBillingAddressId[] = "another address entity ID";
const base::Time kArbitraryDefaultTime = base::Time::FromDoubleT(25);
const syncer::SyncFirstSetupCompleteSource kSetSourceFromTest =
    syncer::SyncFirstSetupCompleteSource::BASIC_FLOW;

template <class T>
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
    result_ = std::move(static_cast<WDResult<T>*>(result.get())->GetValue());
    run_loop_.Quit();
  }

  void Wait() { run_loop_.Run(); }

  T& result() { return result_; }

 private:
  base::RunLoop run_loop_;
  T result_;
};

#if !BUILDFLAG(IS_CHROMEOS_ASH)
std::vector<std::unique_ptr<CreditCard>> GetServerCards(
    scoped_refptr<autofill::AutofillWebDataService> service) {
  AutofillWebDataServiceConsumer<std::vector<std::unique_ptr<CreditCard>>>
      consumer;
  service->GetServerCreditCards(&consumer);
  consumer.Wait();
  return std::move(consumer.result());
}

std::vector<std::unique_ptr<AutofillProfile>> GetServerProfiles(
    scoped_refptr<autofill::AutofillWebDataService> service) {
  AutofillWebDataServiceConsumer<std::vector<std::unique_ptr<AutofillProfile>>>
      consumer;
  service->GetServerProfiles(&consumer);
  consumer.Wait();
  return std::move(consumer.result());
}

std::unique_ptr<autofill::PaymentsCustomerData> GetPaymentsCustomerData(
    scoped_refptr<autofill::AutofillWebDataService> service) {
  AutofillWebDataServiceConsumer<
      std::unique_ptr<autofill::PaymentsCustomerData>>
      consumer;
  service->GetPaymentsCustomerData(&consumer);
  consumer.Wait();
  return std::move(consumer.result());
}

std::vector<std::unique_ptr<autofill::CreditCardCloudTokenData>>
GetCreditCardCloudTokenData(
    scoped_refptr<autofill::AutofillWebDataService> service) {
  AutofillWebDataServiceConsumer<
      std::vector<std::unique_ptr<CreditCardCloudTokenData>>>
      consumer;
  service->GetCreditCardCloudTokenData(&consumer);
  consumer.Wait();
  return std::move(consumer.result());
}
#endif  // !BUILDFLAG(IS_CHROMEOS_ASH)

// Waits until local changes are committed or an auth error is encountered.
class TestForAuthError : public UpdatedProgressMarkerChecker {
 public:
  explicit TestForAuthError(syncer::SyncServiceImpl* service)
      : UpdatedProgressMarkerChecker(service) {}

  // StatusChangeChecker implementation.
  bool IsExitConditionSatisfied(std::ostream* os) override {
    *os << "Waiting for auth error";
    return service()->GetAuthError() !=
               GoogleServiceAuthError::AuthErrorNone() ||
           UpdatedProgressMarkerChecker::IsExitConditionSatisfied(os);
  }
};

}  // namespace

class SingleClientWalletSyncTest : public SyncTest {
 public:
  SingleClientWalletSyncTest() : SyncTest(SINGLE_CLIENT) {
    test_clock_.SetNow(kArbitraryDefaultTime);
  }

  SingleClientWalletSyncTest(const SingleClientWalletSyncTest&) = delete;
  SingleClientWalletSyncTest& operator=(const SingleClientWalletSyncTest&) =
      delete;

  ~SingleClientWalletSyncTest() override = default;

 protected:
  void WaitForOnPersonalDataChanged(autofill::PersonalDataManager* pdm) {
    pdm->AddObserver(&personal_data_observer_);
    base::RunLoop run_loop;
    EXPECT_CALL(personal_data_observer_, OnPersonalDataChanged())
        .WillRepeatedly(QuitMessageLoop(&run_loop));
    run_loop.Run();
    pdm->RemoveObserver(&personal_data_observer_);
  }

  void WaitForNumberOfCards(size_t expected_count,
                            autofill::PersonalDataManager* pdm) {
    while (pdm->GetCreditCards().size() != expected_count ||
           pdm->HasPendingQueriesForTesting()) {
      WaitForOnPersonalDataChanged(pdm);
    }
  }

  void WaitForNumberOfServerProfiles(size_t expected_count,
                                     autofill::PersonalDataManager* pdm) {
    while (pdm->GetServerProfiles().size() != expected_count ||
           pdm->HasPendingQueriesForTesting()) {
      WaitForOnPersonalDataChanged(pdm);
    }
  }

  void WaitForPaymentsCustomerData(const std::string& customer_id,
                                   autofill::PersonalDataManager* pdm) {
    while (pdm->GetPaymentsCustomerData() == nullptr ||
           pdm->GetPaymentsCustomerData()->customer_id != customer_id ||
           pdm->HasPendingQueriesForTesting()) {
      WaitForOnPersonalDataChanged(pdm);
    }
  }

  void WaitForCreditCardCloudTokenData(size_t expected_count,
                                       autofill::PersonalDataManager* pdm) {
    while (pdm->GetCreditCardCloudTokenData().size() != expected_count) {
      WaitForOnPersonalDataChanged(pdm);
    }
  }

  bool TriggerGetUpdatesAndWait() {
    const base::Time now = base::Time::Now();
    // Trigger a sync and wait for the new data to arrive.
    TriggerSyncForModelTypes(0, {syncer::AUTOFILL_WALLET_DATA});
    return FullUpdateTypeProgressMarkerChecker(now, GetSyncService(0),
                                               syncer::AUTOFILL_WALLET_DATA)
        .Wait();
  }

  void AdvanceAutofillClockByOneDay() { test_clock_.Advance(base::Days(1)); }

  testing::NiceMock<PersonalDataLoadedObserverMock> personal_data_observer_;
  base::HistogramTester histogram_tester_;
  autofill::TestAutofillClock test_clock_;
};

// ChromeOS does not support late signin after profile creation, so the test
// below does not apply, at least in the current form.
#if !BUILDFLAG(IS_CHROMEOS_ASH)
IN_PROC_BROWSER_TEST_F(SingleClientWalletSyncTest,
                       DownloadAccountStorage_Card) {
  ASSERT_TRUE(SetupClients());
  autofill::PersonalDataManager* pdm = GetPersonalDataManager(0);

  GetFakeServer()->SetWalletData({CreateDefaultSyncWalletAddress(),
                                  CreateDefaultSyncWalletCard(),
                                  CreateDefaultSyncPaymentsCustomerData()});

  ASSERT_TRUE(GetClient(0)->SignInPrimaryAccount());
  ASSERT_TRUE(GetClient(0)->AwaitEngineInitialization());
  ASSERT_TRUE(AwaitQuiescence());
  ASSERT_EQ(syncer::SyncService::TransportState::ACTIVE,
            GetSyncService(0)->GetTransportState());
  ASSERT_TRUE(GetSyncService(0)->GetActiveDataTypes().Has(
      syncer::AUTOFILL_WALLET_DATA));

  scoped_refptr<autofill::AutofillWebDataService> profile_data =
      GetProfileWebDataService(0);
  ASSERT_NE(nullptr, profile_data);
  scoped_refptr<autofill::AutofillWebDataService> account_data =
      GetAccountWebDataService(0);
  ASSERT_NE(nullptr, account_data);

  // Check that no data is stored in the profile storage.
  EXPECT_EQ(0U, GetServerCards(profile_data).size());
  EXPECT_EQ(0U, GetServerProfiles(profile_data).size());

  // Check that one card is stored in the account storage.
  EXPECT_EQ(1U, GetServerCards(account_data).size());

  ASSERT_NE(nullptr, pdm);
  std::vector<CreditCard*> cards = pdm->GetCreditCards();
  ASSERT_EQ(1uL, cards.size());

  ExpectDefaultCreditCardValues(*cards[0]);

  GetClient(0)->SignOutPrimaryAccount();

  // Verify that sync is stopped.
  ASSERT_EQ(syncer::SyncService::TransportState::DISABLED,
            GetSyncService(0)->GetTransportState());
  ASSERT_FALSE(GetSyncService(0)->GetActiveDataTypes().Has(
      syncer::AUTOFILL_WALLET_DATA));

  // Wait for PDM to receive the data change with no cards.
  WaitForNumberOfCards(0, pdm);

  // Check directly in the DB that the account storage is now cleared.
  EXPECT_EQ(0U, GetServerCards(account_data).size());
}

// Wallet data should get cleared from the database when the user signs out and
// different data should get downstreamed when the user signs in with a
// different account.
IN_PROC_BROWSER_TEST_F(SingleClientWalletSyncTest,
                       ClearOnSignOutAndDownstreamOnSignIn) {
  ASSERT_TRUE(SetupClients());
  autofill::PersonalDataManager* pdm = GetPersonalDataManager(0);
  ASSERT_NE(nullptr, pdm);

  GetFakeServer()->SetWalletData(
      {CreateSyncWalletCard(/*name=*/"card-1", /*last_four=*/"0001",
                            kDefaultBillingAddressID),
       CreateSyncWalletAddress(/*name=*/"address-1", /*company=*/"Company-1"),
       CreateDefaultSyncPaymentsCustomerData(),
       CreateSyncCreditCardCloudTokenData(/*cloud_token_data_id=*/"data-1")});

  ASSERT_TRUE(GetClient(0)->SignInPrimaryAccount());
  ASSERT_TRUE(GetClient(0)->AwaitEngineInitialization());
  ASSERT_TRUE(AwaitQuiescence());

  // Make sure the data & metadata is in the DB.
  WaitForNumberOfCards(1, pdm);
  WaitForNumberOfServerProfiles(1, pdm);
  WaitForPaymentsCustomerData(kDefaultCustomerID, pdm);
  WaitForCreditCardCloudTokenData(1, pdm);

  // Signout, the data & metadata should be gone.
  GetClient(0)->SignOutPrimaryAccount();
  WaitForNumberOfCards(0, pdm);

  // Server profiles require waiting for a little longer. This is because
  // WaitForNumberOfCards() completes synchronously upon signout, as
  // IsAutofillWalletImportEnabled() becomes false instantly.
  WaitForNumberOfServerProfiles(0, pdm);

  EXPECT_EQ(0uL, pdm->GetServerProfiles().size());
  EXPECT_EQ(0uL, pdm->GetCreditCards().size());
  EXPECT_EQ(nullptr, pdm->GetPaymentsCustomerData());
  EXPECT_EQ(0uL, pdm->GetCreditCardCloudTokenData().size());
  EXPECT_EQ(0U, GetServerCardsMetadata(0).size());
  EXPECT_EQ(0U, GetServerAddressesMetadata(0).size());

  // Set a different set of cards on the server, then sign in again (this is a
  // good enough approximation of signing in with a different Google account).
  GetFakeServer()->SetWalletData(
      {CreateSyncWalletCard(/*name=*/"new-card", /*last_four=*/"0002",
                            kDefaultBillingAddressID),
       CreateSyncWalletAddress(/*name=*/"new-address", /*company=*/"Company-2"),
       CreateSyncPaymentsCustomerData(/*customer_id=*/"different"),
       CreateSyncCreditCardCloudTokenData(/*cloud_token_data_id=*/"data-2")});
  ASSERT_TRUE(GetClient(0)->SignInPrimaryAccount());

  WaitForNumberOfCards(1, pdm);

  // Make sure the data is in the DB.
  EXPECT_EQ(u"0002", pdm->GetCreditCards()[0]->LastFourDigits());
  ASSERT_EQ(1uL, pdm->GetServerProfiles().size());
  std::vector<AutofillProfile*> profiles = pdm->GetServerProfiles();
  ASSERT_EQ(1uL, profiles.size());
  EXPECT_EQ("Company-2", TruncateUTF8(base::UTF16ToUTF8(
                             profiles[0]->GetRawInfo(autofill::COMPANY_NAME))));
  ASSERT_EQ("different", pdm->GetPaymentsCustomerData()->customer_id);
  ASSERT_EQ(1uL, pdm->GetCreditCardCloudTokenData().size());
  EXPECT_EQ("data-2", pdm->GetCreditCardCloudTokenData()[0]->instrument_token);
}
#endif  // !BUILDFLAG(IS_CHROMEOS_ASH)

IN_PROC_BROWSER_TEST_F(SingleClientWalletSyncTest, EnabledByDefault) {
  ASSERT_TRUE(SetupSync());
  ASSERT_TRUE(GetClient(0)->service()->GetActiveDataTypes().Has(
      syncer::AUTOFILL_WALLET_DATA));
  // TODO(pvalenzuela): Assert that the local root node for AUTOFILL_WALLET_DATA
  // exists.
  ASSERT_TRUE(GetClient(0)->service()->GetActiveDataTypes().Has(
      syncer::AUTOFILL_WALLET_METADATA));
}

// Wallet data should get cleared from the database when sync is disabled.
IN_PROC_BROWSER_TEST_F(SingleClientWalletSyncTest, ClearOnDisableSync) {
  GetFakeServer()->SetWalletData({CreateDefaultSyncWalletAddress(),
                                  CreateDefaultSyncWalletCard(),
                                  CreateDefaultSyncPaymentsCustomerData(),
                                  CreateDefaultSyncCreditCardCloudTokenData()});
  ASSERT_TRUE(SetupSync());

  autofill::PersonalDataManager* pdm = GetPersonalDataManager(0);
  ASSERT_NE(nullptr, pdm);

  // Make sure the data & metadata is in the DB.
  ASSERT_EQ(1uL, pdm->GetServerProfiles().size());
  ASSERT_EQ(1uL, pdm->GetCreditCards().size());
  ASSERT_EQ(1uL, pdm->GetCreditCardCloudTokenData().size());
  ASSERT_EQ(kDefaultCustomerID, pdm->GetPaymentsCustomerData()->customer_id);
  ASSERT_EQ(1U, GetServerCardsMetadata(0).size());
  ASSERT_EQ(1U, GetServerAddressesMetadata(0).size());

  // Disable sync, the data & metadata should be gone.
  GetSyncService(0)->StopAndClear();
  WaitForNumberOfCards(0, pdm);

  EXPECT_EQ(0uL, pdm->GetServerProfiles().size());
  EXPECT_EQ(0uL, pdm->GetCreditCards().size());
  EXPECT_EQ(nullptr, pdm->GetPaymentsCustomerData());
  EXPECT_EQ(0uL, pdm->GetCreditCardCloudTokenData().size());
  EXPECT_EQ(0U, GetServerCardsMetadata(0).size());
  EXPECT_EQ(0U, GetServerAddressesMetadata(0).size());

  // Turn sync on again, the data should come back.
  GetSyncService(0)->SetSyncFeatureRequested();
  // StopAndClear() also clears the "first setup complete" flag, so set it
  // again.
  GetSyncService(0)->GetUserSettings()->SetInitialSyncFeatureSetupComplete(
      kSetSourceFromTest);
  // Wait until Sync restores the card and it arrives at PDM.
  WaitForNumberOfCards(1, pdm);

  EXPECT_EQ(1uL, pdm->GetServerProfiles().size());
  EXPECT_EQ(1uL, pdm->GetCreditCards().size());
  EXPECT_EQ(kDefaultCustomerID, pdm->GetPaymentsCustomerData()->customer_id);
  EXPECT_EQ(1uL, pdm->GetCreditCardCloudTokenData().size());
  EXPECT_EQ(1U, GetServerCardsMetadata(0).size());
  EXPECT_EQ(1U, GetServerAddressesMetadata(0).size());
}

// ChromeOS does not sign out, so the test below does not apply.
#if !BUILDFLAG(IS_CHROMEOS_ASH)
IN_PROC_BROWSER_TEST_F(SingleClientWalletSyncTest, ClearOnSignOut) {
  GetFakeServer()->SetWalletData({CreateDefaultSyncWalletAddress(),
                                  CreateDefaultSyncWalletCard(),
                                  CreateDefaultSyncPaymentsCustomerData(),
                                  CreateDefaultSyncCreditCardCloudTokenData()});
  ASSERT_TRUE(SetupSync());

  autofill::PersonalDataManager* pdm = GetPersonalDataManager(0);
  ASSERT_NE(nullptr, pdm);

  // Make sure the data & metadata is in the DB.
  ASSERT_EQ(1uL, pdm->GetServerProfiles().size());
  ASSERT_EQ(1uL, pdm->GetCreditCards().size());
  ASSERT_EQ(kDefaultCustomerID, pdm->GetPaymentsCustomerData()->customer_id);
  ASSERT_EQ(1uL, pdm->GetCreditCardCloudTokenData().size());
  ASSERT_EQ(1U, GetServerCardsMetadata(0).size());
  ASSERT_EQ(1U, GetServerAddressesMetadata(0).size());

  // Signout, the data & metadata should be gone.
  GetClient(0)->SignOutPrimaryAccount();
  WaitForNumberOfCards(0, pdm);

  EXPECT_EQ(0uL, pdm->GetServerProfiles().size());
  EXPECT_EQ(0uL, pdm->GetCreditCards().size());
  EXPECT_EQ(nullptr, pdm->GetPaymentsCustomerData());
  EXPECT_EQ(0uL, pdm->GetCreditCardCloudTokenData().size());
  EXPECT_EQ(0U, GetServerCardsMetadata(0).size());
  EXPECT_EQ(0U, GetServerAddressesMetadata(0).size());
}
#endif  // !BUILDFLAG(IS_CHROMEOS_ASH)

// Wallet data should get cleared from the database when the user enters the
// sync paused state (e.g. persistent auth error).
IN_PROC_BROWSER_TEST_F(SingleClientWalletSyncTest, ClearOnSyncPaused) {
  GetFakeServer()->SetWalletData({CreateDefaultSyncWalletAddress(),
                                  CreateDefaultSyncWalletCard(),
                                  CreateDefaultSyncPaymentsCustomerData(),
                                  CreateDefaultSyncCreditCardCloudTokenData()});
  ASSERT_TRUE(SetupSync());

  autofill::PersonalDataManager* pdm = GetPersonalDataManager(0);
  ASSERT_NE(nullptr, pdm);

  // Make sure the data & metadata is in the DB.
  ASSERT_EQ(1uL, pdm->GetServerProfiles().size());
  ASSERT_EQ(1uL, pdm->GetCreditCards().size());
  ASSERT_EQ(kDefaultCustomerID, pdm->GetPaymentsCustomerData()->customer_id);
  ASSERT_EQ(1uL, pdm->GetCreditCardCloudTokenData().size());
  ASSERT_EQ(1U, GetServerCardsMetadata(0).size());
  ASSERT_EQ(1U, GetServerAddressesMetadata(0).size());

  // Enter sync paused state, the data & metadata should be gone.
  GetClient(0)->EnterSyncPausedStateForPrimaryAccount();
  WaitForNumberOfCards(0, pdm);

  EXPECT_EQ(0uL, pdm->GetServerProfiles().size());
  EXPECT_EQ(0uL, pdm->GetCreditCards().size());
  EXPECT_EQ(nullptr, pdm->GetPaymentsCustomerData());
  EXPECT_EQ(0uL, pdm->GetCreditCardCloudTokenData().size());
  EXPECT_EQ(0U, GetServerCardsMetadata(0).size());
  EXPECT_EQ(0U, GetServerAddressesMetadata(0).size());

  GetClient(0)->ExitSyncPausedStateForPrimaryAccount();
  WaitForNumberOfCards(1, pdm);
  ASSERT_EQ(1uL, pdm->GetServerProfiles().size());
  ASSERT_EQ(1uL, pdm->GetCreditCards().size());
  ASSERT_EQ(kDefaultCustomerID, pdm->GetPaymentsCustomerData()->customer_id);
  ASSERT_EQ(1uL, pdm->GetCreditCardCloudTokenData().size());
  ASSERT_EQ(1U, GetServerCardsMetadata(0).size());
  ASSERT_EQ(1U, GetServerAddressesMetadata(0).size());
}

// Wallet is not using incremental updates. Make sure existing data gets
// replaced when synced down.
IN_PROC_BROWSER_TEST_F(SingleClientWalletSyncTest,
                       NewSyncDataShouldReplaceExistingData) {
  GetFakeServer()->SetWalletData(
      {CreateSyncWalletCard(/*name=*/"card-1", /*last_four=*/"0001",
                            kDefaultBillingAddressID, /*nickname=*/"",
                            /*instrument_id=*/123),
       CreateSyncWalletAddress(/*name=*/"address-1", /*company=*/"Company-1"),
       CreateDefaultSyncPaymentsCustomerData(),
       CreateSyncCreditCardCloudTokenData(/*cloud_token_data_id=*/"data-1")});
  ASSERT_TRUE(SetupSync());

  // Make sure the data is in the DB.
  autofill::PersonalDataManager* pdm = GetPersonalDataManager(0);
  ASSERT_NE(nullptr, pdm);
  std::vector<CreditCard*> cards = pdm->GetCreditCards();
  ASSERT_EQ(1uL, cards.size());
  EXPECT_EQ(u"0001", cards[0]->LastFourDigits());
  EXPECT_EQ(123, cards[0]->instrument_id());
  // When no nickname is returned from Sync server, credit card's nickname is
  // empty.
  EXPECT_TRUE(cards[0]->nickname().empty());
  std::vector<AutofillProfile*> profiles = pdm->GetServerProfiles();
  ASSERT_EQ(1uL, profiles.size());
  EXPECT_EQ("Company-1", TruncateUTF8(base::UTF16ToUTF8(
                             profiles[0]->GetRawInfo(autofill::COMPANY_NAME))));
  EXPECT_EQ(kDefaultCustomerID, pdm->GetPaymentsCustomerData()->customer_id);
  std::vector<CreditCardCloudTokenData*> cloud_token_data =
      pdm->GetCreditCardCloudTokenData();
  ASSERT_EQ(1uL, cloud_token_data.size());
  EXPECT_EQ("data-1", cloud_token_data[0]->instrument_token);

  // Put some completely new data in the sync server.
  GetFakeServer()->SetWalletData(
      {CreateSyncWalletCard(/*name=*/"new-card", /*last_four=*/"0002",
                            kDefaultBillingAddressID,
                            /*nickname=*/"Grocery Card", /*instrument_id=*/321),
       CreateSyncWalletAddress(/*name=*/"new-address", /*company=*/"Company-2"),
       CreateSyncPaymentsCustomerData(/*customer_id=*/"different"),
       CreateSyncCreditCardCloudTokenData(/*cloud_token_data_id=*/"data-2")});

  WaitForPaymentsCustomerData(/*customer_id=*/"different", pdm);

  // Make sure only the new data is present.
  cards = pdm->GetCreditCards();
  ASSERT_EQ(1uL, cards.size());
  EXPECT_EQ(u"0002", cards[0]->LastFourDigits());
  EXPECT_EQ(321, cards[0]->instrument_id());
  EXPECT_EQ(u"Grocery Card", cards[0]->nickname());
  profiles = pdm->GetServerProfiles();
  ASSERT_EQ(1uL, profiles.size());
  EXPECT_EQ("Company-2", TruncateUTF8(base::UTF16ToUTF8(
                             profiles[0]->GetRawInfo(autofill::COMPANY_NAME))));
  cloud_token_data = pdm->GetCreditCardCloudTokenData();
  ASSERT_EQ(1uL, cloud_token_data.size());
  EXPECT_EQ("data-2", cloud_token_data[0]->instrument_token);
}

// Wallet is not using incremental updates. The server either sends a non-empty
// update with deletion gc directives and with the (possibly empty) full data
// set, or (more often) an empty update.
IN_PROC_BROWSER_TEST_F(SingleClientWalletSyncTest, EmptyUpdatesAreIgnored) {
  GetFakeServer()->SetWalletData(
      {CreateSyncWalletCard(/*name=*/"card-1", /*last_four=*/"0001",
                            kDefaultBillingAddressID),
       CreateSyncWalletAddress(/*name=*/"address-1", /*company=*/"Company-1"),
       CreateDefaultSyncPaymentsCustomerData(),
       CreateSyncCreditCardCloudTokenData("data-1")});
  ASSERT_TRUE(SetupSync());

  // Make sure the card is in the DB.
  autofill::PersonalDataManager* pdm = GetPersonalDataManager(0);
  ASSERT_NE(nullptr, pdm);
  std::vector<CreditCard*> cards = pdm->GetCreditCards();
  ASSERT_EQ(1uL, cards.size());
  EXPECT_EQ(u"0001", cards[0]->LastFourDigits());
  std::vector<AutofillProfile*> profiles = pdm->GetServerProfiles();
  ASSERT_EQ(1uL, profiles.size());
  EXPECT_EQ("Company-1", TruncateUTF8(base::UTF16ToUTF8(
                             profiles[0]->GetRawInfo(autofill::COMPANY_NAME))));
  EXPECT_EQ(kDefaultCustomerID, pdm->GetPaymentsCustomerData()->customer_id);
  std::vector<CreditCardCloudTokenData*> cloud_token_data =
      pdm->GetCreditCardCloudTokenData();
  ASSERT_EQ(1uL, cloud_token_data.size());
  EXPECT_EQ("data-1", cloud_token_data[0]->instrument_token);

  // Trigger a sync and wait for the new data to arrive.
  sync_pb::ModelTypeState state_before =
      GetWalletModelTypeState(syncer::AUTOFILL_WALLET_DATA, 0);
  ASSERT_TRUE(TriggerGetUpdatesAndWait());

  // Check that the new progress marker is stored for empty updates. This is a
  // regression check for crbug.com/924447.
  sync_pb::ModelTypeState state_after =
      GetWalletModelTypeState(syncer::AUTOFILL_WALLET_DATA, 0);
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
  cards = pdm->GetCreditCards();
  ASSERT_EQ(1uL, cards.size());
  EXPECT_EQ(u"0001", cards[0]->LastFourDigits());
  profiles = pdm->GetServerProfiles();
  ASSERT_EQ(1uL, profiles.size());
  EXPECT_EQ("Company-1", TruncateUTF8(base::UTF16ToUTF8(
                             profiles[0]->GetRawInfo(autofill::COMPANY_NAME))));
  EXPECT_EQ(kDefaultCustomerID, pdm->GetPaymentsCustomerData()->customer_id);
  cloud_token_data = pdm->GetCreditCardCloudTokenData();
  ASSERT_EQ(1uL, cloud_token_data.size());
  EXPECT_EQ("data-1", cloud_token_data[0]->instrument_token);
}

// If the server sends the same cards and addresses again, they should not
// change on the client. We should also not overwrite existing metadata.
IN_PROC_BROWSER_TEST_F(SingleClientWalletSyncTest, SameUpdatesAreIgnored) {
  GetFakeServer()->SetWalletData(
      {CreateSyncWalletCard(/*name=*/"card-1", /*last_four=*/"0001",
                            kDefaultBillingAddressID),
       CreateSyncWalletAddress(/*name=*/"address-1", /*company=*/"Company-1"),
       CreateDefaultSyncPaymentsCustomerData(),
       CreateSyncCreditCardCloudTokenData("data-1")});
  ASSERT_TRUE(SetupSync());

  // Record use of to get non-default metadata values.
  autofill::PersonalDataManager* pdm = GetPersonalDataManager(0);
  std::vector<CreditCard*> cards = pdm->GetCreditCards();
  ASSERT_EQ(1uL, cards.size());
  pdm->RecordUseOf(cards[0]);
  std::vector<AutofillProfile*> profiles = pdm->GetServerProfiles();
  ASSERT_EQ(1uL, profiles.size());
  // TODO(crbug.com/941498): Server profiles are not recorded.
  pdm->RecordUseOf(profiles[0]);

  // Keep the same data (only change the customer data and the cloud token to
  // force the FakeServer to send the full update).
  GetFakeServer()->SetWalletData(
      {CreateSyncWalletCard(/*name=*/"card-1", /*last_four=*/"0001",
                            kDefaultBillingAddressID),
       CreateSyncWalletAddress(/*name=*/"address-1", /*company=*/"Company-1"),
       CreateSyncPaymentsCustomerData("different"),
       CreateSyncCreditCardCloudTokenData("data-2")});

  WaitForPaymentsCustomerData(/*customer_id=*/"different", pdm);

  // Make sure the data is present on the client.
  cards = pdm->GetCreditCards();
  ASSERT_EQ(1uL, cards.size());
  EXPECT_EQ(u"0001", cards[0]->LastFourDigits());
  profiles = pdm->GetServerProfiles();
  ASSERT_EQ(1uL, profiles.size());
  EXPECT_EQ("Company-1", TruncateUTF8(base::UTF16ToUTF8(
                             profiles[0]->GetRawInfo(autofill::COMPANY_NAME))));
  std::vector<CreditCardCloudTokenData*> cloud_token_data =
      pdm->GetCreditCardCloudTokenData();
  ASSERT_EQ(1uL, cloud_token_data.size());
  EXPECT_EQ("data-2", cloud_token_data[0]->instrument_token);

  // Test that the non-default metadata values stayed around.
  std::map<std::string, AutofillMetadata> cards_metadata =
      GetServerCardsMetadata(0);
  EXPECT_EQ(1U, cards_metadata.size());
  EXPECT_EQ(2U, cards_metadata.begin()->second.use_count);
  std::map<std::string, AutofillMetadata> addresses_metadata =
      GetServerAddressesMetadata(0);
  EXPECT_EQ(1U, addresses_metadata.size());
  // TODO(crbug.com/941498): Once RecordUseOf() is fixed for server addresses,
  // change this expectation back to 2.
  EXPECT_EQ(1U, addresses_metadata.begin()->second.use_count);
}

// If the server sends the same cards and addresses with changed data, they
// should change on the client.
IN_PROC_BROWSER_TEST_F(SingleClientWalletSyncTest, ChangedEntityGetsUpdated) {
  GetFakeServer()->SetWalletData(
      {CreateSyncWalletCard(/*name=*/"card-1", /*last_four=*/"0002",
                            kDefaultBillingAddressID, /*nickname=*/"Outdated"),
       CreateSyncWalletAddress(/*name=*/"address-1", /*company=*/"Company-2"),
       CreateDefaultSyncPaymentsCustomerData(),
       CreateSyncCreditCardCloudTokenData("data-1")});
  ASSERT_TRUE(SetupSync());

  // Record use of to get non-default metadata values.
  autofill::PersonalDataManager* pdm = GetPersonalDataManager(0);
  std::vector<CreditCard*> cards = pdm->GetCreditCards();
  ASSERT_EQ(1uL, cards.size());
  pdm->RecordUseOf(cards[0]);
  std::vector<AutofillProfile*> profiles = pdm->GetServerProfiles();
  ASSERT_EQ(1uL, profiles.size());
  // TODO(crbug.com/941498): Server profiles are not recorded.
  pdm->RecordUseOf(profiles[0]);

  // Update the data (also change the customer data to force the full update as
  // FakeServer computes the hash for progress markers only based on ids). For
  // server card, only the card's nickname is changed, metadata is unchanged.
  GetFakeServer()->SetWalletData(
      {CreateSyncWalletCard(/*name=*/"card-1", /*last_four=*/"0002",
                            kDefaultBillingAddressID,
                            /*nickname=*/"Grocery Card"),
       CreateSyncWalletAddress(/*name=*/"address-1", /*company=*/"Company-1"),
       CreateSyncPaymentsCustomerData("different"),
       CreateSyncCreditCardCloudTokenData("data-2")});

  WaitForPaymentsCustomerData(/*customer_id=*/"different", pdm);

  // Make sure the data is present on the client.
  cards = pdm->GetCreditCards();
  ASSERT_EQ(1uL, cards.size());
  EXPECT_EQ(u"Grocery Card", cards[0]->nickname());
  profiles = pdm->GetServerProfiles();
  ASSERT_EQ(1uL, profiles.size());
  EXPECT_EQ("Company-1", TruncateUTF8(base::UTF16ToUTF8(
                             profiles[0]->GetRawInfo(autofill::COMPANY_NAME))));
  std::vector<CreditCardCloudTokenData*> cloud_token_data =
      pdm->GetCreditCardCloudTokenData();
  ASSERT_EQ(1uL, cloud_token_data.size());
  EXPECT_EQ("data-2", cloud_token_data[0]->instrument_token);

  // Test that the non-default metadata values stayed around.
  std::map<std::string, AutofillMetadata> cards_metadata =
      GetServerCardsMetadata(0);
  EXPECT_EQ(1U, cards_metadata.size());
  EXPECT_EQ(2U, cards_metadata.begin()->second.use_count);
  std::map<std::string, AutofillMetadata> addresses_metadata =
      GetServerAddressesMetadata(0);
  EXPECT_EQ(1U, addresses_metadata.size());
  // Metadata for server addresses cannot be preserved because the id changes
  // since it is generated on the client as a hash of all the data fields. As a
  // result, the metadata entry gets deleted and recreated under a different id
  // and has a default use count.
  EXPECT_EQ(1U, addresses_metadata.begin()->second.use_count);
}

// If the server sends the same cards again, they should not change on the
// client even if the cards on the client are unmasked.
IN_PROC_BROWSER_TEST_F(SingleClientWalletSyncTest,
                       SameUpdatesAreIgnoredWhenLocalCardsUnmasked) {
  GetFakeServer()->SetWalletData(
      {CreateSyncWalletCard(/*name=*/"card-1", /*last_four=*/"0001",
                            kDefaultBillingAddressID),
       CreateDefaultSyncPaymentsCustomerData()});
  ASSERT_TRUE(SetupSync());

  // Check we have the card locally.
  autofill::PersonalDataManager* pdm = GetPersonalDataManager(0);
  ASSERT_NE(nullptr, pdm);
  std::vector<CreditCard*> cards = pdm->GetCreditCards();
  ASSERT_EQ(1uL, cards.size());
  EXPECT_EQ(u"0001", cards[0]->LastFourDigits());
  EXPECT_EQ(CreditCard::MASKED_SERVER_CARD, cards[0]->record_type());

  // Unmask the card (the full card number has to start with "34" to match the
  // type of the masked card which is by default AMEX in the tests).
  UnmaskServerCard(0, *cards[0], u"3404000300020001");

  // Keep the same data (only change the customer data to force the FakeServer
  // to send the full update).
  GetFakeServer()->SetWalletData(
      {CreateSyncWalletCard(/*name=*/"card-1", /*last_four=*/"0001",
                            kDefaultBillingAddressID),
       CreateSyncPaymentsCustomerData("different")});

  WaitForPaymentsCustomerData(/*customer_id=*/"different", pdm);

  // Make sure the data is present on the client.
  cards = pdm->GetCreditCards();
  ASSERT_EQ(1uL, cards.size());
  EXPECT_EQ(u"0001", cards[0]->LastFourDigits());
  EXPECT_EQ(CreditCard::FULL_SERVER_CARD, cards[0]->record_type());
  EXPECT_EQ("different", pdm->GetPaymentsCustomerData()->customer_id);
}

// Wallet data should get cleared from the database when the wallet sync type
// flag is disabled.
IN_PROC_BROWSER_TEST_F(SingleClientWalletSyncTest, ClearOnDisableWalletSync) {
  GetFakeServer()->SetWalletData({CreateDefaultSyncWalletAddress(),
                                  CreateDefaultSyncWalletCard(),
                                  CreateDefaultSyncPaymentsCustomerData(),
                                  CreateDefaultSyncCreditCardCloudTokenData()});
  ASSERT_TRUE(SetupSync());

  autofill::PersonalDataManager* pdm = GetPersonalDataManager(0);
  ASSERT_NE(nullptr, pdm);

  // Make sure the data & metadata is in the DB.
  ASSERT_EQ(1uL, pdm->GetServerProfiles().size());
  ASSERT_EQ(1uL, pdm->GetCreditCards().size());
  ASSERT_EQ(kDefaultCustomerID, pdm->GetPaymentsCustomerData()->customer_id);
  ASSERT_EQ(1uL, pdm->GetCreditCardCloudTokenData().size());
  ASSERT_EQ(1U, GetServerCardsMetadata(0).size());
  ASSERT_EQ(1U, GetServerAddressesMetadata(0).size());

  // Turn off payments sync, the data & metadata should be gone.
  ASSERT_TRUE(
      GetClient(0)->DisableSyncForType(syncer::UserSelectableType::kPayments));

  WaitForNumberOfCards(0, pdm);

  // Server profiles require waiting for a little longer. This is because
  // WaitForNumberOfCards() completes synchronously upon disabling payments, as
  // IsAutofillWalletImportEnabled() becomes false instantly.
  WaitForNumberOfServerProfiles(0, pdm);

  EXPECT_EQ(0uL, pdm->GetServerProfiles().size());
  EXPECT_EQ(0uL, pdm->GetCreditCards().size());
  EXPECT_EQ(nullptr, pdm->GetPaymentsCustomerData());
  EXPECT_EQ(0uL, pdm->GetCreditCardCloudTokenData().size());
  EXPECT_EQ(0U, GetServerCardsMetadata(0).size());
  EXPECT_EQ(0U, GetServerAddressesMetadata(0).size());
}

// Wallet data should get cleared from the database when the wallet autofill
// integration flag is disabled.
IN_PROC_BROWSER_TEST_F(SingleClientWalletSyncTest,
                       ClearOnDisableWalletAutofill) {
  GetFakeServer()->SetWalletData({CreateDefaultSyncWalletAddress(),
                                  CreateDefaultSyncWalletCard(),
                                  CreateDefaultSyncPaymentsCustomerData(),
                                  CreateDefaultSyncCreditCardCloudTokenData()});
  ASSERT_TRUE(SetupSync());

  autofill::PersonalDataManager* pdm = GetPersonalDataManager(0);
  ASSERT_NE(nullptr, pdm);

  // Make sure the data & metadata is in the DB.
  ASSERT_EQ(1uL, pdm->GetServerProfiles().size());
  ASSERT_EQ(1uL, pdm->GetCreditCards().size());
  ASSERT_EQ(kDefaultCustomerID, pdm->GetPaymentsCustomerData()->customer_id);
  ASSERT_EQ(1uL, pdm->GetCreditCardCloudTokenData().size());
  ASSERT_EQ(1U, GetServerCardsMetadata(0).size());
  ASSERT_EQ(1U, GetServerAddressesMetadata(0).size());

  // Turn off the wallet autofill pref, the data & metadata should be gone as a
  // side effect of the wallet model type controller noticing.
  GetSyncService(0)->GetUserSettings()->SetSelectedTypes(
      /*sync_everything=*/false, /*types=*/{});

  // It's not sufficient to wait for 0 cards (as in other tests) because
  // switching off the pref makes PDM (synchronously) return 0 cards (without
  // the other changes in the DB going through).
  WaitForNumberOfServerProfiles(0, pdm);

  EXPECT_EQ(0uL, pdm->GetCreditCards().size());
  EXPECT_EQ(nullptr, pdm->GetPaymentsCustomerData());
  EXPECT_EQ(0uL, pdm->GetCreditCardCloudTokenData().size());
  EXPECT_EQ(0U, GetServerCardsMetadata(0).size());
  EXPECT_EQ(0U, GetServerAddressesMetadata(0).size());
}

// Wallet data present on the client should be cleared in favor of the new data
// synced down form the server.
IN_PROC_BROWSER_TEST_F(SingleClientWalletSyncTest,
                       NewWalletCardRemovesExistingCardAndProfile) {
  ASSERT_TRUE(SetupSync());
  autofill::PersonalDataManager* pdm = GetPersonalDataManager(0);
  ASSERT_NE(nullptr, pdm);

  // Add a server credit card on the client.
  CreditCard credit_card(CreditCard::MASKED_SERVER_CARD, "a123");
  std::vector<CreditCard> credit_cards = {credit_card};
  wallet_helper::SetServerCreditCards(0, credit_cards);

  // Add a server profile on the client.
  AutofillProfile profile(AutofillProfile::SERVER_PROFILE, "a123");
  profile.SetRawInfo(autofill::COMPANY_NAME, u"JustATest");
  std::vector<AutofillProfile> client_profiles = {profile};
  wallet_helper::SetServerProfiles(0, client_profiles);

  // Add PaymentsCustomerData on the client.
  wallet_helper::SetPaymentsCustomerData(
      0, autofill::PaymentsCustomerData(/*customer_id=*/kDefaultCustomerID));

  // Add a cloud token data on the client.
  CreditCardCloudTokenData data;
  data.instrument_token = "data-1";
  wallet_helper::SetCreditCardCloudTokenData(0, {data});

  // Wait for the pdm to get data from the autofill table (the manual changes
  // above don't notify pdm to refresh automatically).
  pdm->Refresh();
  WaitForNumberOfCards(1, pdm);

  // Make sure the card was added correctly.
  std::vector<CreditCard*> cards = pdm->GetCreditCards();
  ASSERT_EQ(1uL, cards.size());
  EXPECT_EQ("a123", cards[0]->server_id());

  // Make sure the profile was added correctly.
  std::vector<AutofillProfile*> profiles = pdm->GetServerProfiles();
  ASSERT_EQ(1uL, profiles.size());
  EXPECT_EQ("JustATest", TruncateUTF8(base::UTF16ToUTF8(
                             profiles[0]->GetRawInfo(autofill::COMPANY_NAME))));

  // Make sure the customer data was added correctly.
  EXPECT_EQ(kDefaultCustomerID, pdm->GetPaymentsCustomerData()->customer_id);

  // Make sure the cloud token data was added correctly.
  std::vector<CreditCardCloudTokenData*> cloud_token_data =
      pdm->GetCreditCardCloudTokenData();
  ASSERT_EQ(1uL, cloud_token_data.size());
  EXPECT_EQ("data-1", cloud_token_data[0]->instrument_token);

  // Add new data to the server and sync it down.
  GetFakeServer()->SetWalletData(
      {CreateDefaultSyncWalletCard(),
       CreateSyncPaymentsCustomerData("different"),
       CreateSyncCreditCardCloudTokenData("data-2")});

  WaitForPaymentsCustomerData(/*customer_id=*/"different", pdm);

  // The only card present on the client should be the one from the server.
  cards = pdm->GetCreditCards();
  EXPECT_EQ(1uL, cards.size());
  EXPECT_EQ(kDefaultCardID, cards[0]->server_id());

  // There should be no profile present.
  profiles = pdm->GetServerProfiles();
  EXPECT_EQ(0uL, profiles.size());

  // The only cloud token present on the client should be the one from the
  // server.
  cloud_token_data = pdm->GetCreditCardCloudTokenData();
  ASSERT_EQ(1uL, cloud_token_data.size());
  EXPECT_EQ("data-2", cloud_token_data[0]->instrument_token);
}

// Wallet data present on the client should be cleared in favor of the new data
// synced down form the server.
IN_PROC_BROWSER_TEST_F(SingleClientWalletSyncTest,
                       NewWalletDataRemovesExistingData) {
  ASSERT_TRUE(SetupSync());
  autofill::PersonalDataManager* pdm = GetPersonalDataManager(0);
  ASSERT_NE(nullptr, pdm);

  // Add a server profile on the client.
  AutofillProfile profile(AutofillProfile::SERVER_PROFILE, "a123");
  profile.SetRawInfo(autofill::COMPANY_NAME, u"JustATest");
  std::vector<AutofillProfile> client_profiles = {profile};
  wallet_helper::SetServerProfiles(0, client_profiles);

  // Add a server credit card on the client.
  CreditCard credit_card(CreditCard::MASKED_SERVER_CARD, "a123");
  std::vector<CreditCard> credit_cards = {credit_card};
  wallet_helper::SetServerCreditCards(0, credit_cards);

  // Add PaymentsCustomerData on the client.
  wallet_helper::SetPaymentsCustomerData(
      0, autofill::PaymentsCustomerData(/*customer_id=*/kDefaultCustomerID));

  // Add CreditCardCloudTokenData on the client.
  CreditCardCloudTokenData data;
  data.instrument_token = "data-1";
  wallet_helper::SetCreditCardCloudTokenData(0, {data});

  // Wait for the pdm to get data from the autofill table (the manual changes
  // above don't notify pdm to refresh automatically).
  pdm->Refresh();
  WaitForNumberOfCards(1, pdm);

  // Make sure the profile was added correctly.
  std::vector<AutofillProfile*> profiles = pdm->GetServerProfiles();
  EXPECT_EQ(1uL, profiles.size());
  EXPECT_EQ("JustATest", TruncateUTF8(base::UTF16ToUTF8(
                             profiles[0]->GetRawInfo(autofill::COMPANY_NAME))));

  // Make sure the card was added correctly.
  std::vector<CreditCard*> cards = pdm->GetCreditCards();
  EXPECT_EQ(1uL, cards.size());
  EXPECT_EQ("a123", cards[0]->server_id());

  // Make sure the customer data was added correctly.
  EXPECT_EQ(kDefaultCustomerID, pdm->GetPaymentsCustomerData()->customer_id);

  // Make sure the credit card cloud token data was added correctly.
  std::vector<CreditCardCloudTokenData*> cloud_token_data =
      pdm->GetCreditCardCloudTokenData();
  ASSERT_EQ(1uL, cloud_token_data.size());
  EXPECT_EQ("data-1", cloud_token_data[0]->instrument_token);

  // Add a new profile and new customer data from the server and sync them down.
  GetFakeServer()->SetWalletData(
      {CreateDefaultSyncWalletAddress(),
       CreateSyncPaymentsCustomerData(/*customer_id=*/"different"),
       CreateSyncCreditCardCloudTokenData("data-2")});

  WaitForPaymentsCustomerData(/*customer_id=*/"different", pdm);

  // The only profile present on the client should be the one from the server.
  profiles = pdm->GetServerProfiles();
  EXPECT_EQ(1uL, profiles.size());
  EXPECT_NE("JustATest", TruncateUTF8(base::UTF16ToUTF8(
                             profiles[0]->GetRawInfo(autofill::COMPANY_NAME))));

  // There should be no cards present.
  cards = pdm->GetCreditCards();
  EXPECT_EQ(0uL, cards.size());

  // Credit card cloud token data should be updated.
  cloud_token_data = pdm->GetCreditCardCloudTokenData();
  ASSERT_EQ(1uL, cloud_token_data.size());
  EXPECT_EQ("data-2", cloud_token_data[0]->instrument_token);
}

// Tests that a local billing address id set on a card on the client should not
// be overwritten when that same card is synced again.
IN_PROC_BROWSER_TEST_F(SingleClientWalletSyncTest,
                       SameWalletCard_PreservesLocalBillingAddressId) {
  ASSERT_TRUE(SetupSync());
  autofill::PersonalDataManager* pdm = GetPersonalDataManager(0);
  ASSERT_NE(nullptr, pdm);

  // Add a server credit card on the client but with the billing address id of a
  // local profile.
  CreditCard credit_card = GetDefaultCreditCard();
  credit_card.set_billing_address_id(kLocalGuidA);
  std::vector<CreditCard> credit_cards = {credit_card};
  wallet_helper::SetServerCreditCards(0, credit_cards);

  // Wait for the pdm to get data from the autofill table (the manual changes
  // above don't notify pdm to refresh automatically).
  pdm->Refresh();
  WaitForNumberOfCards(1, pdm);

  // Make sure the card was added correctly.
  std::vector<CreditCard*> cards = pdm->GetCreditCards();
  ASSERT_EQ(1uL, cards.size());
  EXPECT_EQ(kDefaultCardID, cards[0]->server_id());

  // Sync the same card from the server, except with a default billing address
  // id.
  GetFakeServer()->SetWalletData(
      {CreateDefaultSyncWalletCard(),
       CreateSyncPaymentsCustomerData(/*customer_id=*/"different")});

  WaitForPaymentsCustomerData(/*customer_id=*/"different", pdm);

  // The billing address is should still refer to the local profile.
  cards = pdm->GetCreditCards();
  ASSERT_EQ(1uL, cards.size());
  EXPECT_EQ(kDefaultCardID, cards[0]->server_id());
  EXPECT_EQ(kLocalGuidA, cards[0]->billing_address_id());
}

// Tests that a server billing address id set on a card on the client is
// overwritten when that same card is synced again.
IN_PROC_BROWSER_TEST_F(SingleClientWalletSyncTest,
                       SameWalletCard_DiscardsOldServerBillingAddressId) {
  ASSERT_TRUE(SetupSync());
  autofill::PersonalDataManager* pdm = GetPersonalDataManager(0);
  ASSERT_NE(nullptr, pdm);

  // Add a server credit card on the client but with the billing address id of a
  // server profile.
  CreditCard credit_card = GetDefaultCreditCard();
  credit_card.set_billing_address_id(kDifferentBillingAddressId);
  std::vector<CreditCard> credit_cards = {credit_card};
  wallet_helper::SetServerCreditCards(0, credit_cards);

  // Wait for the pdm to get data from the autofill table (the manual changes
  // above don't notify pdm to refresh automatically).
  pdm->Refresh();
  WaitForNumberOfCards(1, pdm);

  // Make sure the card was added correctly.
  std::vector<CreditCard*> cards = pdm->GetCreditCards();
  ASSERT_EQ(1uL, cards.size());
  EXPECT_EQ(kDefaultCardID, cards[0]->server_id());

  // Sync the same card from the server, except with a default billing address
  // id.
  GetFakeServer()->SetWalletData(
      {CreateDefaultSyncWalletCard(),
       CreateSyncPaymentsCustomerData(/*customer_id=*/"different")});

  WaitForPaymentsCustomerData(/*customer_id=*/"different", pdm);

  // The billing address should be the one from the server card.
  cards = pdm->GetCreditCards();
  ASSERT_EQ(1uL, cards.size());
  EXPECT_EQ(kDefaultCardID, cards[0]->server_id());
  EXPECT_EQ(kDefaultBillingAddressID, cards[0]->billing_address_id());
}

IN_PROC_BROWSER_TEST_F(SingleClientWalletSyncTest,
                       DoNotConvertServerAddressAgain) {
  sync_pb::SyncEntity address_entity =
      CreateSyncWalletAddress(/*name=*/"address-1", /*company=*/"Company-1");
  GetFakeServer()->SetWalletData(
      {address_entity, CreateDefaultSyncPaymentsCustomerData()});

  // Inject a wallet metadata entity that indicates the address has already been
  // converted.
  sync_pb::EntitySpecifics specifics;
  sync_pb::WalletMetadataSpecifics* wallet_metadata_specifics =
      specifics.mutable_wallet_metadata();
  wallet_metadata_specifics->set_type(
      sync_pb::WalletMetadataSpecifics::ADDRESS);
  wallet_metadata_specifics->set_use_count(3);
  wallet_metadata_specifics->set_use_date(10);
  wallet_metadata_specifics->set_address_has_converted(true);
  // The id gets generated on the client based on the data. Simulate the process
  // to get the same id. On top of it, we need to base64encode it.
  AutofillProfile address = autofill::ProfileFromSpecifics(
      address_entity.specifics().autofill_wallet().address());
  wallet_metadata_specifics->set_id(
      autofill::GetBase64EncodedId(address.server_id()));

  GetFakeServer()->InjectEntity(
      syncer::PersistentUniqueClientEntity::CreateFromSpecificsForTesting(
          "non_unique_name",
          /*client_tag=*/"address-" + wallet_metadata_specifics->id(),
          specifics,
          /*creation_time=*/0,
          /*last_modified_time=*/0));

  ASSERT_TRUE(SetupSync());

  // Wait to make sure we receive the (already converted) metadata entity.
  AutofillWalletConversionChecker(0).Wait();

  // No conversion happens now.
  histogram_tester_.ExpectTotalCount("Autofill.WalletAddressConversionType",
                                     /*count=*/0);
}

// Regression test for crbug.com/1203984.
IN_PROC_BROWSER_TEST_F(SingleClientWalletSyncTest,
                       ShouldUpdateWhenDownloadingManyUpdates) {
  // Tests that a Wallet update is successfully applied even if there are more
  // updates to download for other types. In the past it might result in Wallet
  // data type failure due to a bug with handling |gc_directive|.

  GetFakeServer()->SetMaxGetUpdatesBatchSize(5);
  ASSERT_TRUE(SetupSync());

  // Use the ID which is the least one to guarantee that Wallet entity will be
  // in the first GetUpdates request.
  GetFakeServer()->SetWalletData({CreateSyncWalletCard(
      /*name=*/"server_id_0", /*last_four=*/"0001", kDefaultBillingAddressID)});

  // Inject a lot of bookmark to result in several GetUpdates requests.
  fake_server::EntityBuilderFactory entity_builder_factory;
  for (int i = 1; i < 15; i++) {
    std::string title = "Montreal Canadiens";
    fake_server::BookmarkEntityBuilder bookmark_builder =
        entity_builder_factory.NewBookmarkEntityBuilder(title);
    bookmark_builder.SetId("server_id_" + base::NumberToString(i));
    fake_server_->InjectEntity(bookmark_builder.BuildBookmark(
        GURL("http://foo.com/" + base::NumberToString(i))));
  }

  autofill::PersonalDataManager* pdm = GetPersonalDataManager(0);
  ASSERT_NE(nullptr, pdm);
  WaitForNumberOfCards(1, pdm);
}

class SingleClientWalletSecondaryAccountSyncTest
    : public SingleClientWalletSyncTest {
 public:
  SingleClientWalletSecondaryAccountSyncTest() = default;

  SingleClientWalletSecondaryAccountSyncTest(
      const SingleClientWalletSecondaryAccountSyncTest&) = delete;
  SingleClientWalletSecondaryAccountSyncTest& operator=(
      const SingleClientWalletSecondaryAccountSyncTest&) = delete;

  ~SingleClientWalletSecondaryAccountSyncTest() override = default;

  void SetUpInProcessBrowserTestFixture() override {
    SingleClientWalletSyncTest::SetUpInProcessBrowserTestFixture();

    test_signin_client_subscription_ =
        secondary_account_helper::SetUpSigninClient(&test_url_loader_factory_);
  }

  void SetUpOnMainThread() override {
#if BUILDFLAG(IS_CHROMEOS_ASH)
    secondary_account_helper::InitNetwork();
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)
    SyncTest::SetUpOnMainThread();
  }

  Profile* profile() { return GetProfile(0); }

 private:
  base::test::ScopedFeatureList features_;

  base::CallbackListSubscription test_signin_client_subscription_;
};

// ChromeOS doesn't support changes to the primary account after startup, so
// these tests don't apply.
#if !BUILDFLAG(IS_CHROMEOS_ASH)
IN_PROC_BROWSER_TEST_F(SingleClientWalletSecondaryAccountSyncTest,
                       SwitchesFromAccountToProfileStorageOnSyncOptIn) {
  ASSERT_TRUE(SetupClients()) << "SetupClients() failed.";

  GetFakeServer()->SetWalletData({CreateDefaultSyncWalletCard(),
                                  CreateDefaultSyncPaymentsCustomerData(),
                                  CreateDefaultSyncCreditCardCloudTokenData()});

  // Set up Sync in transport mode for an unconsented account.
  secondary_account_helper::SignInUnconsentedAccount(
      profile(), &test_url_loader_factory_, "user@email.com");
  ASSERT_TRUE(GetClient(0)->AwaitSyncTransportActive());
  ASSERT_EQ(syncer::SyncService::TransportState::ACTIVE,
            GetSyncService(0)->GetTransportState());
  ASSERT_FALSE(GetSyncService(0)->IsSyncFeatureEnabled());
  ASSERT_FALSE(GetSyncService(0)->IsSyncFeatureActive());
  ASSERT_TRUE(GetSyncService(0)->GetActiveDataTypes().Has(
      syncer::AUTOFILL_WALLET_DATA));

  // PersonalDataManager should use (ephemeral) account storage.
  EXPECT_FALSE(GetPersonalDataManager(0)
                   ->IsSyncFeatureEnabledForPaymentsServerMetrics());
  EXPECT_TRUE(
      GetPersonalDataManager(0)->IsUsingAccountStorageForServerDataForTest());

  scoped_refptr<autofill::AutofillWebDataService> account_data =
      GetAccountWebDataService(0);
  ASSERT_NE(nullptr, account_data);
  scoped_refptr<autofill::AutofillWebDataService> profile_data =
      GetProfileWebDataService(0);
  ASSERT_NE(nullptr, profile_data);

  // Check that the data is stored in the account storage (ephemeral), but not
  // in the profile storage (persisted).
  EXPECT_EQ(1U, GetServerCards(account_data).size());
  EXPECT_EQ(0U, GetServerCards(profile_data).size());
  EXPECT_NE(nullptr, GetPaymentsCustomerData(account_data).get());
  EXPECT_EQ(nullptr, GetPaymentsCustomerData(profile_data).get());
  EXPECT_EQ(1U, GetCreditCardCloudTokenData(account_data).size());
  EXPECT_EQ(0U, GetCreditCardCloudTokenData(profile_data).size());

  // Simulate the user opting in to full Sync, and set first-time setup to
  // complete.
  secondary_account_helper::GrantSyncConsent(profile(), "user@email.com");
  GetSyncService(0)->SetSyncFeatureRequested();
  GetSyncService(0)->GetUserSettings()->SetInitialSyncFeatureSetupComplete(
      kSetSourceFromTest);

  // Wait for Sync to get reconfigured into feature mode.
  ASSERT_TRUE(GetClient(0)->AwaitSyncSetupCompletion());
  ASSERT_EQ(syncer::SyncService::TransportState::ACTIVE,
            GetSyncService(0)->GetTransportState());
  ASSERT_TRUE(GetSyncService(0)->IsSyncFeatureEnabled());
  ASSERT_TRUE(GetSyncService(0)->IsSyncFeatureActive());
  ASSERT_TRUE(GetSyncService(0)->GetActiveDataTypes().Has(
      syncer::AUTOFILL_WALLET_DATA));

  // PersonalDataManager should have switched to persistent storage.
  EXPECT_TRUE(GetPersonalDataManager(0)
                  ->IsSyncFeatureEnabledForPaymentsServerMetrics());
  EXPECT_FALSE(
      GetPersonalDataManager(0)->IsUsingAccountStorageForServerDataForTest());

  // The data should now be in the profile storage (persisted).
  EXPECT_EQ(0U, GetServerCards(account_data).size());
  EXPECT_EQ(1U, GetServerCards(profile_data).size());
  EXPECT_EQ(nullptr, GetPaymentsCustomerData(account_data).get());
  EXPECT_NE(nullptr, GetPaymentsCustomerData(profile_data).get());
  EXPECT_EQ(0U, GetCreditCardCloudTokenData(account_data).size());
  EXPECT_EQ(1U, GetCreditCardCloudTokenData(profile_data).size());
}

IN_PROC_BROWSER_TEST_F(
    SingleClientWalletSecondaryAccountSyncTest,
    SwitchesFromAccountToProfileStorageOnSyncOptInWithAdvancedSetup) {
  ASSERT_TRUE(SetupClients()) << "SetupClients() failed.";

  GetFakeServer()->SetWalletData({CreateDefaultSyncWalletCard(),
                                  CreateDefaultSyncCreditCardCloudTokenData()});

  // Set up Sync in transport mode for an unconsented account.
  secondary_account_helper::SignInUnconsentedAccount(
      profile(), &test_url_loader_factory_, "user@email.com");
  ASSERT_TRUE(GetClient(0)->AwaitSyncTransportActive());
  ASSERT_EQ(syncer::SyncService::TransportState::ACTIVE,
            GetSyncService(0)->GetTransportState());
  ASSERT_FALSE(GetSyncService(0)->IsSyncFeatureEnabled());
  ASSERT_FALSE(GetSyncService(0)->IsSyncFeatureActive());
  ASSERT_TRUE(GetSyncService(0)->GetActiveDataTypes().Has(
      syncer::AUTOFILL_WALLET_DATA));

  // PersonalDataManager should use (ephemeral) account storage.
  EXPECT_FALSE(GetPersonalDataManager(0)
                   ->IsSyncFeatureEnabledForPaymentsServerMetrics());
  EXPECT_TRUE(
      GetPersonalDataManager(0)->IsUsingAccountStorageForServerDataForTest());

  scoped_refptr<autofill::AutofillWebDataService> account_data =
      GetAccountWebDataService(0);
  ASSERT_NE(nullptr, account_data);
  scoped_refptr<autofill::AutofillWebDataService> profile_data =
      GetProfileWebDataService(0);
  ASSERT_NE(nullptr, profile_data);

  // Check that the card is stored in the account storage (ephemeral), but not
  // in the profile storage (persisted).
  EXPECT_EQ(1U, GetServerCards(account_data).size());
  EXPECT_EQ(0U, GetServerCards(profile_data).size());
  EXPECT_EQ(1U, GetCreditCardCloudTokenData(account_data).size());
  EXPECT_EQ(0U, GetCreditCardCloudTokenData(profile_data).size());

  // Simulate the user opting in to full Sync.
  secondary_account_helper::GrantSyncConsent(profile(), "user@email.com");

  // Now start actually configuring Sync.
  GetSyncService(0)->SetSyncFeatureRequested();
  std::unique_ptr<syncer::SyncSetupInProgressHandle> setup_handle =
      GetSyncService(0)->GetSetupInProgressHandle();

  // TODO(crbug.com/1435431): kAutofill is used here to mimic what the UI does,
  // but could be removed once the coupling is relaxed.
  GetSyncService(0)->GetUserSettings()->SetSelectedTypes(
      /*sync_everything=*/false, {syncer::UserSelectableType::kAutofill,
                                  syncer::UserSelectableType::kPayments});

  // Once the user finishes the setup, the newly selected data types will
  // actually get configured.
  setup_handle.reset();
  ASSERT_EQ(syncer::SyncService::TransportState::CONFIGURING,
            GetSyncService(0)->GetTransportState());

  GetSyncService(0)->GetUserSettings()->SetInitialSyncFeatureSetupComplete(
      kSetSourceFromTest);

  // Wait for Sync to get reconfigured into feature mode.
  ASSERT_TRUE(GetClient(0)->AwaitSyncSetupCompletion());
  ASSERT_EQ(syncer::SyncService::TransportState::ACTIVE,
            GetSyncService(0)->GetTransportState());
  ASSERT_TRUE(GetSyncService(0)->IsSyncFeatureEnabled());
  ASSERT_TRUE(GetSyncService(0)->IsSyncFeatureActive());
  ASSERT_TRUE(GetSyncService(0)->GetActiveDataTypes().Has(
      syncer::AUTOFILL_WALLET_DATA));

  // PersonalDataManager should have switched to persistent storage.
  EXPECT_TRUE(GetPersonalDataManager(0)
                  ->IsSyncFeatureEnabledForPaymentsServerMetrics());
  EXPECT_FALSE(
      GetPersonalDataManager(0)->IsUsingAccountStorageForServerDataForTest());

  // The card should now be in the profile storage (persisted).
  EXPECT_EQ(0U, GetServerCards(account_data).size());
  EXPECT_EQ(1U, GetServerCards(profile_data).size());
  EXPECT_EQ(0U, GetCreditCardCloudTokenData(account_data).size());
  EXPECT_EQ(1U, GetCreditCardCloudTokenData(profile_data).size());
}
#endif  // !BUILDFLAG(IS_CHROMEOS_ASH)
