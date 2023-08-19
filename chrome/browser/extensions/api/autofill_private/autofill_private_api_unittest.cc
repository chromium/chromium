// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/api/autofill_private/autofill_private_api.h"

#include <vector>

#include "chrome/browser/autofill/autofill_uitest_util.h"
#include "chrome/browser/extensions/extension_apitest.h"
#include "chrome/browser/ui/autofill/chrome_autofill_client.h"
#include "components/autofill/content/browser/test_autofill_client_injector.h"
#include "components/autofill/content/browser/test_content_autofill_client.h"
#include "components/autofill/core/browser/metrics/payments/mandatory_reauth_metrics.h"
#include "components/autofill/core/common/autofill_prefs.h"
#include "components/device_reauth/mock_device_authenticator.h"
#include "components/prefs/pref_service.h"
#include "content/public/test/browser_test.h"

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
        ->SetPaymentMethodsMandatoryReauthEnabled(IsFeatureTurnedOn());
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
  autofill::TestAutofillClientInjector<autofill::TestContentAutofillClient>
      test_autofill_client_injector_;
};

// This tests the logging for mandatory reauth opt-in / opt-out flows when
// triggered from the settings page.
IN_PROC_BROWSER_TEST_P(MandatoryReauthSettingsPageMetricsTest,
                       SettingsPageMandatoryReauthToggleSwitching) {
  base::HistogramTester histogram_tester;

  ON_CALL(*static_cast<autofill::payments::MockMandatoryReauthManager*>(
              autofill_client()->GetOrCreatePaymentsMandatoryReauthManager()),
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
                       SettingsPageMandatoryReauthEditLocalCard) {
  base::HistogramTester histogram_tester;

  ON_CALL(*static_cast<autofill::payments::MockMandatoryReauthManager*>(
              autofill_client()->GetOrCreatePaymentsMandatoryReauthManager()),
          AuthenticateWithMessage)
      .WillByDefault(
          testing::WithArg<1>([auth_success = IsUserAuthSuccessful()](
                                  base::OnceCallback<void(bool)> callback) {
            std::move(callback).Run(auth_success);
          }));

  RunAutofillSubtest("authenticateUserToEditLocalCard");

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
