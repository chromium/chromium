// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/api/autofill_private/autofill_private_api.h"

#include <string>
#include <vector>

#include "base/functional/bind.h"
#include "base/strings/strcat.h"
#include "base/test/bind.h"
#include "base/test/gmock_callback_support.h"
#include "base/test/run_until.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/test_future.h"
#include "chrome/browser/autofill/autofill_entity_data_manager_factory.h"
#include "chrome/browser/autofill/autofill_uitest_util.h"
#include "chrome/browser/extensions/api/autofill_private/autofill_ai_util.h"
#include "chrome/browser/extensions/api/autofill_private/autofill_private_event_router.h"
#include "chrome/browser/extensions/api/autofill_private/autofill_private_event_router_factory.h"
#include "chrome/browser/extensions/extension_apitest.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser.h"
#include "components/autofill/content/browser/test_autofill_client_injector.h"
#include "components/autofill/content/browser/test_content_autofill_client.h"
#include "components/autofill/core/browser/data_manager/addresses/test_address_data_manager.h"
#include "components/autofill/core/browser/data_manager/autofill_ai/entity_data_manager.h"
#include "components/autofill/core/browser/data_manager/autofill_ai/entity_data_manager_test_utils.h"
#include "components/autofill/core/browser/data_manager/payments/payments_data_manager.h"
#include "components/autofill/core/browser/data_model/autofill_ai/entity_instance.h"
#include "components/autofill/core/browser/data_model/payments/credit_card.h"
#include "components/autofill/core/browser/foundations/autofill_client.h"
#include "components/autofill/core/browser/metrics/payments/mandatory_reauth_metrics.h"
#include "components/autofill/core/browser/network/autofill_ai/mock_wallet_pass_access_manager.h"
#include "components/autofill/core/browser/network/autofill_ai/wallet_pass_access_manager.h"
#include "components/autofill/core/browser/payments/payments_autofill_client.h"
#include "components/autofill/core/browser/payments/payments_request_details.h"
#include "components/autofill/core/browser/payments/test_payments_network_interface.h"
#include "components/autofill/core/browser/payments/virtual_card_enrollment_flow.h"
#include "components/autofill/core/browser/permissions/autofill_ai/autofill_ai_permission_utils.h"
#include "components/autofill/core/browser/test_utils/autofill_test_utils.h"
#include "components/autofill/core/browser/test_utils/entity_data_test_utils.h"
#include "components/autofill/core/common/autofill_features.h"
#include "components/autofill/core/common/autofill_payments_features.h"
#include "components/autofill/core/common/autofill_prefs.h"
#include "components/device_reauth/mock_device_authenticator.h"
#include "components/prefs/pref_registry_simple.h"
#include "components/prefs/pref_service.h"
#include "components/sync/test/test_sync_service.h"
#include "components/wallet/core/browser/walletable_permission_utils.h"
#include "components/wallet/core/common/wallet_features.h"
#include "components/wallet/core/common/wallet_prefs.h"
#include "content/public/test/browser_test.h"
#include "extensions/browser/api_test_utils.h"

namespace {

using autofill::EntityInstance;
using ::base::test::RunOnceCallback;
using ::testing::Bool;
using ::testing::Combine;
using ::testing::Eq;
using ::testing::Pointee;
using ::testing::Return;
using ::testing::TestParamInfo;
using ::testing::WithParamInterface;

#if BUILDFLAG(IS_MAC) || BUILDFLAG(IS_WIN) || BUILDFLAG(IS_CHROMEOS)
using autofill::autofill_metrics::MandatoryReauthAuthenticationFlowEvent;

// There are 2 boolean params set in the test suites.
// The first param can be retrieved via `IsFeatureTurnedOn()` which determines
// if the toggle is currently turned on or off. The second param can be
// retrieved via `IsUserAuthSuccessful()` which determines if the user auth was
// successful or not.
class MandatoryReauthSettingsPageMetricsTest
    : public extensions::ExtensionApiTest,
      public WithParamInterface<std::tuple<bool, bool>> {
 public:
  MandatoryReauthSettingsPageMetricsTest() {
#if BUILDFLAG(IS_CHROMEOS)
    // Enable the feature flag for this test.
    scoped_feature_list_.InitAndEnableFeature(
        autofill::features::kAutofillEnablePaymentsMandatoryReauthChromeOs);
#endif  // BUILDFLAG(IS_CHROMEOS)
  }
  MandatoryReauthSettingsPageMetricsTest(
      const MandatoryReauthSettingsPageMetricsTest&) = delete;
  MandatoryReauthSettingsPageMetricsTest& operator=(
      const MandatoryReauthSettingsPageMetricsTest&) = delete;
  ~MandatoryReauthSettingsPageMetricsTest() override = default;

  void SetUpOnMainThread() override {
    ExtensionApiTest::SetUpOnMainThread();
    personal_data_manager()
        .payments_data_manager()
        .SetPaymentMethodsMandatoryReauthEnabled(IsFeatureTurnedOn());
    extensions::AutofillPrivateEventRouterFactory::GetForProfile(
        browser_context())
        ->RebindPersonalDataManagerForTesting(&personal_data_manager());
  }

  void TearDownOnMainThread() override {
    // Unbinding the `autofill_client()` test PDM on the
    // `AutofillPrivateEventRouter`. This removes the test PDM instance added to
    // the observers in `SetUpOnMainThread()` for `AutofillPrivateEventRouter`.
    extensions::AutofillPrivateEventRouterFactory::GetForProfile(
        browser_context())
        ->UnbindPersonalDataManagerForTesting();
  }

  bool IsFeatureTurnedOn() const { return std::get<0>(GetParam()); }

  bool IsUserAuthSuccessful() const { return std::get<1>(GetParam()); }

 protected:
  bool RunAutofillSubtest(const std::string& subtest) {
    const std::string extension_url = "main.html?" + subtest;
    return RunExtensionTest("autofill_private",
                            {.extension_url = extension_url.c_str()},
                            {.load_as_component = true});
  }

  autofill::TestContentAutofillClient* autofill_client() {
    return test_autofill_client_injector_[GetActiveWebContents()];
  }
  autofill::TestPersonalDataManager& personal_data_manager() {
    return autofill_client()->GetPersonalDataManager();
  }

 private:
  content::BrowserContext* browser_context() {
    return GetActiveWebContents()->GetBrowserContext();
  }
#if BUILDFLAG(IS_CHROMEOS)
  base::test::ScopedFeatureList scoped_feature_list_;
#endif  // BUILDFLAG(IS_CHROMEOS)

  autofill::TestAutofillClientInjector<autofill::TestContentAutofillClient>
      test_autofill_client_injector_;
};

// This tests the logging for mandatory reauth opt-in / opt-out flows when
// triggered from the settings page.
IN_PROC_BROWSER_TEST_P(MandatoryReauthSettingsPageMetricsTest,
                       SettingsPageMandatoryReauthToggleSwitching) {
  base::HistogramTester histogram_tester;

  ON_CALL(*static_cast<autofill::payments::MockMandatoryReauthManager*>(
              autofill_client()
                  ->GetPaymentsAutofillClient()
                  ->GetOrCreatePaymentsMandatoryReauthManager()),
          AuthenticateWithMessage)
      .WillByDefault(
          testing::WithArg<1>([auth_success = IsUserAuthSuccessful()](
                                  base::OnceCallback<void(bool)> callback) {
            std::move(callback).Run(auth_success);
          }));

  RunAutofillSubtest("authenticateUserAndFlipMandatoryAuthToggle");

  std::string histogram_name = base::StrCat(
      {"Autofill.PaymentMethods.MandatoryReauth.OptChangeEvent.SettingsPage.",
       IsFeatureTurnedOn() ? "OptOut" : "OptIn"});

  EXPECT_THAT(
      histogram_tester.GetAllSamples(histogram_name),
      testing::ElementsAre(
          base::Bucket(MandatoryReauthAuthenticationFlowEvent::kFlowStarted, 1),
          base::Bucket(
              IsUserAuthSuccessful()
                  ? MandatoryReauthAuthenticationFlowEvent::kFlowSucceeded
                  : MandatoryReauthAuthenticationFlowEvent::kFlowFailed,
              1)));
}

IN_PROC_BROWSER_TEST_P(MandatoryReauthSettingsPageMetricsTest,
                       SettingsPageMandatoryReauthReturnLocalCard) {
  base::HistogramTester histogram_tester;

  ON_CALL(*static_cast<autofill::payments::MockMandatoryReauthManager*>(
              autofill_client()
                  ->GetPaymentsAutofillClient()
                  ->GetOrCreatePaymentsMandatoryReauthManager()),
          AuthenticateWithMessage)
      .WillByDefault(
          testing::WithArg<1>([auth_success = IsUserAuthSuccessful()](
                                  base::OnceCallback<void(bool)> callback) {
            std::move(callback).Run(auth_success);
          }));

  RunAutofillSubtest("getLocalCard");

  std::string histogram_name =
      "Autofill.PaymentMethods.MandatoryReauth.AuthEvent.SettingsPage.EditCard";

  std::vector<base::Bucket> expected_histogram_buckets;
  if (IsFeatureTurnedOn()) {
    expected_histogram_buckets = {
        base::Bucket(MandatoryReauthAuthenticationFlowEvent::kFlowStarted, 1),
        base::Bucket(
            IsUserAuthSuccessful()
                ? MandatoryReauthAuthenticationFlowEvent::kFlowSucceeded
                : MandatoryReauthAuthenticationFlowEvent::kFlowFailed,
            1)};
  }

  EXPECT_EQ(histogram_tester.GetAllSamples(histogram_name),
            expected_histogram_buckets);
}

INSTANTIATE_TEST_SUITE_P(,
                         MandatoryReauthSettingsPageMetricsTest,
                         Combine(Bool(), Bool()));
#endif  // BUILDFLAG(IS_MAC) || BUILDFLAG(IS_WIN)

class MockSyncService : public syncer::TestSyncService {
 public:
  MOCK_METHOD(syncer::DataTypeSet, GetActiveDataTypes, (), (const override));
};

class AutofillPrivateApiBrowserTest : public extensions::ExtensionApiTest {
 public:
  AutofillPrivateApiBrowserTest() {
    feature_list_.InitWithFeaturesAndParameters(
        /*enabled_features=*/
        {
            {autofill::features::kAutofillAiWithDataSchema, {}},
            {autofill::features::kAutofillAiAvailableByDefault, {}},
            {autofill::features::kAutofillAiWalletFlightReservation, {}},
            {autofill::features::kAutofillAiWalletVehicleRegistration, {}},
            {autofill::features::kAutofillEnableSaveToWalletFromSettings, {}},
            {wallet::features::kWalletablePassDetection,
             {{wallet::features::kWalletablePassDetectionCountryAllowlist.name,
               "US"}}},
        },
        /*disabled_features=*/
        {});
  }
  AutofillPrivateApiBrowserTest(const AutofillPrivateApiBrowserTest&) = delete;
  AutofillPrivateApiBrowserTest& operator=(
      const AutofillPrivateApiBrowserTest&) = delete;
  ~AutofillPrivateApiBrowserTest() override = default;
  void SetUpOnMainThread() override {
    ExtensionApiTest::SetUpOnMainThread();
    payments_data_manager().SetSyncingForTest(/*is_syncing_for_test=*/true);
  }

  void TearDownOnMainThread() override {
    InProcessBrowserTest::TearDownOnMainThread();
  }

  autofill::TestAddressDataManager& address_data_manager() {
    return personal_data_manager().test_address_data_manager();
  }
  autofill::TestContentAutofillClient* autofill_client() {
    return test_autofill_client_injector_[GetActiveWebContents()];
  }
  autofill::TestPaymentsDataManager& payments_data_manager() {
    return personal_data_manager().test_payments_data_manager();
  }
  autofill::TestPersonalDataManager& personal_data_manager() {
    return autofill_client()->GetPersonalDataManager();
  }

 protected:
  bool RunAutofillSubtest(const std::string& subtest) {
    const std::string extension_url = "main.html?" + subtest;
    return RunExtensionTest("autofill_private",
                            {.extension_url = extension_url.c_str()},
                            {.load_as_component = true});
  }

 private:
  autofill::TestAutofillClientInjector<autofill::TestContentAutofillClient>
      test_autofill_client_injector_;
  base::test::ScopedFeatureList feature_list_;
};

// Test to verify all the CVCs(server and local) are bulk deleted when the API
// is called.
IN_PROC_BROWSER_TEST_F(AutofillPrivateApiBrowserTest, BulkDeleteAllCvcs) {
  autofill::CreditCard local_card =
      autofill::test::WithCvc(autofill::test::GetCreditCard(), u"789");
  autofill::CreditCard server_card =
      autofill::test::WithCvc(autofill::test::GetMaskedServerCard(), u"098");
  payments_data_manager().AddCreditCard(local_card);
  payments_data_manager().AddServerCreditCard(server_card);

  // Verify that cards are same as above and the CVCs are present for both of
  // them.
  ASSERT_EQ(payments_data_manager().GetCreditCards().size(), 2u);
  for (const autofill::CreditCard* card :
       payments_data_manager().GetCreditCards()) {
    EXPECT_FALSE(card->cvc().empty());
    if (card->record_type() ==
        autofill::CreditCard::RecordType::kMaskedServerCard) {
      EXPECT_EQ(card->number(), server_card.number());
      EXPECT_EQ(card->cvc(), server_card.cvc());
    } else {
      EXPECT_EQ(card->number(), local_card.number());
      EXPECT_EQ(card->cvc(), local_card.cvc());
    }
  }

  RunAutofillSubtest("bulkDeleteAllCvcs");

  // Verify that cards are same as above and the CVCs are deleted for both of
  // them.
  ASSERT_EQ(payments_data_manager().GetCreditCards().size(), 2u);
  for (const autofill::CreditCard* card :
       payments_data_manager().GetCreditCards()) {
    EXPECT_TRUE(card->cvc().empty());
    if (card->record_type() ==
        autofill::CreditCard::RecordType::kMaskedServerCard) {
      EXPECT_EQ(card->number(), server_card.number());
    } else {
      EXPECT_EQ(card->number(), local_card.number());
    }
  }
}

IN_PROC_BROWSER_TEST_F(AutofillPrivateApiBrowserTest,
                       LogServerCardLinkClicked) {
  base::HistogramTester histogram_tester;
  ASSERT_TRUE(RunAutofillSubtest("logServerCardLinkClicked"));
  histogram_tester.ExpectUniqueSample(
      "Autofill.ServerCardLinkClicked",
      autofill::AutofillMetrics::PaymentsSigninState::kSignedOut, 1);
}

IN_PROC_BROWSER_TEST_F(AutofillPrivateApiBrowserTest, RemoveVirtualCard) {
  using autofill::payments::TestPaymentsNetworkInterface;
  autofill::payments::MockMultipleRequestPaymentsNetworkInterface*
      mock_multiple_request_payments_network_interface_;
  autofill::payments::UpdateVirtualCardEnrollmentRequestDetails details;
  auto mock_multiple_request_payments_network_interface = std::make_unique<
      autofill::payments::MockMultipleRequestPaymentsNetworkInterface>(
      autofill_client()->GetURLLoaderFactory(),
      *autofill_client()->GetIdentityManager());
  mock_multiple_request_payments_network_interface_ =
      mock_multiple_request_payments_network_interface.get();
  autofill_client()
      ->GetPaymentsAutofillClient()
      ->set_multiple_request_payments_network_interface(
          std::move(mock_multiple_request_payments_network_interface));
  EXPECT_CALL(*mock_multiple_request_payments_network_interface_,
              UpdateVirtualCardEnrollment(testing::_, testing::_))
      .WillOnce(
          testing::DoAll(testing::SaveArg<0>(&details),
                         Return(autofill::payments::RequestId("11223344"))));
  // Required for adding the server card.
  payments_data_manager().SetSyncingForTest(
      /*is_syncing_for_test=*/true);
  autofill::CreditCard virtual_card = autofill::test::GetVirtualCard();
  virtual_card.set_server_id("a123");
  virtual_card.set_instrument_id(123);
  payments_data_manager().AddServerCreditCard(virtual_card);

  EXPECT_TRUE(RunAutofillSubtest("removeVirtualCard"));

  EXPECT_EQ(details.virtual_card_enrollment_request_type,
            autofill::VirtualCardEnrollmentRequestType::kUnenroll);
}

IN_PROC_BROWSER_TEST_F(AutofillPrivateApiBrowserTest,
                       SetAutofillSyncToggleEnabled) {
  syncer::TestSyncService test_sync_service;
  address_data_manager().SetSyncServiceForTest(&test_sync_service);
  test_sync_service.GetUserSettings()->SetSelectedType(
      syncer::UserSelectableType::kAutofill, false);
  EXPECT_FALSE(test_sync_service.GetUserSettings()->GetSelectedTypes().Has(
      syncer::UserSelectableType::kAutofill));
  EXPECT_TRUE(RunAutofillSubtest("setAutofillSyncToggleEnabled"));
  EXPECT_TRUE(test_sync_service.GetUserSettings()->GetSelectedTypes().Has(
      syncer::UserSelectableType::kAutofill));
}

// TODO(crbug.com/40759629): Fix and re-enable this test.
#if BUILDFLAG(IS_MAC) || BUILDFLAG(IS_LINUX)
#define MAYBE_EntityInstances DISABLED_EntityInstances
#else
#define MAYBE_EntityInstances EntityInstances
#endif
IN_PROC_BROWSER_TEST_F(AutofillPrivateApiBrowserTest, MAYBE_EntityInstances) {
  // Test that loading, adding, editing and deleting entity instances works.
  ASSERT_TRUE(RunAutofillSubtest("loadEmptyEntityInstancesList"));
  ASSERT_TRUE(RunAutofillSubtest("addEntityInstance"));
  ASSERT_TRUE(RunAutofillSubtest("addEntityInstanceWithIncompleteDate"));
  ASSERT_TRUE(RunAutofillSubtest("getEntityInstanceByGuid"));
  ASSERT_TRUE(RunAutofillSubtest("loadFirstEntityInstance"));
  ASSERT_TRUE(RunAutofillSubtest("updateEntityInstance"));
  ASSERT_TRUE(RunAutofillSubtest("loadUpdatedEntityInstance"));
  ASSERT_TRUE(RunAutofillSubtest("removeEntityInstance"));
  ASSERT_TRUE(RunAutofillSubtest("loadEmptyEntityInstancesList"));
  ASSERT_TRUE(RunAutofillSubtest("testExpectedLabelsAreGenerated"));
  ASSERT_TRUE(RunAutofillSubtest("shouldAuthenticateToView"));
  // Test that retrieving general entity type information works.
  ASSERT_TRUE(RunAutofillSubtest("getWritableEntityTypes"));
  ASSERT_TRUE(RunAutofillSubtest("getAllAttributeTypesForEntityTypeName"));
  ASSERT_TRUE(RunAutofillSubtest("getRequiredAttributeTypesForEntityTypeName"));
}

IN_PROC_BROWSER_TEST_F(AutofillPrivateApiBrowserTest, TypedEntityInstances) {
  ASSERT_TRUE(RunAutofillSubtest("testEntityTypeInEntityInstanceWithLabels"));
}

IN_PROC_BROWSER_TEST_F(AutofillPrivateApiBrowserTest,
                       GetEmptyPayOverTimeIssuerList) {
  ASSERT_TRUE(RunAutofillSubtest("getEmptyPayOverTimeIssuerList"));
}

IN_PROC_BROWSER_TEST_F(AutofillPrivateApiBrowserTest, SetAutofillAiOptIn) {
  autofill_client()->set_entity_data_manager(
      autofill::AutofillEntityDataManagerFactory::GetForProfile(profile()));
  autofill_client()->SetUpPrefsAndIdentityForAutofillAi();
  EXPECT_TRUE(autofill::SetAutofillAiOptInStatus(
      *autofill_client(), autofill::AutofillAiOptInStatus::kOptedOut));
  EXPECT_FALSE(autofill::GetAutofillAiOptInStatus(*autofill_client()));
  EXPECT_TRUE(RunAutofillSubtest("verifyUserOptedOutOfAutofillAi"));

  base::test::TestFuture<autofill::AutofillClient::IphFeature>
      feature_used_future;
  autofill_client()->set_notify_iph_feature_used_mock_callback(
      feature_used_future.GetRepeatingCallback());

  EXPECT_TRUE(RunAutofillSubtest("optIntoAutofillAi"));
  EXPECT_EQ(feature_used_future.Get(),
            autofill::AutofillClient::IphFeature::kAutofillAi);
  EXPECT_TRUE(autofill::GetAutofillAiOptInStatus(*autofill_client()));
  EXPECT_TRUE(RunAutofillSubtest("verifyUserOptedIntoAutofillAi"));

  EXPECT_TRUE(RunAutofillSubtest("optOutOfAutofillAi"));
  EXPECT_FALSE(autofill::GetAutofillAiOptInStatus(*autofill_client()));
  EXPECT_TRUE(RunAutofillSubtest("verifyUserOptedOutOfAutofillAi"));
}

IN_PROC_BROWSER_TEST_F(AutofillPrivateApiBrowserTest,
                       GetAllWritableEntityTypes_DoesNotIncludeReadOnlyTypes) {
  ASSERT_TRUE(RunAutofillSubtest("getWritableEntityTypes"));
}

IN_PROC_BROWSER_TEST_F(AutofillPrivateApiBrowserTest,
                       SetWalletablePassDetectionOptInStatus) {
  autofill_client()->GetPrefs()->registry()->RegisterDictionaryPref(
      wallet::prefs::kWalletablePassDetectionOptInStatus);
  autofill_client()->SetUpPrefsAndIdentityForAutofillAi();
  EXPECT_TRUE(RunAutofillSubtest("optIntoWalletablePassDetection"));
  EXPECT_TRUE(RunAutofillSubtest("verifyUserOptedIntoWalletablePassDetection"));

  EXPECT_TRUE(RunAutofillSubtest("optOutOfWalletablePassDetection"));
  EXPECT_TRUE(
      RunAutofillSubtest("verifyUserOptedOutOfWalletablePassDetection"));
}

IN_PROC_BROWSER_TEST_F(
    AutofillPrivateApiBrowserTest,
    SetWalletablePassDetectionOptInStatus_SwitchEligibility) {
  autofill_client()->GetPrefs()->registry()->RegisterDictionaryPref(
      wallet::prefs::kWalletablePassDetectionOptInStatus);
  autofill_client()->SetUpPrefsAndIdentityForAutofillAi();

  // Ensure we are eligible initially (US is usually supported).
  autofill_client()->SetVariationConfigCountryCode(
      autofill::GeoIpCountryCode("US"));
  ASSERT_TRUE(wallet::IsEligibleForWalletablePassDetection(
      autofill_client()->GetIdentityManager(),
      wallet::GeoIpCountryCode(
          autofill_client()->GetVariationConfigCountryCode().value())));

  EXPECT_TRUE(RunAutofillSubtest("optIntoWalletablePassDetection"));
  EXPECT_TRUE(RunAutofillSubtest("verifyUserOptedIntoWalletablePassDetection"));

  EXPECT_TRUE(RunAutofillSubtest("optOutOfWalletablePassDetection"));
  EXPECT_TRUE(
      RunAutofillSubtest("verifyUserOptedOutOfWalletablePassDetection"));

  // Become ineligible.
  autofill_client()->SetVariationConfigCountryCode(
      autofill::GeoIpCountryCode("XX"));
  ASSERT_FALSE(wallet::IsEligibleForWalletablePassDetection(
      autofill_client()->GetIdentityManager(),
      wallet::GeoIpCountryCode(
          autofill_client()->GetVariationConfigCountryCode().value())));

  // Verify that we cannot opt into Walletable Pass Detection anymore.
  EXPECT_TRUE(
      RunAutofillSubtest("optIntoWalletablePassDetectionExpectingFailure"));
  EXPECT_TRUE(
      RunAutofillSubtest("verifyUserOptedOutOfWalletablePassDetection"));
}

IN_PROC_BROWSER_TEST_F(AutofillPrivateApiBrowserTest,
                       AddEntityInstance_SavesToWalletIfEligible) {
  autofill_client()->set_entity_data_manager(
      autofill::AutofillEntityDataManagerFactory::GetForProfile(profile()));
  autofill_client()->SetUpPrefsAndIdentityForAutofillAi();
  autofill_client()->SetVariationConfigCountryCode(
      autofill::GeoIpCountryCode("US"));

  testing::NiceMock<MockSyncService> mock_sync_service;
  address_data_manager().SetSyncServiceForTest(&mock_sync_service);
  autofill_client()->set_sync_service(&mock_sync_service);

  autofill_client()->GetSyncService()->GetUserSettings()->SetSelectedType(
      syncer::UserSelectableType::kPayments, true);
  ON_CALL(mock_sync_service, GetActiveDataTypes())
      .WillByDefault(Return(syncer::DataTypeSet{syncer::AUTOFILL_VALUABLE}));

  EntityInstance entity_instance =
      autofill::test::GetVehicleEntityInstanceWithRandomGuid();

  extensions::api::autofill_private::EntityInstance api_entity =
      extensions::autofill_ai_util::EntityInstanceToPrivateApiEntityInstance(
          entity_instance, "en-US", /*entity_supports_wallet_storage=*/true);

  // Explicitly request storage in Wallet.
  api_entity.stored_in_wallet = true;

  base::ListValue args;
  args.Append(api_entity.ToValue());
  std::string json_args;
  base::JSONWriter::Write(args, &json_args);

  auto* entity_data_manager =
      autofill::AutofillEntityDataManagerFactory::GetForProfile(profile());
  ASSERT_TRUE(entity_data_manager);

  auto function = base::MakeRefCounted<
      extensions::AutofillPrivateAddOrUpdateEntityInstanceFunction>();
  function->SetRenderFrameHost(GetActiveWebContents()->GetPrimaryMainFrame());

  ASSERT_TRUE(extensions::api_test_utils::RunFunction(function.get(), json_args,
                                                      profile()));

  autofill::EntityDataChangedWaiter(entity_data_manager).Wait();
  base::optional_ref<const EntityInstance> saved_entity =
      entity_data_manager->GetEntityInstance(entity_instance.guid());
  ASSERT_TRUE(saved_entity.has_value()) << "Entity should exist after save";

  EXPECT_EQ(saved_entity->record_type(),
            EntityInstance::RecordType::kServerWallet);
}

IN_PROC_BROWSER_TEST_F(
    AutofillPrivateApiBrowserTest,
    AddEntityInstance_FallsBackToLocalIfSaveToWalletOnPaymentsOffToggle) {
  autofill_client()->set_entity_data_manager(
      autofill::AutofillEntityDataManagerFactory::GetForProfile(profile()));
  autofill_client()->SetUpPrefsAndIdentityForAutofillAi();
  autofill_client()->SetVariationConfigCountryCode(
      autofill::GeoIpCountryCode("US"));

  syncer::TestSyncService test_sync_service;
  address_data_manager().SetSyncServiceForTest(&test_sync_service);
  test_sync_service.GetUserSettings()->SetSelectedType(
      syncer::UserSelectableType::kPayments, false);

  EntityInstance entity_instance =
      autofill::test::GetVehicleEntityInstanceWithRandomGuid();

  extensions::api::autofill_private::EntityInstance api_entity =
      extensions::autofill_ai_util::EntityInstanceToPrivateApiEntityInstance(
          entity_instance, "en-US", /*entity_supports_wallet_storage=*/true);

  // Explicitly request storage in Wallet.
  api_entity.stored_in_wallet = true;

  base::ListValue args;
  args.Append(api_entity.ToValue());
  std::string json_args;
  base::JSONWriter::Write(args, &json_args);

  auto* entity_data_manager =
      autofill::AutofillEntityDataManagerFactory::GetForProfile(profile());
  ASSERT_TRUE(entity_data_manager);

  auto function = base::MakeRefCounted<
      extensions::AutofillPrivateAddOrUpdateEntityInstanceFunction>();
  function->SetRenderFrameHost(GetActiveWebContents()->GetPrimaryMainFrame());

  ASSERT_TRUE(extensions::api_test_utils::RunFunction(function.get(), json_args,
                                                      profile()));
  autofill::EntityDataChangedWaiter(entity_data_manager).Wait();
  base::optional_ref<const EntityInstance> saved_entity =
      entity_data_manager->GetEntityInstance(entity_instance.guid());
  ASSERT_TRUE(saved_entity.has_value()) << "Entity should exist after save";

  EXPECT_EQ(saved_entity->record_type(), EntityInstance::RecordType::kLocal);
}

class AutofillPrivateApiSavePrivatePassToWalletTest
    : public AutofillPrivateApiBrowserTest {
 public:
  AutofillPrivateApiSavePrivatePassToWalletTest() = default;

  void SetUpOnMainThread() override {
    AutofillPrivateApiBrowserTest::SetUpOnMainThread();

    autofill_client()->set_entity_data_manager(
        autofill::AutofillEntityDataManagerFactory::GetForProfile(profile()));
    autofill_client()->SetUpPrefsAndIdentityForAutofillAi();
    autofill_client()->SetVariationConfigCountryCode(
        autofill::GeoIpCountryCode("US"));

    autofill_client()->set_wallet_pass_access_manager(
        std::make_unique<
            testing::NiceMock<autofill::MockWalletPassAccessManager>>());
    address_data_manager().SetSyncServiceForTest(&mock_sync_service_);
    autofill_client()->set_sync_service(&mock_sync_service_);
    autofill_client()->GetSyncService()->GetUserSettings()->SetSelectedType(
        syncer::UserSelectableType::kPayments, true);
    ON_CALL(mock_sync_service_, GetActiveDataTypes())
        .WillByDefault(Return(syncer::DataTypeSet{syncer::AUTOFILL_VALUABLE}));
  }

  autofill::MockWalletPassAccessManager& wallet_manager() {
    return static_cast<autofill::MockWalletPassAccessManager&>(
        *autofill_client()->GetWalletPassAccessManager());
  }

 private:
  base::test::ScopedFeatureList feature_list_{
      autofill::features::kAutofillAiWalletPrivatePasses};
  testing::NiceMock<MockSyncService> mock_sync_service_;
};

IN_PROC_BROWSER_TEST_F(AutofillPrivateApiSavePrivatePassToWalletTest,
                       AddPassport_SavesToWallet_WhenEligible) {
  EntityInstance entity_instance = autofill::test::GetPassportEntityInstance(
      {.record_type = EntityInstance::RecordType::kServerWallet});

  extensions::api::autofill_private::EntityInstance api_entity =
      extensions::autofill_ai_util::EntityInstanceToPrivateApiEntityInstance(
          entity_instance, "en-US", /*entity_supports_wallet_storage=*/true);
  api_entity.stored_in_wallet = true;

  base::ListValue args;
  args.Append(api_entity.ToValue());
  std::string json_args;
  base::JSONWriter::Write(args, &json_args);

  EXPECT_CALL(wallet_manager(), SaveWalletEntityInstance)
      .WillOnce(RunOnceCallback<1>(
          autofill::test::MaskEntityInstance(entity_instance)));

  auto function = base::MakeRefCounted<
      extensions::AutofillPrivateAddOrUpdateEntityInstanceFunction>();
  function->SetRenderFrameHost(GetActiveWebContents()->GetPrimaryMainFrame());

  ASSERT_TRUE(extensions::api_test_utils::RunFunction(function.get(), json_args,
                                                      profile()));

  // Verify EDM was updated
  auto* entity_data_manager =
      autofill::AutofillEntityDataManagerFactory::GetForProfile(profile());
  autofill::EntityDataChangedWaiter(entity_data_manager).Wait();

  base::optional_ref<const EntityInstance> saved_entity =
      entity_data_manager->GetEntityInstance(entity_instance.guid());
  ASSERT_TRUE(saved_entity.has_value());
  EXPECT_EQ(saved_entity->record_type(),
            EntityInstance::RecordType::kServerWallet);
}

IN_PROC_BROWSER_TEST_F(AutofillPrivateApiSavePrivatePassToWalletTest,
                       AddPassport_SavesToLocal_WhenRequestFails) {
  EntityInstance entity_instance = autofill::test::GetPassportEntityInstance(
      {.record_type = EntityInstance::RecordType::kServerWallet});
  extensions::api::autofill_private::EntityInstance api_entity =
      extensions::autofill_ai_util::EntityInstanceToPrivateApiEntityInstance(
          entity_instance, "en-US", /*entity_supports_wallet_storage=*/true);
  api_entity.stored_in_wallet = true;

  base::ListValue args;
  args.Append(api_entity.ToValue());
  std::string json_args;
  base::JSONWriter::Write(args, &json_args);

  EXPECT_CALL(wallet_manager(), SaveWalletEntityInstance)
      .WillOnce(RunOnceCallback<1>(std::nullopt));

  auto function = base::MakeRefCounted<
      extensions::AutofillPrivateAddOrUpdateEntityInstanceFunction>();
  function->SetRenderFrameHost(GetActiveWebContents()->GetPrimaryMainFrame());

  ASSERT_TRUE(extensions::api_test_utils::RunFunction(function.get(), json_args,
                                                      profile()));

  auto* entity_data_manager =
      autofill::AutofillEntityDataManagerFactory::GetForProfile(profile());
  autofill::EntityDataChangedWaiter(entity_data_manager).Wait();

  base::optional_ref<const EntityInstance> saved_entity =
      entity_data_manager->GetEntityInstance(entity_instance.guid());
  ASSERT_TRUE(saved_entity.has_value());

  // The API should have saved the entity locally.
  EXPECT_EQ(saved_entity->record_type(), EntityInstance::RecordType::kLocal);
}

#if BUILDFLAG(IS_MAC) || BUILDFLAG(IS_WIN) || BUILDFLAG(IS_ANDROID) || \
    BUILDFLAG(IS_CHROMEOS)
class AutofillPrivateApiAuthToViewSensitiveEntityTest
    : public AutofillPrivateApiBrowserTest,
      public WithParamInterface<std::tuple<bool, bool>> {
 public:
  AutofillPrivateApiAuthToViewSensitiveEntityTest() {
    if (IsFeatureEnabled()) {
      feature_list_.InitAndEnableFeature(
          autofill::features::kAutofillAiReauthRequired);
    } else {
      feature_list_.InitAndDisableFeature(
          autofill::features::kAutofillAiReauthRequired);
    }
  }

  void SetUpOnMainThread() override {
    AutofillPrivateApiBrowserTest::SetUpOnMainThread();

    autofill::prefs::SetAutofillAiReauthBeforeFillingEnabled(
        autofill_client()->GetPrefs(), IsPrefEnabled());
  }

  bool IsPrefEnabled() const { return std::get<0>(GetParam()); }
  bool IsFeatureEnabled() const { return std::get<1>(GetParam()); }

 private:
  base::test::ScopedFeatureList feature_list_;
};

// Tests the AuthenticateUserBeforeViewingEntityData function under different
// pref and feature flag combinations.
IN_PROC_BROWSER_TEST_P(AutofillPrivateApiAuthToViewSensitiveEntityTest,
                       AuthenticateUserBeforeViewingEntityData) {
  const bool should_attempt_auth = IsPrefEnabled() && IsFeatureEnabled();

  if (should_attempt_auth) {
    // Authentication Successful
    {
      auto authenticator =
          std::make_unique<device_reauth::MockDeviceAuthenticator>();
      EXPECT_CALL(*authenticator, CanAuthenticateWithBiometricOrScreenLock)
          .WillOnce(Return(true));
      EXPECT_CALL(*authenticator, AuthenticateWithMessage)
          .WillOnce(RunOnceCallback<1>(true));
      autofill_client()->SetDeviceAuthenticator(std::move(authenticator));

      auto function = base::MakeRefCounted<
          extensions::
              AutofillPrivateAuthenticateUserBeforeViewingEntityDataFunction>();
      function->SetRenderFrameHost(
          GetActiveWebContents()->GetPrimaryMainFrame());

      std::optional<base::Value> result =
          extensions::api_test_utils::RunFunctionAndReturnSingleResult(
              function.get(), "[]", profile());
      ASSERT_TRUE(result);
      EXPECT_TRUE(result->GetBool()) << "Auth should succeed";
    }

    //  Authentication Failed
    {
      auto authenticator =
          std::make_unique<device_reauth::MockDeviceAuthenticator>();
      EXPECT_CALL(*authenticator, CanAuthenticateWithBiometricOrScreenLock)
          .WillOnce(Return(true));
      EXPECT_CALL(*authenticator, AuthenticateWithMessage)
          .WillOnce(RunOnceCallback<1>(false));
      autofill_client()->SetDeviceAuthenticator(std::move(authenticator));

      auto function = base::MakeRefCounted<
          extensions::
              AutofillPrivateAuthenticateUserBeforeViewingEntityDataFunction>();
      function->SetRenderFrameHost(
          GetActiveWebContents()->GetPrimaryMainFrame());

      std::optional<base::Value> result =
          extensions::api_test_utils::RunFunctionAndReturnSingleResult(
              function.get(), "[]", profile());
      ASSERT_TRUE(result);
      EXPECT_FALSE(result->GetBool()) << "Auth should fail";
    }
  } else {
    // Authentication should be SKIPPED, either because the feature or the pref
    // are off.
    auto authenticator =
        std::make_unique<device_reauth::MockDeviceAuthenticator>();
    EXPECT_CALL(*authenticator, CanAuthenticateWithBiometricOrScreenLock)
        .Times(0);
    EXPECT_CALL(*authenticator, AuthenticateWithMessage).Times(0);
    autofill_client()->SetDeviceAuthenticator(std::move(authenticator));

    auto function = base::MakeRefCounted<
        extensions::
            AutofillPrivateAuthenticateUserBeforeViewingEntityDataFunction>();
    function->SetRenderFrameHost(GetActiveWebContents()->GetPrimaryMainFrame());

    std::optional<base::Value> result =
        extensions::api_test_utils::RunFunctionAndReturnSingleResult(
            function.get(), "[]", profile());
    ASSERT_TRUE(result);
    EXPECT_TRUE(result->GetBool())
        << "Result should be true as auth is skipped";
  }
}

// Instantiate the test suite with all combinations of the boolean parameters.
// The first boolean is for the preference, the second for the feature flag.
INSTANTIATE_TEST_SUITE_P(
    All,
    AutofillPrivateApiAuthToViewSensitiveEntityTest,
    Combine(Bool(), Bool()),
    [](const TestParamInfo<std::tuple<bool, bool>>& info) {
      return std::string(std::get<0>(info.param)
                             ? "AuthenticationRequired_PrefOn_"
                             : "AuthenticationRequired_PreOff_") +
             std::string(std::get<1>(info.param) ? "FeatureOn" : "FeatureOff");
    });

class AutofillPrivateApiGetEntityInstanceAuthEnabledTest
    : public AutofillPrivateApiBrowserTest {
 public:
  AutofillPrivateApiGetEntityInstanceAuthEnabledTest() = default;

  autofill::EntityDataManager* entity_data_manager() {
    return autofill::AutofillEntityDataManagerFactory::GetForProfile(profile());
  }

  [[nodiscard]] bool AddEntity(const EntityInstance& entity_instance) {
    entity_data_manager()->AddOrUpdateEntityInstance(entity_instance);
    return base::test::RunUntil([&]() {
      return entity_data_manager()
          ->GetEntityInstance(entity_instance.guid())
          .has_value();
    });
  }

 private:
  base::test::ScopedFeatureList feature_list_{
      autofill::features::kAutofillAiReauthRequired};
};

IN_PROC_BROWSER_TEST_F(
    AutofillPrivateApiGetEntityInstanceAuthEnabledTest,
    AuthenticateUserBeforeReturningEntityData_AuthenticationProcessSucceeds) {
  EntityInstance entity_instance =
      autofill::test::GetPassportEntityInstanceWithRandomGuid();
  ASSERT_TRUE(AddEntity(entity_instance));

  auto authenticator =
      std::make_unique<device_reauth::MockDeviceAuthenticator>();
  EXPECT_CALL(*authenticator, CanAuthenticateWithBiometricOrScreenLock)
      .WillOnce(Return(true));
  EXPECT_CALL(*authenticator, AuthenticateWithMessage)
      .WillOnce(RunOnceCallback<1>(true));
  autofill_client()->SetDeviceAuthenticator(std::move(authenticator));

  auto function = base::MakeRefCounted<
      extensions::AutofillPrivateGetEntityInstanceByGuidFunction>();
  function->SetRenderFrameHost(GetActiveWebContents()->GetPrimaryMainFrame());

  const std::string guid = entity_instance.guid().value();
  const std::string args = base::StrCat({"[\"", guid, "\"]"});
  std::optional<base::Value> result =
      extensions::api_test_utils::RunFunctionAndReturnSingleResult(
          function.get(), args, profile());

  ASSERT_TRUE(result);
  ASSERT_TRUE(result->is_dict());
  EXPECT_THAT(result->GetDict().FindString("guid"), Pointee(Eq(guid)));
}

IN_PROC_BROWSER_TEST_F(
    AutofillPrivateApiGetEntityInstanceAuthEnabledTest,
    AuthenticateUserBeforeReturningEntityData_AuthenticationProcessFails) {
  EntityInstance entity_instance =
      autofill::test::GetPassportEntityInstanceWithRandomGuid();
  ASSERT_TRUE(AddEntity(entity_instance));

  auto authenticator =
      std::make_unique<device_reauth::MockDeviceAuthenticator>();
  EXPECT_CALL(*authenticator, CanAuthenticateWithBiometricOrScreenLock)
      .WillOnce(Return(true));
  EXPECT_CALL(*authenticator, AuthenticateWithMessage)
      .WillOnce(RunOnceCallback<1>(false));
  autofill_client()->SetDeviceAuthenticator(std::move(authenticator));

  auto function = base::MakeRefCounted<
      extensions::AutofillPrivateGetEntityInstanceByGuidFunction>();
  function->SetRenderFrameHost(GetActiveWebContents()->GetPrimaryMainFrame());

  const std::string guid = entity_instance.guid().value();
  const std::string args = base::StrCat({"[\"", guid, "\"]"});
  std::optional<base::Value> result =
      extensions::api_test_utils::RunFunctionAndReturnSingleResult(
          function.get(), args, profile());
  EXPECT_FALSE(result.has_value());
}

// Tests that if the authentication pref is off, no authentication is required.
IN_PROC_BROWSER_TEST_F(AutofillPrivateApiGetEntityInstanceAuthEnabledTest,
                       AuthenticateUserBeforeReturningEntityData_PrefOff) {
  autofill::prefs::SetAutofillAiReauthBeforeFillingEnabled(
      autofill_client()->GetPrefs(), false);

  EntityInstance entity_instance =
      autofill::test::GetPassportEntityInstanceWithRandomGuid();
  ASSERT_TRUE(AddEntity(entity_instance));
  const std::string guid = entity_instance.guid().value();
  const std::string args = base::StrCat({"[\"", guid, "\"]"});

  auto authenticator =
      std::make_unique<device_reauth::MockDeviceAuthenticator>();
  EXPECT_CALL(*authenticator, CanAuthenticateWithBiometricOrScreenLock)
      .Times(0);
  EXPECT_CALL(*authenticator, AuthenticateWithMessage).Times(0);
  autofill_client()->SetDeviceAuthenticator(std::move(authenticator));

  auto function = base::MakeRefCounted<
      extensions::AutofillPrivateGetEntityInstanceByGuidFunction>();
  function->SetRenderFrameHost(GetActiveWebContents()->GetPrimaryMainFrame());

  std::optional<base::Value> result =
      extensions::api_test_utils::RunFunctionAndReturnSingleResult(
          function.get(), args, profile());
  ASSERT_TRUE(result);
  ASSERT_TRUE(result->is_dict());
  EXPECT_THAT(result->GetDict().FindString("guid"), Pointee(Eq(guid)));
}

IN_PROC_BROWSER_TEST_F(
    AutofillPrivateApiGetEntityInstanceAuthEnabledTest,
    NonSensitiveData_DoNotAuthenticateUserBeforeReturningEntityData) {
  // Passport number is the only sensitive field, by making it empty
  // authentications is not required.
  EntityInstance entity_instance =
      autofill::test::GetPassportEntityInstanceWithRandomGuid({.number = u""});
  CHECK(entity_data_manager());
  ASSERT_TRUE(AddEntity(entity_instance));

  auto authenticator =
      std::make_unique<device_reauth::MockDeviceAuthenticator>();
  EXPECT_CALL(*authenticator, AuthenticateWithMessage).Times(0);
  autofill_client()->SetDeviceAuthenticator(std::move(authenticator));

  auto function = base::MakeRefCounted<
      extensions::AutofillPrivateGetEntityInstanceByGuidFunction>();
  function->SetRenderFrameHost(GetActiveWebContents()->GetPrimaryMainFrame());

  const std::string guid = entity_instance.guid().value();
  const std::string args = base::StrCat({"[\"", guid, "\"]"});
  std::optional<base::Value> result =
      extensions::api_test_utils::RunFunctionAndReturnSingleResult(
          function.get(), args, profile());
  ASSERT_TRUE(result);
  ASSERT_TRUE(result->is_dict());
  EXPECT_THAT(result->GetDict().FindString("guid"), Pointee(Eq(guid)));
}

class AutofillPrivateApiGetEntityInstancedTest
    : public AutofillPrivateApiBrowserTest {
 public:
  AutofillPrivateApiGetEntityInstancedTest() = default;

  autofill::EntityDataManager* entity_data_manager() {
    return autofill::AutofillEntityDataManagerFactory::GetForProfile(profile());
  }

  [[nodiscard]] bool AddEntity(const EntityInstance& entity_instance) {
    entity_data_manager()->AddOrUpdateEntityInstance(entity_instance);
    return base::test::RunUntil([&]() {
      return entity_data_manager()
          ->GetEntityInstance(entity_instance.guid())
          .has_value();
    });
  }
};

IN_PROC_BROWSER_TEST_F(AutofillPrivateApiGetEntityInstancedTest,
                       ReturnsEntityInstance) {
  EntityInstance entity_instance =
      autofill::test::GetPassportEntityInstanceWithRandomGuid();
  ASSERT_TRUE(AddEntity(entity_instance));

  auto function = base::MakeRefCounted<
      extensions::AutofillPrivateGetEntityInstanceByGuidFunction>();
  function->SetRenderFrameHost(GetActiveWebContents()->GetPrimaryMainFrame());

  const std::string guid = entity_instance.guid().value();
  const std::string args = base::StrCat({"[\"", guid, "\"]"});
  std::optional<base::Value> result =
      extensions::api_test_utils::RunFunctionAndReturnSingleResult(
          function.get(), args, profile());

  ASSERT_TRUE(result);
  ASSERT_TRUE(result->is_dict());
  EXPECT_THAT(result->GetDict().FindString("guid"), Pointee(Eq(guid)));
}
class AutofillPrivateApiObfuscationUnitTest
    : public AutofillPrivateApiBrowserTest {
 public:
  AutofillPrivateApiObfuscationUnitTest() = default;

 private:
  base::test::ScopedFeatureList feature_list_{
      autofill::features::kAutofillAiReauthRequired};
};

IN_PROC_BROWSER_TEST_F(AutofillPrivateApiObfuscationUnitTest,
                       ObfuscatedLabels) {
  autofill::prefs::SetAutofillAiReauthBeforeFillingEnabled(
      profile()->GetPrefs(), true);
  ASSERT_TRUE(RunAutofillSubtest("testExpectedObfuscatedLabelsAreGenerated"));
}

class AutofillPrivateApiUpdateAutofillAiAuthRequirementPrefTest
    : public AutofillPrivateApiBrowserTest {
 public:
  AutofillPrivateApiUpdateAutofillAiAuthRequirementPrefTest() {
    feature_list_.InitAndEnableFeature(
        autofill::features::kAutofillAiReauthRequired);
  }

 private:
  base::test::ScopedFeatureList feature_list_;
};

IN_PROC_BROWSER_TEST_F(
    AutofillPrivateApiUpdateAutofillAiAuthRequirementPrefTest,
    Success) {
  autofill::prefs::SetAutofillAiReauthBeforeFillingEnabled(
      autofill_client()->GetPrefs(), false);

  auto authenticator =
      std::make_unique<device_reauth::MockDeviceAuthenticator>();
  EXPECT_CALL(*authenticator, CanAuthenticateWithBiometricOrScreenLock)
      .WillOnce(Return(true));
  EXPECT_CALL(*authenticator, AuthenticateWithMessage)
      .WillOnce(RunOnceCallback<1>(true));
  autofill_client()->SetDeviceAuthenticator(std::move(authenticator));

  auto function = base::MakeRefCounted<
      extensions::AutofillPrivateToggleAutofillAiReauthRequirementFunction>();
  function->SetRenderFrameHost(GetActiveWebContents()->GetPrimaryMainFrame());

  extensions::api_test_utils::RunFunction(function.get(), "[]", profile());
  EXPECT_TRUE(autofill::prefs::IsAutofillAiReauthBeforeFillingEnabled(
      autofill_client()->GetPrefs()));
}

IN_PROC_BROWSER_TEST_F(
    AutofillPrivateApiUpdateAutofillAiAuthRequirementPrefTest,
    AuthenticationFailed) {
  autofill::prefs::SetAutofillAiReauthBeforeFillingEnabled(
      autofill_client()->GetPrefs(), false);

  auto authenticator =
      std::make_unique<device_reauth::MockDeviceAuthenticator>();
  EXPECT_CALL(*authenticator, CanAuthenticateWithBiometricOrScreenLock)
      .WillOnce(Return(true));
  EXPECT_CALL(*authenticator, AuthenticateWithMessage)
      .WillOnce(RunOnceCallback<1>(false));
  autofill_client()->SetDeviceAuthenticator(std::move(authenticator));

  auto function = base::MakeRefCounted<
      extensions::AutofillPrivateToggleAutofillAiReauthRequirementFunction>();
  function->SetRenderFrameHost(GetActiveWebContents()->GetPrimaryMainFrame());

  extensions::api_test_utils::RunFunction(function.get(), "[]", profile());
  EXPECT_FALSE(autofill::prefs::IsAutofillAiReauthBeforeFillingEnabled(
      autofill_client()->GetPrefs()));
}

#endif  // BUILDFLAG(IS_MAC) || BUILDFLAG(IS_WIN) || BUILDFLAG(IS_ANDROID) ||
        // BUILDFLAG(IS_CHROMEOS)

}  // namespace
