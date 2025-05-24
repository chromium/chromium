// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/api/autofill_private/autofill_private_api.h"

#include <string>
#include <vector>

#include "base/functional/bind.h"
#include "base/test/bind.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/test_future.h"
#include "chrome/browser/autofill/autofill_entity_data_manager_factory.h"
#include "chrome/browser/autofill/autofill_uitest_util.h"
#include "chrome/browser/extensions/api/autofill_private/autofill_private_event_router.h"
#include "chrome/browser/extensions/api/autofill_private/autofill_private_event_router_factory.h"
#include "chrome/browser/extensions/extension_apitest.h"
#include "chrome/browser/ui/autofill/chrome_autofill_client.h"
#include "chrome/browser/ui/browser.h"
#include "components/autofill/content/browser/test_autofill_client_injector.h"
#include "components/autofill/content/browser/test_content_autofill_client.h"
#include "components/autofill/core/browser/data_manager/addresses/test_address_data_manager.h"
#include "components/autofill/core/browser/data_manager/payments/payments_data_manager.h"
#include "components/autofill/core/browser/data_model/payments/credit_card.h"
#include "components/autofill/core/browser/foundations/autofill_client.h"
#include "components/autofill/core/browser/metrics/payments/mandatory_reauth_metrics.h"
#include "components/autofill/core/browser/payments/payments_autofill_client.h"
#include "components/autofill/core/browser/payments/payments_request_details.h"
#include "components/autofill/core/browser/payments/test_payments_network_interface.h"
#include "components/autofill/core/browser/permissions/autofill_ai/autofill_ai_permission_utils.h"
#include "components/autofill/core/browser/test_utils/autofill_test_utils.h"
#include "components/autofill/core/common/autofill_features.h"
#include "components/autofill/core/common/autofill_prefs.h"
#include "components/device_reauth/mock_device_authenticator.h"
#include "components/prefs/pref_service.h"
#include "components/sync/test/test_sync_service.h"
#include "content/public/test/browser_test.h"

namespace {

using ::testing::Eq;

#if BUILDFLAG(IS_MAC) || BUILDFLAG(IS_WIN)
using autofill::autofill_metrics::MandatoryReauthAuthenticationFlowEvent;

// There are 2 boolean params set in the test suites.
// The first param can be retrieved via `IsFeatureTurnedOn()` which determines
// if the toggle is currently turned on or off. The second param can be
// retrieved via `IsUserAuthSuccessful()` which determines if the user auth was
// successful or not.
class MandatoryReauthSettingsPageMetricsTest
    : public extensions::ExtensionApiTest,
      public testing::WithParamInterface<std::tuple<bool, bool>> {
 public:
  MandatoryReauthSettingsPageMetricsTest() = default;
  MandatoryReauthSettingsPageMetricsTest(
      const MandatoryReauthSettingsPageMetricsTest&) = delete;
  MandatoryReauthSettingsPageMetricsTest& operator=(
      const MandatoryReauthSettingsPageMetricsTest&) = delete;
  ~MandatoryReauthSettingsPageMetricsTest() override = default;

  void SetUpOnMainThread() override {
    ExtensionApiTest::SetUpOnMainThread();
    personal_data_manager().SetPrefService(autofill_client()->GetPrefs());
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
    return test_autofill_client_injector_
        [browser()->tab_strip_model()->GetActiveWebContents()];
  }
  autofill::TestPersonalDataManager& personal_data_manager() {
    return autofill_client()->GetPersonalDataManager();
  }

 private:
  content::BrowserContext* browser_context() {
    return browser()
        ->tab_strip_model()
        ->GetActiveWebContents()
        ->GetBrowserContext();
  }

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
                         testing::Combine(testing::Bool(), testing::Bool()));
#endif  // BUILDFLAG(IS_MAC) || BUILDFLAG(IS_WIN)

class AutofillPrivateApiUnitTest : public extensions::ExtensionApiTest {
 public:
  AutofillPrivateApiUnitTest() = default;
  AutofillPrivateApiUnitTest(const AutofillPrivateApiUnitTest&) = delete;
  AutofillPrivateApiUnitTest& operator=(const AutofillPrivateApiUnitTest&) =
      delete;
  ~AutofillPrivateApiUnitTest() override = default;
  void SetUpOnMainThread() override {
    ExtensionApiTest::SetUpOnMainThread();
    payments_data_manager().SetSyncingForTest(/*is_syncing_for_test=*/true);
    payments_data_manager().SetPrefService(autofill_client()->GetPrefs());
  }

  void TearDownOnMainThread() override {
    InProcessBrowserTest::TearDownOnMainThread();
  }

  autofill::TestAddressDataManager& address_data_manager() {
    return personal_data_manager().test_address_data_manager();
  }
  autofill::TestContentAutofillClient* autofill_client() {
    return test_autofill_client_injector_
        [browser()->tab_strip_model()->GetActiveWebContents()];
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
  base::test::ScopedFeatureList feature_list_{
      autofill::features::kAutofillAiWithDataSchema};
};

// Test to verify all the CVCs(server and local) are bulk deleted when the API
// is called.
IN_PROC_BROWSER_TEST_F(AutofillPrivateApiUnitTest, BulkDeleteAllCvcs) {
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

IN_PROC_BROWSER_TEST_F(AutofillPrivateApiUnitTest, LogServerCardLinkClicked) {
  base::HistogramTester histogram_tester;
  ASSERT_TRUE(RunAutofillSubtest("logServerCardLinkClicked"));
  histogram_tester.ExpectUniqueSample(
      "Autofill.ServerCardLinkClicked",
      autofill::AutofillMetrics::PaymentsSigninState::kSignedOut, 1);
}

IN_PROC_BROWSER_TEST_F(AutofillPrivateApiUnitTest, RemoveVirtualCard) {
  using autofill::payments::TestPaymentsNetworkInterface;
  autofill_client()
      ->GetPaymentsAutofillClient()
      ->set_payments_network_interface(
          std::make_unique<TestPaymentsNetworkInterface>(
              autofill_client()->GetURLLoaderFactory(),
              autofill_client()->GetIdentityManager(),
              &personal_data_manager()));
  // Required for adding the server card.
  payments_data_manager().SetSyncingForTest(
      /*is_syncing_for_test=*/true);
  autofill::CreditCard virtual_card = autofill::test::GetVirtualCard();
  virtual_card.set_server_id("a123");
  virtual_card.set_instrument_id(123);
  payments_data_manager().AddServerCreditCard(virtual_card);
  EXPECT_TRUE(RunAutofillSubtest("removeVirtualCard"));
  EXPECT_THAT(
      static_cast<TestPaymentsNetworkInterface*>(
          autofill_client()
              ->GetPaymentsAutofillClient()
              ->GetPaymentsNetworkInterface())
          ->update_virtual_card_enrollment_request_details(),
      ::testing::AllOf(
          ::testing::Field(
              &autofill::payments::UpdateVirtualCardEnrollmentRequestDetails::
                  instrument_id,
              123),
          ::testing::Field(
              &autofill::payments::UpdateVirtualCardEnrollmentRequestDetails::
                  virtual_card_enrollment_request_type,
              autofill::VirtualCardEnrollmentRequestType::kUnenroll)));
}

IN_PROC_BROWSER_TEST_F(AutofillPrivateApiUnitTest,
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
IN_PROC_BROWSER_TEST_F(AutofillPrivateApiUnitTest, MAYBE_EntityInstances) {
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
  //  Test that retrieving general entity type information works.
  ASSERT_TRUE(RunAutofillSubtest("getAllEntityTypes"));
  ASSERT_TRUE(RunAutofillSubtest("getAllAttributeTypesForEntityTypeName"));
}

IN_PROC_BROWSER_TEST_F(AutofillPrivateApiUnitTest,
                       GetEmptyPayOverTimeIssuerList) {
  ASSERT_TRUE(RunAutofillSubtest("getEmptyPayOverTimeIssuerList"));
}

IN_PROC_BROWSER_TEST_F(AutofillPrivateApiUnitTest, SetAutofillAiOptIn) {
  autofill_client()->set_entity_data_manager(
      autofill::AutofillEntityDataManagerFactory::GetForProfile(profile()));
  autofill_client()->SetUpPrefsAndIdentityForAutofillAi();
  EXPECT_TRUE(autofill::SetAutofillAiOptInStatus(*autofill_client(), false));
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

// Tests that the scenario where the user becomes ineligible and then tries
// opting into Autofill AI behaves as expected.
IN_PROC_BROWSER_TEST_F(AutofillPrivateApiUnitTest,
                       SetAutofillAiOptIn_SwitchEligibility) {
  autofill_client()->set_entity_data_manager(
      autofill::AutofillEntityDataManagerFactory::GetForProfile(profile()));
  autofill_client()->SetUpPrefsAndIdentityForAutofillAi();

  ASSERT_TRUE(autofill::MayPerformAutofillAiAction(
      *autofill_client(), autofill::AutofillAiAction::kOptIn));
  EXPECT_TRUE(autofill::SetAutofillAiOptInStatus(*autofill_client(), true));

  // Verify that we can opt out of Autofill AI while eligible.
  ASSERT_TRUE(RunAutofillSubtest("optOutOfAutofillAi"));
  EXPECT_TRUE(RunAutofillSubtest("verifyUserOptedOutOfAutofillAi"));

  // Become ineligible.
  autofill_client()->set_app_locale("de-DE");
  ASSERT_FALSE(autofill::MayPerformAutofillAiAction(
      *autofill_client(), autofill::AutofillAiAction::kOptIn));

  // Verify that we cannot opt into Autofill AI anymore.
  ASSERT_TRUE(RunAutofillSubtest("optIntoAutofillAi"));
  EXPECT_TRUE(RunAutofillSubtest("verifyUserOptedOutOfAutofillAi"));
}

}  // namespace
