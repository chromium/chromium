// Copyright (c) 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/command_line.h"
#include "base/macros.h"
#include "base/message_loop/message_loop.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/chrome_notification_types.h"
#include "chrome/browser/sync/test/integration/autofill_helper.h"
#include "chrome/browser/sync/test/integration/profile_sync_service_harness.h"
#include "chrome/browser/sync/test/integration/secondary_account_helper.h"
#include "chrome/browser/sync/test/integration/single_client_status_change_checker.h"
#include "chrome/browser/sync/test/integration/sync_datatype_helper.h"
#include "chrome/browser/sync/test/integration/sync_test.h"
#include "chrome/browser/web_data_service_factory.h"
#include "components/autofill/core/browser/autofill_data_util.h"
#include "components/autofill/core/browser/credit_card.h"
#include "components/autofill/core/browser/field_types.h"
#include "components/autofill/core/browser/payments/payments_customer_data.h"
#include "components/autofill/core/browser/personal_data_manager.h"
#include "components/autofill/core/browser/personal_data_manager_observer.h"
#include "components/autofill/core/common/autofill_features.h"
#include "components/autofill/core/common/autofill_prefs.h"
#include "components/browser_sync/profile_sync_service.h"
#include "components/prefs/pref_service.h"
#include "components/sync/base/model_type.h"
#include "components/sync/driver/sync_driver_switches.h"
#include "components/sync/test/fake_server/fake_server.h"
#include "components/webdata/common/web_data_service_consumer.h"
#include "content/public/browser/notification_service.h"
#include "testing/gmock/include/gmock/gmock.h"

using autofill::AutofillProfile;
using autofill::CreditCard;
using autofill::data_util::TruncateUTF8;
using autofill_helper::GetAccountWebDataService;
using autofill_helper::GetPersonalDataManager;
using autofill_helper::GetProfileWebDataService;
using base::ASCIIToUTF16;

namespace {

ACTION_P(QuitMessageLoop, loop) {
  loop->Quit();
}

// Constants for the credit card.
const char kDefaultCardID[] = "wallet card ID";
const int kDefaultCardExpMonth = 8;
const int kDefaultCardExpYear = 2087;
const char kDefaultCardLastFour[] = "1234";
const char kDefaultCardName[] = "Patrick Valenzuela";
const char kDefaultBillingAddressId[] = "billing address entity ID";
const sync_pb::WalletMaskedCreditCard_WalletCardType kDefaultCardType =
    sync_pb::WalletMaskedCreditCard::AMEX;

// Constants for the address.
const char kDefaultAddressID[] = "wallet address ID";
const char kDefaultAddressName[] = "John S. Doe";
const char kDefaultCompanyName[] = "The Company";
const char kDefaultStreetAddress[] = "1234 Fake Street\nApp 2";
const char kDefaultCity[] = "Cityville";
const char kDefaultState[] = "Stateful";
const char kDefaultCountry[] = "US";
const char kDefaultZip[] = "90011";
const char kDefaultPhone[] = "1.800.555.1234";
const char kDefaultSortingCode[] = "CEDEX";
const char kDefaultDependentLocality[] = "DepLoc";
const char kDefaultLanguageCode[] = "en";

// Constants for PaymentsCustomerData.
const char kDefaultCustomerID[] = "deadbeef";

const char kLocalGuidA[] = "EDC609ED-7EEE-4F27-B00C-423242A9C44A";
const char kDifferentBillingAddressId[] = "another address entity ID";

template <class T>
class AutofillWebDataServiceConsumer : public WebDataServiceConsumer {
 public:
  AutofillWebDataServiceConsumer() {}

  virtual ~AutofillWebDataServiceConsumer() {}

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
  DISALLOW_COPY_AND_ASSIGN(AutofillWebDataServiceConsumer);
};

class PersonalDataLoadedObserverMock
    : public autofill::PersonalDataManagerObserver {
 public:
  PersonalDataLoadedObserverMock() {}
  ~PersonalDataLoadedObserverMock() override {}

  MOCK_METHOD0(OnPersonalDataChanged, void());
};

class WaitForNextWalletUpdateChecker : public StatusChangeChecker,
                                       public syncer::SyncServiceObserver {
 public:
  explicit WaitForNextWalletUpdateChecker(
      browser_sync::ProfileSyncService* service)
      : service_(service),
        initial_marker_(GetInitialMarker(service)),
        scoped_observer_(this) {
    scoped_observer_.Add(service);
  }

  bool IsExitConditionSatisfied() override {
    // GetLastCycleSnapshot() returns by value, so make sure to capture it for
    // iterator use.
    const syncer::SyncCycleSnapshot snap = service_->GetLastCycleSnapshot();
    const syncer::ProgressMarkerMap& progress_markers =
        snap.download_progress_markers();
    auto marker_it = progress_markers.find(syncer::AUTOFILL_WALLET_DATA);
    if (marker_it == progress_markers.end()) {
      return false;
    }
    return marker_it->second != initial_marker_;
  }

  std::string GetDebugMessage() const override {
    return "Waiting for an updated Wallet progress marker.";
  }

  // syncer::SyncServiceObserver implementation.
  void OnSyncCycleCompleted(syncer::SyncService* sync) override {
    CheckExitCondition();
  }

 private:
  static std::string GetInitialMarker(
      const browser_sync::ProfileSyncService* service) {
    // GetLastCycleSnapshot() returns by value, so make sure to capture it for
    // iterator use.
    const syncer::SyncCycleSnapshot snap = service->GetLastCycleSnapshot();
    const syncer::ProgressMarkerMap& progress_markers =
        snap.download_progress_markers();
    auto marker_it = progress_markers.find(syncer::AUTOFILL_WALLET_DATA);
    if (marker_it == progress_markers.end()) {
      return "N/A";
    }
    return marker_it->second;
  }

  const browser_sync::ProfileSyncService* service_;
  const std::string initial_marker_;
  ScopedObserver<browser_sync::ProfileSyncService,
                 WaitForNextWalletUpdateChecker>
      scoped_observer_;
};

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

#if !defined(OS_CHROMEOS)
std::unique_ptr<autofill::PaymentsCustomerData> GetPaymentsCustomerData(
    scoped_refptr<autofill::AutofillWebDataService> service) {
  AutofillWebDataServiceConsumer<
      std::unique_ptr<autofill::PaymentsCustomerData>>
      consumer;
  service->GetPaymentsCustomerData(&consumer);
  consumer.Wait();
  return std::move(consumer.result());
}
#endif

sync_pb::SyncEntity CreateDefaultSyncWalletCard() {
  sync_pb::SyncEntity entity;
  entity.set_name(kDefaultCardID);
  entity.set_id_string(kDefaultCardID);
  entity.set_version(0);  // Will be overridden by the fake server.
  entity.set_ctime(12345);
  entity.set_mtime(12345);
  sync_pb::AutofillWalletSpecifics* wallet_specifics =
      entity.mutable_specifics()->mutable_autofill_wallet();
  wallet_specifics->set_type(
      sync_pb::AutofillWalletSpecifics::MASKED_CREDIT_CARD);

  sync_pb::WalletMaskedCreditCard* credit_card =
      wallet_specifics->mutable_masked_card();
  credit_card->set_id(kDefaultCardID);
  credit_card->set_exp_month(kDefaultCardExpMonth);
  credit_card->set_exp_year(kDefaultCardExpYear);
  credit_card->set_last_four(kDefaultCardLastFour);
  credit_card->set_name_on_card(kDefaultCardName);
  credit_card->set_status(sync_pb::WalletMaskedCreditCard::VALID);
  credit_card->set_type(kDefaultCardType);
  credit_card->set_billing_address_id(kDefaultBillingAddressId);
  return entity;
}

sync_pb::SyncEntity CreateSyncWalletCard(const std::string& name,
                                         const std::string& last_four) {
  sync_pb::SyncEntity result = CreateDefaultSyncWalletCard();
  result.set_name(name);
  result.set_id_string(name);
  sync_pb::WalletMaskedCreditCard* credit_card = result.mutable_specifics()
                                                     ->mutable_autofill_wallet()
                                                     ->mutable_masked_card();
  credit_card->set_last_four(last_four);
  credit_card->set_id(name);
  return result;
}

sync_pb::SyncEntity CreateSyncPaymentsCustomerData(
    const std::string& customer_id) {
  sync_pb::SyncEntity entity;
  entity.set_name(customer_id);
  entity.set_id_string(customer_id);
  entity.set_version(0);  // Will be overridden by the fake server.
  entity.set_ctime(12345);
  entity.set_mtime(12345);
  sync_pb::AutofillWalletSpecifics* wallet_specifics =
      entity.mutable_specifics()->mutable_autofill_wallet();
  wallet_specifics->set_type(sync_pb::AutofillWalletSpecifics::CUSTOMER_DATA);

  sync_pb::PaymentsCustomerData* customer_data =
      wallet_specifics->mutable_customer_data();
  customer_data->set_id(customer_id);
  return entity;
}

sync_pb::SyncEntity CreateDefaultSyncPaymentsCustomerData() {
  return CreateSyncPaymentsCustomerData(kDefaultCustomerID);
}

CreditCard GetDefaultCreditCard() {
  CreditCard card(CreditCard::MASKED_SERVER_CARD, kDefaultCardID);
  card.SetExpirationMonth(kDefaultCardExpMonth);
  card.SetExpirationYear(kDefaultCardExpYear);
  card.SetNumber(base::UTF8ToUTF16(kDefaultCardLastFour));
  card.SetRawInfo(autofill::CREDIT_CARD_NAME_FULL,
                  base::UTF8ToUTF16(kDefaultCardName));
  card.SetServerStatus(CreditCard::OK);
  card.SetNetworkForMaskedCard(autofill::kAmericanExpressCard);
  card.set_card_type(CreditCard::CARD_TYPE_CREDIT);
  card.set_billing_address_id(kDefaultBillingAddressId);
  return card;
}

sync_pb::SyncEntity CreateDefaultSyncWalletAddress() {
  sync_pb::SyncEntity entity;
  entity.set_name(kDefaultAddressID);
  entity.set_id_string(kDefaultAddressID);
  entity.set_version(0);  // Will be overridden by the fake server.
  entity.set_ctime(12345);
  entity.set_mtime(12345);

  sync_pb::AutofillWalletSpecifics* wallet_specifics =
      entity.mutable_specifics()->mutable_autofill_wallet();
  wallet_specifics->set_type(sync_pb::AutofillWalletSpecifics::POSTAL_ADDRESS);

  sync_pb::WalletPostalAddress* wallet_address =
      wallet_specifics->mutable_address();
  wallet_address->set_id(kDefaultAddressID);
  wallet_address->set_recipient_name(kDefaultAddressName);
  wallet_address->set_company_name(kDefaultCompanyName);
  wallet_address->add_street_address(kDefaultStreetAddress);
  wallet_address->set_address_1(kDefaultState);
  wallet_address->set_address_2(kDefaultCity);
  wallet_address->set_address_3(kDefaultDependentLocality);
  wallet_address->set_postal_code(kDefaultZip);
  wallet_address->set_country_code(kDefaultCountry);
  wallet_address->set_phone_number(kDefaultPhone);
  wallet_address->set_sorting_code(kDefaultSortingCode);
  wallet_address->set_language_code(kDefaultLanguageCode);

  return entity;
}

sync_pb::SyncEntity CreateSyncWalletAddress(const std::string& name,
                                            const std::string& company) {
  sync_pb::SyncEntity result = CreateDefaultSyncWalletAddress();
  result.set_name(name);
  result.set_id_string(name);
  sync_pb::WalletPostalAddress* wallet_address =
      result.mutable_specifics()->mutable_autofill_wallet()->mutable_address();
  wallet_address->set_id(name);
  wallet_address->set_company_name(company);
  return result;
}

// TODO(sebsg): Instead add a function to create a card, and one to inject in
// the server. Then compare the cards directly.
void ExpectDefaultCreditCardValues(const CreditCard& card) {
  EXPECT_EQ(CreditCard::MASKED_SERVER_CARD, card.record_type());
  EXPECT_EQ(kDefaultCardID, card.server_id());
  EXPECT_EQ(base::UTF8ToUTF16(kDefaultCardLastFour), card.LastFourDigits());
  EXPECT_EQ(autofill::kAmericanExpressCard, card.network());
  EXPECT_EQ(kDefaultCardExpMonth, card.expiration_month());
  EXPECT_EQ(kDefaultCardExpYear, card.expiration_year());
  EXPECT_EQ(base::UTF8ToUTF16(kDefaultCardName),
            card.GetRawInfo(autofill::ServerFieldType::CREDIT_CARD_NAME_FULL));
  EXPECT_EQ(kDefaultBillingAddressId, card.billing_address_id());
}

// TODO(sebsg): Instead add a function to create a profile, and one to inject in
// the server. Then compare the profiles directly.
void ExpectDefaultProfileValues(const AutofillProfile& profile) {
  EXPECT_EQ(kDefaultLanguageCode, profile.language_code());
  EXPECT_EQ(
      kDefaultAddressName,
      TruncateUTF8(base::UTF16ToUTF8(profile.GetRawInfo(autofill::NAME_FULL))));
  EXPECT_EQ(kDefaultCompanyName,
            TruncateUTF8(
                base::UTF16ToUTF8(profile.GetRawInfo(autofill::COMPANY_NAME))));
  EXPECT_EQ(kDefaultStreetAddress,
            TruncateUTF8(base::UTF16ToUTF8(
                profile.GetRawInfo(autofill::ADDRESS_HOME_STREET_ADDRESS))));
  EXPECT_EQ(kDefaultCity, TruncateUTF8(base::UTF16ToUTF8(profile.GetRawInfo(
                              autofill::ADDRESS_HOME_CITY))));
  EXPECT_EQ(kDefaultState, TruncateUTF8(base::UTF16ToUTF8(profile.GetRawInfo(
                               autofill::ADDRESS_HOME_STATE))));
  EXPECT_EQ(kDefaultZip, TruncateUTF8(base::UTF16ToUTF8(
                             profile.GetRawInfo(autofill::ADDRESS_HOME_ZIP))));
  EXPECT_EQ(kDefaultCountry, TruncateUTF8(base::UTF16ToUTF8(profile.GetRawInfo(
                                 autofill::ADDRESS_HOME_COUNTRY))));
  EXPECT_EQ(kDefaultPhone, TruncateUTF8(base::UTF16ToUTF8(profile.GetRawInfo(
                               autofill::PHONE_HOME_WHOLE_NUMBER))));
  EXPECT_EQ(kDefaultSortingCode,
            TruncateUTF8(base::UTF16ToUTF8(
                profile.GetRawInfo(autofill::ADDRESS_HOME_SORTING_CODE))));
  EXPECT_EQ(kDefaultDependentLocality,
            TruncateUTF8(base::UTF16ToUTF8(profile.GetRawInfo(
                autofill::ADDRESS_HOME_DEPENDENT_LOCALITY))));
}

// Class that enables or disables USS based on test parameter. Must be the first
// base class of the test fixture.
// TODO(jkrcal): When the new implementation fully launches, remove this class,
// convert all tests from *_P back to *_F and remove the instance at the end.
class UssSwitchToggler : public testing::WithParamInterface<bool> {
 public:
  UssSwitchToggler() {}

  // Sets up feature overrides, based on the parameter of the test.
  void InitWithDefaultFeatures() { InitWithFeatures({}, {}); }

  // Sets up feature overrides, adds the toggled feature on top of specified
  // |enabled_features| and |disabled_features|. Vectors are passed by value
  // because we need to alter them anyway.
  void InitWithFeatures(std::vector<base::Feature> enabled_features,
                        std::vector<base::Feature> disabled_features) {
    if (GetParam()) {
      enabled_features.push_back(switches::kSyncUSSAutofillWalletData);
    } else {
      disabled_features.push_back(switches::kSyncUSSAutofillWalletData);
    }

    override_features_.InitWithFeatures(enabled_features, disabled_features);
  }

 private:
  base::test::ScopedFeatureList override_features_;
};

}  // namespace

class SingleClientWalletSyncTest : public UssSwitchToggler, public SyncTest {
 public:
  SingleClientWalletSyncTest() : SyncTest(SINGLE_CLIENT) {}
  ~SingleClientWalletSyncTest() override {}

 protected:
  void RefreshAndWaitForOnPersonalDataChanged(
      autofill::PersonalDataManager* pdm) {
    WaitForOnPersonalDataChanged(/*should_trigger_refresh=*/true, pdm);
  }

  void WaitForOnPersonalDataChanged(bool should_trigger_refresh,
                                    autofill::PersonalDataManager* pdm) {
    pdm->AddObserver(&personal_data_observer_);
    if (should_trigger_refresh) {
      pdm->Refresh();
    }
    base::RunLoop run_loop;
    EXPECT_CALL(personal_data_observer_, OnPersonalDataChanged())
        .WillRepeatedly(QuitMessageLoop(&run_loop));
    run_loop.Run();
    pdm->RemoveObserver(&personal_data_observer_);
  }

  void WaitForNumberOfCards(autofill::PersonalDataManager* pdm,
                            size_t expected_count) {
    while (pdm->GetCreditCards().size() != expected_count) {
      WaitForOnPersonalDataChanged(/*should_trigger_refresh=*/false, pdm);
    }
  }

  PersonalDataLoadedObserverMock personal_data_observer_;

 private:
  DISALLOW_COPY_AND_ASSIGN(SingleClientWalletSyncTest);
};

IN_PROC_BROWSER_TEST_P(SingleClientWalletSyncTest, EnabledByDefault) {
  InitWithDefaultFeatures();
  ASSERT_TRUE(SetupSync());
  ASSERT_TRUE(GetClient(0)->service()->GetActiveDataTypes().Has(
      syncer::AUTOFILL_WALLET_DATA));
  // TODO(pvalenzuela): Assert that the local root node for AUTOFILL_WALLET_DATA
  // exists.
  ASSERT_TRUE(GetClient(0)->service()->GetActiveDataTypes().Has(
      syncer::AUTOFILL_WALLET_METADATA));
}

IN_PROC_BROWSER_TEST_P(SingleClientWalletSyncTest, DownloadProfileStorage) {
  InitWithFeatures(/*enabled_features=*/{},
                   /*disabled_features=*/
                   {autofill::features::kAutofillEnableAccountWalletStorage});
  GetFakeServer()->SetWalletData(
      {CreateDefaultSyncWalletAddress(), CreateDefaultSyncWalletCard()});
  ASSERT_TRUE(SetupSync());

  auto profile_data = GetProfileWebDataService(0);
  ASSERT_NE(nullptr, profile_data);
  auto account_data = GetAccountWebDataService(0);
  ASSERT_EQ(nullptr, account_data);

  autofill::PersonalDataManager* pdm = GetPersonalDataManager(0);
  ASSERT_NE(nullptr, pdm);
  std::vector<CreditCard*> cards = pdm->GetCreditCards();
  ASSERT_EQ(1uL, cards.size());
  std::vector<AutofillProfile*> profiles = pdm->GetServerProfiles();
  ASSERT_EQ(1uL, profiles.size());

  // Check that the data was set correctly.
  ExpectDefaultCreditCardValues(*cards[0]);
  ExpectDefaultProfileValues(*profiles[0]);

  // Check that the data is stored in the profile storage.
  EXPECT_EQ(1U, GetServerCards(GetProfileWebDataService(0)).size());
  EXPECT_EQ(1U, GetServerProfiles(GetProfileWebDataService(0)).size());
}

// ChromeOS does not support late signin after profile creation, so the test
// below does not apply, at least in the current form.
#if !defined(OS_CHROMEOS)
// The account storage requires USS, so we only test the USS implementation
// here.
IN_PROC_BROWSER_TEST_F(SingleClientWalletSyncTest,
                       DownloadAccountStorage_Card) {
  base::test::ScopedFeatureList features;
  features.InitWithFeatures(
      /*enabled_features=*/{switches::kSyncStandaloneTransport,
                            switches::kSyncUSSAutofillWalletData,
                            autofill::features::
                                kAutofillEnableAccountWalletStorage},
      /*disabled_features=*/{});

  ASSERT_TRUE(SetupClients());
  autofill::PersonalDataManager* pdm = GetPersonalDataManager(0);
  pdm->OnSyncServiceInitialized(GetSyncService(0));

  GetFakeServer()->SetWalletData(
      {CreateDefaultSyncWalletAddress(), CreateDefaultSyncWalletCard()});

  ASSERT_TRUE(GetClient(0)->SignInPrimaryAccount());
  ASSERT_TRUE(GetClient(0)->AwaitEngineInitialization(
      /*skip_passphrase_verification=*/false));
  ASSERT_TRUE(AwaitQuiescence());
  ASSERT_EQ(syncer::SyncService::TransportState::ACTIVE,
            GetSyncService(0)->GetTransportState());
  ASSERT_TRUE(GetSyncService(0)->GetActiveDataTypes().Has(
      syncer::AUTOFILL_WALLET_DATA));

  auto profile_data = GetProfileWebDataService(0);
  ASSERT_NE(nullptr, profile_data);
  auto account_data = GetAccountWebDataService(0);
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

  // Now sign back out.
  GetClient(0)->SignOutPrimaryAccount();

  // Verify that sync is stopped.
  ASSERT_EQ(syncer::SyncService::TransportState::DISABLED,
            GetSyncService(0)->GetTransportState());
  ASSERT_FALSE(GetSyncService(0)->GetActiveDataTypes().Has(
      syncer::AUTOFILL_WALLET_DATA));

  // Wait for PDM to receive the data change.
  WaitForOnPersonalDataChanged(/*should_trigger_refresh=*/false, pdm);

  // Check that the account storage is now cleared.
  EXPECT_EQ(0U, GetServerCards(account_data).size());

  cards = pdm->GetCreditCards();
  EXPECT_EQ(0U, cards.size());
}
#endif  // !defined(OS_CHROMEOS)

// Wallet data should get cleared from the database when sync is disabled.
IN_PROC_BROWSER_TEST_P(SingleClientWalletSyncTest, ClearOnDisableSync) {
  InitWithDefaultFeatures();
  GetFakeServer()->SetWalletData({CreateDefaultSyncWalletAddress(),
                                  CreateDefaultSyncWalletCard(),
                                  CreateDefaultSyncPaymentsCustomerData()});
  ASSERT_TRUE(SetupSync());

  // Make sure the data is in the DB.
  autofill::PersonalDataManager* pdm = GetPersonalDataManager(0);
  ASSERT_NE(nullptr, pdm);
  EXPECT_EQ(1uL, pdm->GetCreditCards().size());
  EXPECT_EQ(kDefaultCustomerID, pdm->GetPaymentsCustomerData()->customer_id);

  // Turn off sync, the data should be gone.
  GetSyncService(0)->RequestStop(syncer::SyncService::CLEAR_DATA);
  WaitForNumberOfCards(pdm, 0);

  EXPECT_EQ(0uL, pdm->GetCreditCards().size());
  EXPECT_EQ(nullptr, pdm->GetPaymentsCustomerData());

  // Turn sync on again, the data should come back.
  GetSyncService(0)->RequestStart();
  // RequestStop(CLEAR_DATA) also clears the "first setup complete" flag, so
  // set it again.
  GetSyncService(0)->SetFirstSetupComplete();
  // Wait until Sync restores the card and it arrives at PDM.
  WaitForNumberOfCards(pdm, 1);

  EXPECT_EQ(1uL, pdm->GetCreditCards().size());
  EXPECT_EQ(kDefaultCustomerID, pdm->GetPaymentsCustomerData()->customer_id);
}

// Wallet data should get cleared from the database when sync is (temporarily)
// stopped, e.g. due to the Sync feature toggle in Android settings.
IN_PROC_BROWSER_TEST_P(SingleClientWalletSyncTest, ClearOnStopSync) {
  InitWithDefaultFeatures();
  GetFakeServer()->SetWalletData({CreateDefaultSyncWalletAddress(),
                                  CreateDefaultSyncWalletCard(),
                                  CreateDefaultSyncPaymentsCustomerData()});
  ASSERT_TRUE(SetupSync());

  // Make sure the data is in the DB.
  autofill::PersonalDataManager* pdm = GetPersonalDataManager(0);
  ASSERT_NE(nullptr, pdm);
  EXPECT_EQ(1uL, pdm->GetCreditCards().size());
  EXPECT_EQ(kDefaultCustomerID, pdm->GetPaymentsCustomerData()->customer_id);

  // Turn off sync, the card should be gone.
  GetSyncService(0)->RequestStop(syncer::SyncService::KEEP_DATA);
  WaitForNumberOfCards(pdm, 0);

  EXPECT_EQ(0uL, pdm->GetCreditCards().size());
  EXPECT_EQ(nullptr, pdm->GetPaymentsCustomerData());

  // Turn sync on again, the data should come back.
  GetSyncService(0)->RequestStart();
  // Wait until Sync restores the card and it arrives at PDM.
  WaitForNumberOfCards(pdm, 1);

  EXPECT_EQ(1uL, pdm->GetCreditCards().size());
  EXPECT_EQ(kDefaultCustomerID, pdm->GetPaymentsCustomerData()->customer_id);
}

// ChromeOS does not sign out, so the test below does not apply.
#if !defined(OS_CHROMEOS)
// Wallet data should get cleared from the database when the user signs out.
IN_PROC_BROWSER_TEST_P(SingleClientWalletSyncTest, ClearOnSignOut) {
  InitWithDefaultFeatures();
  GetFakeServer()->SetWalletData({CreateDefaultSyncWalletAddress(),
                                  CreateDefaultSyncWalletCard(),
                                  CreateDefaultSyncPaymentsCustomerData()});
  ASSERT_TRUE(SetupSync());

  // Make sure the data is in the DB.
  autofill::PersonalDataManager* pdm = GetPersonalDataManager(0);
  ASSERT_NE(nullptr, pdm);
  EXPECT_EQ(1uL, pdm->GetCreditCards().size());
  EXPECT_EQ(kDefaultCustomerID, pdm->GetPaymentsCustomerData()->customer_id);

  // Turn off sync, the data should be gone.
  GetClient(0)->SignOutPrimaryAccount();
  WaitForNumberOfCards(pdm, 0);

  EXPECT_EQ(0uL, pdm->GetCreditCards().size());
  EXPECT_EQ(nullptr, pdm->GetPaymentsCustomerData());
}
#endif  // !defined(OS_CHROMEOS)

// Wallet is not using incremental updates. Make sure existing data gets
// replaced when synced down.
IN_PROC_BROWSER_TEST_P(SingleClientWalletSyncTest,
                       NewSyncDataShouldReplaceExistingData) {
  InitWithDefaultFeatures();

  GetFakeServer()->SetWalletData(
      {CreateSyncWalletCard(/*name=*/"card-1", /*last_four=*/"0001"),
       CreateSyncWalletAddress(/*name=*/"address-1", /*company=*/"Company-1"),
       CreateDefaultSyncPaymentsCustomerData()});
  ASSERT_TRUE(SetupSync());

  // Make sure the data is in the DB.
  autofill::PersonalDataManager* pdm = GetPersonalDataManager(0);
  ASSERT_NE(nullptr, pdm);
  std::vector<CreditCard*> cards = pdm->GetCreditCards();
  ASSERT_EQ(1uL, cards.size());
  EXPECT_EQ(ASCIIToUTF16("0001"), cards[0]->LastFourDigits());
  std::vector<AutofillProfile*> profiles = pdm->GetServerProfiles();
  ASSERT_EQ(1uL, profiles.size());
  EXPECT_EQ("Company-1", TruncateUTF8(base::UTF16ToUTF8(
                             profiles[0]->GetRawInfo(autofill::COMPANY_NAME))));
  EXPECT_EQ(kDefaultCustomerID, pdm->GetPaymentsCustomerData()->customer_id);

  // Put some completely new data in the sync server.
  GetFakeServer()->SetWalletData(
      {CreateSyncWalletCard(/*name=*/"new-card", /*last_four=*/"0002"),
       CreateSyncWalletAddress(/*name=*/"new-address", /*company=*/"Company-2"),
       CreateSyncPaymentsCustomerData(/*customer_id=*/"newid")});

  // Constructing the checker captures the current progress marker. Make sure to
  // do that before triggering the fetch.
  WaitForNextWalletUpdateChecker checker(GetSyncService(0));
  // Trigger a sync and wait for the new data to arrive.
  TriggerSyncForModelTypes(0,
                           syncer::ModelTypeSet(syncer::AUTOFILL_WALLET_DATA));
  ASSERT_TRUE(checker.Wait());

  // Make sure only the new data is present.
  cards = pdm->GetCreditCards();
  ASSERT_EQ(1uL, cards.size());
  EXPECT_EQ(ASCIIToUTF16("0002"), cards[0]->LastFourDigits());
  profiles = pdm->GetServerProfiles();
  ASSERT_EQ(1uL, profiles.size());
  EXPECT_EQ("Company-2", TruncateUTF8(base::UTF16ToUTF8(
                             profiles[0]->GetRawInfo(autofill::COMPANY_NAME))));
  EXPECT_EQ("newid", pdm->GetPaymentsCustomerData()->customer_id);
}

// Wallet is not using incremental updates. The server either sends a non-empty
// update with deletion gc directives and with the (possibly empty) full data
// set, or (more often) an empty update.
IN_PROC_BROWSER_TEST_P(SingleClientWalletSyncTest, EmptyUpdatesAreIgnored) {
  InitWithDefaultFeatures();
  GetFakeServer()->SetWalletData(
      {CreateSyncWalletCard(/*name=*/"card-1", /*last_four=*/"0001"),
       CreateSyncWalletAddress(/*name=*/"address-1", /*company=*/"Company-1"),
       CreateDefaultSyncPaymentsCustomerData()});
  ASSERT_TRUE(SetupSync());

  // Make sure the card is in the DB.
  autofill::PersonalDataManager* pdm = GetPersonalDataManager(0);
  ASSERT_NE(nullptr, pdm);
  std::vector<CreditCard*> cards = pdm->GetCreditCards();
  ASSERT_EQ(1uL, cards.size());
  EXPECT_EQ(ASCIIToUTF16("0001"), cards[0]->LastFourDigits());
  std::vector<AutofillProfile*> profiles = pdm->GetServerProfiles();
  ASSERT_EQ(1uL, profiles.size());
  EXPECT_EQ("Company-1", TruncateUTF8(base::UTF16ToUTF8(
                             profiles[0]->GetRawInfo(autofill::COMPANY_NAME))));
  EXPECT_EQ(kDefaultCustomerID, pdm->GetPaymentsCustomerData()->customer_id);

  // Do not change anything on the server so that the update forced below is an
  // empty one.

  // Constructing the checker captures the current progress marker. Make sure to
  // do that before triggering the fetch.
  WaitForNextWalletUpdateChecker checker(GetSyncService(0));
  // Trigger a sync and wait for the new data to arrive.
  TriggerSyncForModelTypes(0,
                           syncer::ModelTypeSet(syncer::AUTOFILL_WALLET_DATA));
  ASSERT_TRUE(checker.Wait());

  // Refresh the pdm so that it gets cards from autofill table.
  RefreshAndWaitForOnPersonalDataChanged(pdm);

  // Make sure the same data is present on the client.
  cards = pdm->GetCreditCards();
  ASSERT_EQ(1uL, cards.size());
  EXPECT_EQ(ASCIIToUTF16("0001"), cards[0]->LastFourDigits());
  profiles = pdm->GetServerProfiles();
  ASSERT_EQ(1uL, profiles.size());
  EXPECT_EQ("Company-1", TruncateUTF8(base::UTF16ToUTF8(
                             profiles[0]->GetRawInfo(autofill::COMPANY_NAME))));
  EXPECT_EQ(kDefaultCustomerID, pdm->GetPaymentsCustomerData()->customer_id);
}

// Wallet data should get cleared from the database when the wallet sync type
// flag is disabled.
IN_PROC_BROWSER_TEST_P(SingleClientWalletSyncTest, ClearOnDisableWalletSync) {
  InitWithDefaultFeatures();
  GetFakeServer()->SetWalletData({CreateDefaultSyncWalletAddress(),
                                  CreateDefaultSyncWalletCard(),
                                  CreateDefaultSyncPaymentsCustomerData()});
  ASSERT_TRUE(SetupSync());

  // Make sure the data is in the DB.
  autofill::PersonalDataManager* pdm = GetPersonalDataManager(0);
  ASSERT_NE(nullptr, pdm);
  EXPECT_EQ(1uL, pdm->GetCreditCards().size());
  EXPECT_EQ(kDefaultCustomerID, pdm->GetPaymentsCustomerData()->customer_id);

  // Turn off autofill sync, the data should be gone.
  ASSERT_TRUE(GetClient(0)->DisableSyncForDatatype(syncer::AUTOFILL));
  WaitForOnPersonalDataChanged(/*should_trigger_refresh=*/false, pdm);

  EXPECT_EQ(0uL, pdm->GetCreditCards().size());
  EXPECT_EQ(nullptr, pdm->GetPaymentsCustomerData());
}

// Wallet data should get cleared from the database when the wallet autofill
// integration flag is disabled.
IN_PROC_BROWSER_TEST_P(SingleClientWalletSyncTest,
                       ClearOnDisableWalletAutofill) {
  InitWithDefaultFeatures();
  GetFakeServer()->SetWalletData({CreateDefaultSyncWalletAddress(),
                                  CreateDefaultSyncWalletCard(),
                                  CreateDefaultSyncPaymentsCustomerData()});
  ASSERT_TRUE(SetupSync());

  // Make sure the card and PaymentsCustomerData are in the DB.
  autofill::PersonalDataManager* pdm = GetPersonalDataManager(0);
  ASSERT_NE(nullptr, pdm);
  EXPECT_EQ(1uL, pdm->GetCreditCards().size());

  // Turn off the wallet autofill pref, the card should be gone as a side
  // effect of the wallet data type controller noticing.
  autofill::prefs::SetPaymentsIntegrationEnabled(GetProfile(0)->GetPrefs(),
                                                 false);
  EXPECT_EQ(0uL, pdm->GetCreditCards().size());
}

// Wallet data present on the client should be cleared in favor of the new data
// synced down form the server.
IN_PROC_BROWSER_TEST_P(SingleClientWalletSyncTest,
                       NewWalletCardRemovesExistingCardAndProfile) {
  InitWithDefaultFeatures();
  ASSERT_TRUE(SetupClients());
  autofill::PersonalDataManager* pdm = GetPersonalDataManager(0);
  ASSERT_NE(nullptr, pdm);

  // Add a server credit card on the client.
  CreditCard credit_card(CreditCard::MASKED_SERVER_CARD, "a123");
  std::vector<CreditCard> credit_cards = {credit_card};
  autofill_helper::SetServerCreditCards(0, credit_cards);

  // Add a server profile on the client.
  AutofillProfile profile(AutofillProfile::SERVER_PROFILE, "a123");
  profile.SetRawInfo(autofill::COMPANY_NAME, ASCIIToUTF16("JustATest"));
  std::vector<AutofillProfile> client_profiles = {profile};
  autofill_helper::SetServerProfiles(0, client_profiles);

  // Add PaymentsCustomerData on the client.
  autofill_helper::SetPaymentsCustomerData(
      0, autofill::PaymentsCustomerData(/*customer_id=*/kDefaultCustomerID));

  // Refresh the pdm so that it gets data from autofill table.
  RefreshAndWaitForOnPersonalDataChanged(pdm);

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

  // Add a new card from the server and sync it down.
  GetFakeServer()->SetWalletData(
      {CreateDefaultSyncWalletCard(), CreateDefaultSyncPaymentsCustomerData()});
  ASSERT_TRUE(SetupSync());

  // The only card present on the client should be the one from the server.
  cards = pdm->GetCreditCards();
  EXPECT_EQ(1uL, cards.size());
  EXPECT_EQ(kDefaultCardID, cards[0]->server_id());

  // There should be no profile present.
  profiles = pdm->GetServerProfiles();
  EXPECT_EQ(0uL, profiles.size());

  // The PaymentsCustomerData should still be there.
  EXPECT_EQ(kDefaultCustomerID, pdm->GetPaymentsCustomerData()->customer_id);
}

// Wallet data present on the client should be cleared in favor of the new data
// synced down form the server.
IN_PROC_BROWSER_TEST_P(SingleClientWalletSyncTest,
                       NewWalletDataRemovesExistingData) {
  InitWithDefaultFeatures();
  ASSERT_TRUE(SetupClients());
  autofill::PersonalDataManager* pdm = GetPersonalDataManager(0);
  ASSERT_NE(nullptr, pdm);

  // Add a server profile on the client.
  AutofillProfile profile(AutofillProfile::SERVER_PROFILE, "a123");
  profile.SetRawInfo(autofill::COMPANY_NAME, ASCIIToUTF16("JustATest"));
  std::vector<AutofillProfile> client_profiles = {profile};
  autofill_helper::SetServerProfiles(0, client_profiles);

  // Add a server credit card on the client.
  CreditCard credit_card(CreditCard::MASKED_SERVER_CARD, "a123");
  std::vector<CreditCard> credit_cards = {credit_card};
  autofill_helper::SetServerCreditCards(0, credit_cards);

  // Add PaymentsCustomerData on the client.
  autofill_helper::SetPaymentsCustomerData(
      0, autofill::PaymentsCustomerData(/*customer_id=*/kDefaultCustomerID));

  // Refresh the pdm so that it gets cards from autofill table.
  RefreshAndWaitForOnPersonalDataChanged(pdm);

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

  // Add a new profile and new customer data from the server and sync them down.
  GetFakeServer()->SetWalletData(
      {CreateDefaultSyncWalletAddress(),
       CreateSyncPaymentsCustomerData(/*customer_id=*/"newid")});
  ASSERT_TRUE(SetupSync());

  // The only profile present on the client should be the one from the server.
  profiles = pdm->GetServerProfiles();
  EXPECT_EQ(1uL, profiles.size());
  EXPECT_EQ(kDefaultCompanyName,
            TruncateUTF8(base::UTF16ToUTF8(
                profiles[0]->GetRawInfo(autofill::COMPANY_NAME))));

  // There should be no cards present.
  cards = pdm->GetCreditCards();
  EXPECT_EQ(0uL, cards.size());

  // Payments customer data should be updated.
  EXPECT_EQ("newid", pdm->GetPaymentsCustomerData()->customer_id);
}

// Tests that a local billing address id set on a card on the client should not
// be overwritten when that same card is synced again.
IN_PROC_BROWSER_TEST_P(SingleClientWalletSyncTest,
                       SameWalletCard_PreservesLocalBillingAddressId) {
  InitWithDefaultFeatures();
  ASSERT_TRUE(SetupClients());
  autofill::PersonalDataManager* pdm = GetPersonalDataManager(0);
  ASSERT_NE(nullptr, pdm);

  // Add a server credit card on the client but with the billing address id of a
  // local profile.
  CreditCard credit_card = GetDefaultCreditCard();
  credit_card.set_billing_address_id(kLocalGuidA);
  std::vector<CreditCard> credit_cards = {credit_card};
  autofill_helper::SetServerCreditCards(0, credit_cards);

  // Refresh the pdm so that it gets cards from autofill table.
  RefreshAndWaitForOnPersonalDataChanged(pdm);

  // Make sure the card was added correctly.
  std::vector<CreditCard*> cards = pdm->GetCreditCards();
  ASSERT_EQ(1uL, cards.size());
  EXPECT_EQ(kDefaultCardID, cards[0]->server_id());

  // Sync the same card from the server, except with a default billing address
  // id.
  GetFakeServer()->SetWalletData({CreateDefaultSyncWalletCard()});
  ASSERT_TRUE(SetupSync());

  // The billing address is should still refer to the local profile.
  cards = pdm->GetCreditCards();
  ASSERT_EQ(1uL, cards.size());
  EXPECT_EQ(kDefaultCardID, cards[0]->server_id());
  EXPECT_EQ(kLocalGuidA, cards[0]->billing_address_id());
}

// Tests that a server billing address id set on a card on the client is
// overwritten when that same card is synced again.
IN_PROC_BROWSER_TEST_P(SingleClientWalletSyncTest,
                       SameWalletCard_DiscardsOldServerBillingAddressId) {
  InitWithDefaultFeatures();
  ASSERT_TRUE(SetupClients());
  autofill::PersonalDataManager* pdm = GetPersonalDataManager(0);
  ASSERT_NE(nullptr, pdm);

  // Add a server credit card on the client but with the billing address id of a
  // server profile.
  CreditCard credit_card = GetDefaultCreditCard();
  credit_card.set_billing_address_id(kDifferentBillingAddressId);
  std::vector<CreditCard> credit_cards = {credit_card};
  autofill_helper::SetServerCreditCards(0, credit_cards);

  // Refresh the pdm so that it gets cards from autofill table.
  RefreshAndWaitForOnPersonalDataChanged(pdm);

  // Make sure the card was added correctly.
  std::vector<CreditCard*> cards = pdm->GetCreditCards();
  ASSERT_EQ(1uL, cards.size());
  EXPECT_EQ(kDefaultCardID, cards[0]->server_id());

  // Sync the same card from the server, except with a default billing address
  // id.
  GetFakeServer()->SetWalletData({CreateDefaultSyncWalletCard()});
  ASSERT_TRUE(SetupSync());

  // The billing address should be the one from the server card.
  cards = pdm->GetCreditCards();
  ASSERT_EQ(1uL, cards.size());
  EXPECT_EQ(kDefaultCardID, cards[0]->server_id());
  EXPECT_EQ(kDefaultBillingAddressId, cards[0]->billing_address_id());
}

class SingleClientWalletSecondaryAccountSyncTest
    : public SingleClientWalletSyncTest {
 public:
  SingleClientWalletSecondaryAccountSyncTest() {
    features_.InitWithFeatures(
        /*enabled_features=*/{switches::kSyncStandaloneTransport,
                              switches::kSyncSupportSecondaryAccount,
                              switches::kSyncUSSAutofillWalletData,
                              autofill::features::
                                  kAutofillEnableAccountWalletStorage},
        /*disabled_features=*/{});
  }
  ~SingleClientWalletSecondaryAccountSyncTest() override {}

  void SetUpInProcessBrowserTestFixture() override {
    fake_gaia_cookie_manager_factory_ =
        secondary_account_helper::SetUpFakeGaiaCookieManagerService();
  }

  void SetUpOnMainThread() override {
#if defined(OS_CHROMEOS)
    secondary_account_helper::InitNetwork();
#endif  // defined(OS_CHROMEOS)
  }

  Profile* profile() { return GetProfile(0); }

 private:
  base::test::ScopedFeatureList features_;

  secondary_account_helper::ScopedFakeGaiaCookieManagerServiceFactory
      fake_gaia_cookie_manager_factory_;

  DISALLOW_COPY_AND_ASSIGN(SingleClientWalletSecondaryAccountSyncTest);
};

// ChromeOS doesn't support changes to the primary account after startup, so
// these secondary-account-related tests don't apply.
#if !defined(OS_CHROMEOS)
IN_PROC_BROWSER_TEST_F(SingleClientWalletSecondaryAccountSyncTest,
                       SwitchesFromAccountToProfileStorageOnSyncOptIn) {
  ASSERT_TRUE(SetupClients()) << "SetupClients() failed.";
  GetPersonalDataManager(0)->OnSyncServiceInitialized(GetSyncService(0));

  GetFakeServer()->SetWalletData(
      {CreateDefaultSyncWalletCard(), CreateDefaultSyncPaymentsCustomerData()});

  // Set up Sync in transport mode for a non-primary account.
  secondary_account_helper::SignInSecondaryAccount(profile(), "user@email.com");
  ASSERT_TRUE(GetClient(0)->AwaitSyncSetupCompletion(
      /*skip_passphrase_verification=*/false));
  ASSERT_EQ(syncer::SyncService::TransportState::ACTIVE,
            GetSyncService(0)->GetTransportState());
  ASSERT_FALSE(GetSyncService(0)->IsSyncFeatureEnabled());
  ASSERT_FALSE(GetSyncService(0)->IsSyncFeatureActive());
  ASSERT_TRUE(GetSyncService(0)->GetActiveDataTypes().Has(
      syncer::AUTOFILL_WALLET_DATA));

  // PersonalDataManager should use (ephemeral) account storage.
  EXPECT_FALSE(GetPersonalDataManager(0)->IsSyncFeatureEnabled());
  EXPECT_TRUE(
      GetPersonalDataManager(0)->IsUsingAccountStorageForServerCardsForTest());

  auto account_data = GetAccountWebDataService(0);
  ASSERT_NE(nullptr, account_data);
  auto profile_data = GetProfileWebDataService(0);
  ASSERT_NE(nullptr, profile_data);

  // Check that the data is stored in the account storage (ephemeral), but not
  // in the profile storage (persisted).
  EXPECT_EQ(1U, GetServerCards(account_data).size());
  EXPECT_EQ(0U, GetServerCards(profile_data).size());
  EXPECT_NE(nullptr, GetPaymentsCustomerData(account_data).get());
  EXPECT_EQ(nullptr, GetPaymentsCustomerData(profile_data).get());

  // Simulate the user opting in to full Sync: Make the account primary, and
  // set first-time setup to complete.
  secondary_account_helper::MakeAccountPrimary(profile(), "user@email.com");
  GetSyncService(0)->SetFirstSetupComplete();

  // Wait for Sync to get reconfigured into feature mode.
  ASSERT_TRUE(GetClient(0)->AwaitSyncSetupCompletion(
      /*skip_passphrase_verification=*/false));
  ASSERT_EQ(syncer::SyncService::TransportState::ACTIVE,
            GetSyncService(0)->GetTransportState());
  ASSERT_TRUE(GetSyncService(0)->IsSyncFeatureEnabled());
  ASSERT_TRUE(GetSyncService(0)->IsSyncFeatureActive());
  ASSERT_TRUE(GetSyncService(0)->GetActiveDataTypes().Has(
      syncer::AUTOFILL_WALLET_DATA));

  // PersonalDataManager should have switched to persistent storage.
  EXPECT_TRUE(GetPersonalDataManager(0)->IsSyncFeatureEnabled());
  EXPECT_FALSE(
      GetPersonalDataManager(0)->IsUsingAccountStorageForServerCardsForTest());

  // The data should now be in the profile storage (persisted).
  EXPECT_EQ(0U, GetServerCards(account_data).size());
  EXPECT_EQ(1U, GetServerCards(profile_data).size());
  EXPECT_EQ(nullptr, GetPaymentsCustomerData(account_data).get());
  EXPECT_NE(nullptr, GetPaymentsCustomerData(profile_data).get());
}

IN_PROC_BROWSER_TEST_F(
    SingleClientWalletSecondaryAccountSyncTest,
    SwitchesFromAccountToProfileStorageOnSyncOptInWithAdvancedSetup) {
  ASSERT_TRUE(SetupClients()) << "SetupClients() failed.";
  GetPersonalDataManager(0)->OnSyncServiceInitialized(GetSyncService(0));

  GetFakeServer()->SetWalletData({CreateDefaultSyncWalletCard()});

  // Set up Sync in transport mode for a non-primary account.
  secondary_account_helper::SignInSecondaryAccount(profile(), "user@email.com");
  ASSERT_TRUE(GetClient(0)->AwaitSyncSetupCompletion(
      /*skip_passphrase_verification=*/false));
  ASSERT_EQ(syncer::SyncService::TransportState::ACTIVE,
            GetSyncService(0)->GetTransportState());
  ASSERT_FALSE(GetSyncService(0)->IsSyncFeatureEnabled());
  ASSERT_FALSE(GetSyncService(0)->IsSyncFeatureActive());
  ASSERT_TRUE(GetSyncService(0)->GetActiveDataTypes().Has(
      syncer::AUTOFILL_WALLET_DATA));

  // PersonalDataManager should use (ephemeral) account storage.
  EXPECT_FALSE(GetPersonalDataManager(0)->IsSyncFeatureEnabled());
  EXPECT_TRUE(
      GetPersonalDataManager(0)->IsUsingAccountStorageForServerCardsForTest());

  auto account_data = GetAccountWebDataService(0);
  ASSERT_NE(nullptr, account_data);
  auto profile_data = GetProfileWebDataService(0);
  ASSERT_NE(nullptr, profile_data);

  // Check that the card is stored in the account storage (ephemeral), but not
  // in the profile storage (persisted).
  EXPECT_EQ(1U, GetServerCards(account_data).size());
  EXPECT_EQ(0U, GetServerCards(profile_data).size());

  // Simulate the user opting in to full Sync: First, make the account primary.
  secondary_account_helper::MakeAccountPrimary(profile(), "user@email.com");

  // Now start actually configuring Sync.
  auto setup_handle = GetSyncService(0)->GetSetupInProgressHandle();

  // Adding a primary account triggers a restart of the Sync engine, so it
  // should now be initializing again.
  ASSERT_EQ(syncer::SyncService::TransportState::INITIALIZING,
            GetSyncService(0)->GetTransportState());

  ASSERT_TRUE(GetClient(0)->AwaitEngineInitialization(
      /*skip_passphrase_verification=*/false));

  // Since we're still holding on to the setup-in-progress handle, the data
  // types can't be configured yet.
  ASSERT_EQ(syncer::SyncService::TransportState::PENDING_DESIRED_CONFIGURATION,
            GetSyncService(0)->GetTransportState());

  GetSyncService(0)->OnUserChoseDatatypes(
      /*sync_everything=*/false, syncer::ModelTypeSet(syncer::AUTOFILL));

  // Once the user finishes the setup, we can actually configure.
  setup_handle.reset();
  ASSERT_EQ(syncer::SyncService::TransportState::CONFIGURING,
            GetSyncService(0)->GetTransportState());

  GetSyncService(0)->SetFirstSetupComplete();

  // Wait for Sync to get reconfigured into feature mode.
  ASSERT_TRUE(GetClient(0)->AwaitSyncSetupCompletion(
      /*skip_passphrase_verification=*/false));
  ASSERT_EQ(syncer::SyncService::TransportState::ACTIVE,
            GetSyncService(0)->GetTransportState());
  ASSERT_TRUE(GetSyncService(0)->IsSyncFeatureEnabled());
  ASSERT_TRUE(GetSyncService(0)->IsSyncFeatureActive());
  ASSERT_TRUE(GetSyncService(0)->GetActiveDataTypes().Has(
      syncer::AUTOFILL_WALLET_DATA));

  // PersonalDataManager should have switched to persistent storage.
  EXPECT_TRUE(GetPersonalDataManager(0)->IsSyncFeatureEnabled());
  EXPECT_FALSE(
      GetPersonalDataManager(0)->IsUsingAccountStorageForServerCardsForTest());

  // The card should now be in the profile storage (persisted).
  EXPECT_EQ(0U, GetServerCards(account_data).size());
  EXPECT_EQ(1U, GetServerCards(profile_data).size());
}
#endif  // !defined(OS_CHROMEOS)

// This tests that switching between Sync-the-feature and
// Sync-standalone-transport properly migrates server credit cards between the
// profile (i.e. persisted) and account (i.e. ephemeral) storage.
// When turning off Sync-the-feature via SyncService::RequestStop, you can
// specify either KEEP_DATA or CLEAR_DATA. For full coverage, we test all
// transitions, and each time verify that the card is in the correct storage:
// 1. Start out in Sync-the-feature mode -> profile storage.
// 2. RequestStop(KEEP_DATA) -> account storage.
// 3. Enable Sync-the-feature again -> profile storage.
// 4. RequestStop(CLEAR_DATA) -> account storage.
// 5. Enable Sync-the-feature again -> profile storage.
IN_PROC_BROWSER_TEST_F(SingleClientWalletSyncTest,
                       SwitchesBetweenAccountAndProfileStorageOnTogglingSync) {
  base::test::ScopedFeatureList features;
  features.InitWithFeatures(
      /*enabled_features=*/{switches::kSyncStandaloneTransport,
                            switches::kSyncUSSAutofillWalletData,
                            autofill::features::
                                kAutofillEnableAccountWalletStorage},
      /*disabled_features=*/{});

  ASSERT_TRUE(SetupClients()) << "SetupClients() failed.";
  GetPersonalDataManager(0)->OnSyncServiceInitialized(GetSyncService(0));

  GetFakeServer()->SetWalletData({CreateDefaultSyncWalletCard()});

  // STEP 1. Set up Sync in full feature mode.
  ASSERT_TRUE(GetClient(0)->SetupSync());
  ASSERT_EQ(syncer::SyncService::TransportState::ACTIVE,
            GetSyncService(0)->GetTransportState());
  ASSERT_TRUE(GetSyncService(0)->IsSyncFeatureEnabled());
  ASSERT_TRUE(GetSyncService(0)->IsSyncFeatureActive());
  ASSERT_TRUE(GetSyncService(0)->GetActiveDataTypes().Has(
      syncer::AUTOFILL_WALLET_DATA));

  // PersonalDataManager should use the regular persisted (non-account) storage.
  EXPECT_TRUE(GetPersonalDataManager(0)->IsSyncFeatureEnabled());
  EXPECT_FALSE(
      GetPersonalDataManager(0)->IsUsingAccountStorageForServerCardsForTest());

  auto account_data = GetAccountWebDataService(0);
  ASSERT_NE(nullptr, account_data);
  auto profile_data = GetProfileWebDataService(0);
  ASSERT_NE(nullptr, profile_data);

  // Check that the card is stored in the profile storage (persisted), but not
  // in the account storage (ephemeral).
  EXPECT_EQ(0U, GetServerCards(account_data).size());
  EXPECT_EQ(1U, GetServerCards(profile_data).size());

  // STEP 2. Turn off Sync-the-feature temporarily (e.g. the Sync feature toggle
  // on Android), i.e. leave the Sync data around.
  GetSyncService(0)->RequestStop(syncer::SyncService::KEEP_DATA);

  // Wait for Sync to get reconfigured into transport mode.
  ASSERT_TRUE(GetClient(0)->AwaitSyncSetupCompletion(
      /*skip_passphrase_verification=*/false));
  ASSERT_EQ(syncer::SyncService::TransportState::ACTIVE,
            GetSyncService(0)->GetTransportState());
  ASSERT_FALSE(GetSyncService(0)->IsSyncFeatureEnabled());
  ASSERT_FALSE(GetSyncService(0)->IsSyncFeatureActive());
  ASSERT_TRUE(GetSyncService(0)->GetActiveDataTypes().Has(
      syncer::AUTOFILL_WALLET_DATA));

  // PersonalDataManager should have switched to ephemeral storage.
  EXPECT_FALSE(GetPersonalDataManager(0)->IsSyncFeatureEnabled());
  EXPECT_TRUE(
      GetPersonalDataManager(0)->IsUsingAccountStorageForServerCardsForTest());

  // The card should now be in the account storage (ephemeral). Note that even
  // though we specified KEEP_DATA above, the card is *not* in the profile
  // storage (persisted) anymore, because AUTOFILL_WALLET_DATA is special-cased
  // to always clear its data when Sync is turned off.
  EXPECT_EQ(1U, GetServerCards(account_data).size());
  EXPECT_EQ(0U, GetServerCards(profile_data).size());

  // STEP 3. Turn Sync-the-feature on again.
  GetSyncService(0)->RequestStart();

  // Wait for Sync to get reconfigured into full feature mode again.
  ASSERT_TRUE(GetClient(0)->AwaitSyncSetupCompletion(
      /*skip_passphrase_verification=*/false));
  ASSERT_EQ(syncer::SyncService::TransportState::ACTIVE,
            GetSyncService(0)->GetTransportState());
  ASSERT_TRUE(GetSyncService(0)->IsSyncFeatureEnabled());
  ASSERT_TRUE(GetSyncService(0)->IsSyncFeatureActive());
  ASSERT_TRUE(GetSyncService(0)->GetActiveDataTypes().Has(
      syncer::AUTOFILL_WALLET_DATA));

  // PersonalDataManager should have switched back to persistent storage.
  EXPECT_TRUE(GetPersonalDataManager(0)->IsSyncFeatureEnabled());
  EXPECT_FALSE(
      GetPersonalDataManager(0)->IsUsingAccountStorageForServerCardsForTest());

  // And the card should be in the profile i.e. persistent storage again.
  EXPECT_EQ(0U, GetServerCards(account_data).size());
  EXPECT_EQ(1U, GetServerCards(profile_data).size());

  // STEP 4. Turn off Sync-the-feature again, but this time clear data.
  GetSyncService(0)->RequestStop(syncer::SyncService::CLEAR_DATA);

  // Wait for Sync to get reconfigured into transport mode.
  ASSERT_TRUE(GetClient(0)->AwaitSyncSetupCompletion(
      /*skip_passphrase_verification=*/false));
  ASSERT_EQ(syncer::SyncService::TransportState::ACTIVE,
            GetSyncService(0)->GetTransportState());
  ASSERT_FALSE(GetSyncService(0)->IsSyncFeatureEnabled());
  ASSERT_FALSE(GetSyncService(0)->IsSyncFeatureActive());
  ASSERT_TRUE(GetSyncService(0)->GetActiveDataTypes().Has(
      syncer::AUTOFILL_WALLET_DATA));

  // PersonalDataManager should have switched to ephemeral storage.
  EXPECT_FALSE(GetPersonalDataManager(0)->IsSyncFeatureEnabled());
  EXPECT_TRUE(
      GetPersonalDataManager(0)->IsUsingAccountStorageForServerCardsForTest());

  // The card should now be in the account storage (ephemeral).
  EXPECT_EQ(1U, GetServerCards(account_data).size());
  EXPECT_EQ(0U, GetServerCards(profile_data).size());

  // STEP 5. Turn Sync-the-feature on again.
  GetSyncService(0)->RequestStart();
  GetSyncService(0)->SetFirstSetupComplete();

  // Wait for Sync to get reconfigured into full feature mode again.
  ASSERT_TRUE(GetClient(0)->AwaitSyncSetupCompletion(
      /*skip_passphrase_verification=*/false));
  ASSERT_EQ(syncer::SyncService::TransportState::ACTIVE,
            GetSyncService(0)->GetTransportState());
  ASSERT_TRUE(GetSyncService(0)->IsSyncFeatureEnabled());
  ASSERT_TRUE(GetSyncService(0)->IsSyncFeatureActive());
  ASSERT_TRUE(GetSyncService(0)->GetActiveDataTypes().Has(
      syncer::AUTOFILL_WALLET_DATA));

  // PersonalDataManager should have switched back to persistent storage.
  EXPECT_TRUE(GetPersonalDataManager(0)->IsSyncFeatureEnabled());
  EXPECT_FALSE(
      GetPersonalDataManager(0)->IsUsingAccountStorageForServerCardsForTest());

  // And the card should be in the profile i.e. persistent storage again.
  EXPECT_EQ(0U, GetServerCards(account_data).size());
  EXPECT_EQ(1U, GetServerCards(profile_data).size());
}

INSTANTIATE_TEST_CASE_P(USS,
                        SingleClientWalletSyncTest,
                        ::testing::Values(false, true));
