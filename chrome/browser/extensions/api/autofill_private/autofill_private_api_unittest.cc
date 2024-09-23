// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/api/autofill_private/autofill_private_api.h"

#include <string>
#include <vector>

#include "base/test/bind.h"
#include "base/test/test_future.h"
#include "chrome/browser/autofill/autofill_uitest_util.h"
#include "chrome/browser/extensions/api/autofill_private/autofill_private_event_router.h"
#include "chrome/browser/extensions/api/autofill_private/autofill_private_event_router_factory.h"
#include "chrome/browser/extensions/extension_apitest.h"
#include "chrome/browser/ui/autofill/chrome_autofill_client.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/user_annotations/user_annotations_service_factory.h"
#include "components/autofill/content/browser/test_autofill_client_injector.h"
#include "components/autofill/content/browser/test_content_autofill_client.h"
#include "components/autofill/core/browser/autofill_test_utils.h"
#include "components/autofill/core/browser/data_model/credit_card.h"
#include "components/autofill/core/browser/metrics/payments/mandatory_reauth_metrics.h"
#include "components/autofill/core/browser/payments/payments_autofill_client.h"
#include "components/autofill/core/browser/payments_data_manager.h"
#include "components/autofill/core/common/autofill_prefs.h"
#include "components/device_reauth/mock_device_authenticator.h"
#include "components/prefs/pref_service.h"
#include "components/user_annotations/test_user_annotations_service.h"
#include "components/user_annotations/user_annotations_types.h"
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
    autofill_client()->GetPersonalDataManager()->SetPrefService(
        autofill_client()->GetPrefs());
    autofill_client()
        ->GetPersonalDataManager()
        ->payments_data_manager()
        .SetPaymentMethodsMandatoryReauthEnabled(IsFeatureTurnedOn());
    extensions::AutofillPrivateEventRouterFactory::GetForProfile(
        browser_context())
        ->RebindPersonalDataManagerForTesting(
            autofill_client()->GetPersonalDataManager());
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
    autofill::WaitForPersonalDataManagerToBeLoaded(profile());

    const std::string extension_url = "main.html?" + subtest;
    return RunExtensionTest("autofill_private",
                            {.extension_url = extension_url.c_str()},
                            {.load_as_component = true});
  }

  autofill::TestContentAutofillClient* autofill_client() {
    return test_autofill_client_injector_
        [browser()->tab_strip_model()->GetActiveWebContents()];
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
    autofill_client()
        ->GetPersonalDataManager()
        ->payments_data_manager()
        .SetSyncingForTest(
            /*is_syncing_for_test=*/true);
    UserAnnotationsServiceFactory::GetInstance()->SetTestingFactoryAndUse(
        profile(),
        base::BindLambdaForTesting([](content::BrowserContext* context)
                                       -> std::unique_ptr<KeyedService> {
          return std::make_unique<
              user_annotations::TestUserAnnotationsService>();
        }));
  }

  void TearDownOnMainThread() override {
    InProcessBrowserTest::TearDownOnMainThread();
  }

  autofill::TestContentAutofillClient* autofill_client() {
    return test_autofill_client_injector_
        [browser()->tab_strip_model()->GetActiveWebContents()];
  }

  user_annotations::TestUserAnnotationsService* user_annotations_service() {
    return static_cast<user_annotations::TestUserAnnotationsService*>(
        UserAnnotationsServiceFactory::GetForProfile(profile()));
  }

  user_annotations::UserAnnotationsEntries GetAllUserAnnotationsEntries() {
    base::test::TestFuture<user_annotations::UserAnnotationsEntries>
        test_future;
    user_annotations_service()->RetrieveAllEntries(test_future.GetCallback());
    return test_future.Take();
  }

 protected:
  bool RunAutofillSubtest(const std::string& subtest) {
    autofill::WaitForPersonalDataManagerToBeLoaded(profile());

    const std::string extension_url = "main.html?" + subtest;
    return RunExtensionTest("autofill_private",
                            {.extension_url = extension_url.c_str()},
                            {.load_as_component = true});
  }

 private:
  autofill::TestAutofillClientInjector<autofill::TestContentAutofillClient>
      test_autofill_client_injector_;
  raw_ptr<user_annotations::TestUserAnnotationsService>
      user_annotations_service_;
};

// Test to verify all the CVCs(server and local) are bulk deleted when the API
// is called.
IN_PROC_BROWSER_TEST_F(AutofillPrivateApiUnitTest, BulkDeleteAllCvcs) {
  autofill::CreditCard local_card =
      autofill::test::WithCvc(autofill::test::GetCreditCard(), u"789");
  autofill::CreditCard server_card =
      autofill::test::WithCvc(autofill::test::GetMaskedServerCard(), u"098");
  autofill::TestPersonalDataManager* personal_data =
      autofill_client()->GetPersonalDataManager();
  personal_data->payments_data_manager().AddCreditCard(local_card);
  personal_data->test_payments_data_manager().AddServerCreditCard(server_card);

  // Verify that cards are same as above and the CVCs are present for both of
  // them.
  ASSERT_EQ(personal_data->payments_data_manager().GetCreditCards().size(), 2u);
  for (const autofill::CreditCard* card :
       personal_data->payments_data_manager().GetCreditCards()) {
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
  ASSERT_EQ(personal_data->payments_data_manager().GetCreditCards().size(), 2u);
  for (const autofill::CreditCard* card :
       personal_data->payments_data_manager().GetCreditCards()) {
    EXPECT_TRUE(card->cvc().empty());
    if (card->record_type() ==
        autofill::CreditCard::RecordType::kMaskedServerCard) {
      EXPECT_EQ(card->number(), server_card.number());
    } else {
      EXPECT_EQ(card->number(), local_card.number());
    }
  }
}

IN_PROC_BROWSER_TEST_F(AutofillPrivateApiUnitTest, RetrieveAllUserAnnotations) {
  ASSERT_EQ(user_annotations_service()->count_entries_retrieved(), 0u);
  RunAutofillSubtest("getUserAnnotationsEntries");
  ASSERT_EQ(user_annotations_service()->count_entries_retrieved(), 1u);
}

IN_PROC_BROWSER_TEST_F(AutofillPrivateApiUnitTest, RemoveUserAnnotationEntry) {
  // Seed user annotations service with entries.
  ASSERT_TRUE(RunAutofillSubtest("deleteUserAnnotationsEntry"));
  optimization_guide::proto::UserAnnotationsEntry entry_1;
  entry_1.set_entry_id(123);
  optimization_guide::proto::UserAnnotationsEntry entry_2;
  entry_2.set_entry_id(321);
  user_annotations_service()->ReplaceAllEntries({entry_1, entry_2});
  EXPECT_EQ(GetAllUserAnnotationsEntries().size(), 2u);

  // By default, the test deletes the entry whose id is 123.
  RunAutofillSubtest("deleteUserAnnotationsEntry");

  user_annotations::UserAnnotationsEntries entries =
      GetAllUserAnnotationsEntries();
  EXPECT_EQ(entries.size(), 1u);
  EXPECT_EQ(entries[0].entry_id(), 321u);
}

IN_PROC_BROWSER_TEST_F(AutofillPrivateApiUnitTest, RemoveAllUserAnnotations) {
  // Seed user annotations service with entries.
  optimization_guide::proto::UserAnnotationsEntry entry;
  entry.set_entry_id(0);
  user_annotations_service()->ReplaceAllEntries({entry});
  EXPECT_EQ(GetAllUserAnnotationsEntries().size(), 1u);

  RunAutofillSubtest("deleteAllUserAnnotationsEntries");
  EXPECT_TRUE(GetAllUserAnnotationsEntries().empty());
}

}  // namespace
