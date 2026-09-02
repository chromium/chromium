// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <string>
#include <unordered_map>
#include <utility>

#include "base/strings/strcat.h"
#include "base/values.h"
#include "chrome/browser/policy/policy_test_utils.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/signin/identity_test_environment_profile_adaptor.h"
#include "chrome/browser/ui/tabs/public/tab_features.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/common/webui_url_constants.h"
#include "chrome/test/base/ui_test_utils.h"
#include "components/autofill/core/common/autofill_features.h"
#include "components/optimization_guide/core/feature_registry/feature_registration.h"
#include "components/optimization_guide/core/model_execution/model_execution_prefs.h"
#include "components/password_manager/core/browser/password_autofill_manager.h"
#include "components/policy/core/common/policy_map.h"
#include "components/policy/policy_constants.h"
#include "components/signin/public/base/signin_switches.h"
#include "components/signin/public/identity_manager/account_capabilities_test_mutator.h"
#include "content/public/browser/web_contents.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "url/gurl.h"

namespace policy {
namespace {

using optimization_guide::model_execution::prefs::
    ModelExecutionEnterprisePolicyValue;

using enum ModelExecutionEnterprisePolicyValue;

// The policy's value are 0, 1, 2 and Autofill expects that the enum
// ModelExecutionEnterprisePolicyValue matches those values.
static_assert(std::to_underlying(kAllow) == 0);
static_assert(std::to_underlying(kAllowWithoutLogging) == 1);
static_assert(std::to_underlying(kDisable) == 2);

// This test has two parameters:
//  * Policy value.
class AutofillAiPolicyTest
    : public PolicyTest,
      public testing::WithParamInterface<ModelExecutionEnterprisePolicyValue> {
 public:
  AutofillAiPolicyTest() {
    scoped_feature_list_.InitWithFeatures(
        {autofill::features::kAutofillAiWithDataSchema,
         autofill::features::kAutofillAiIgnoreGeoIp,
         autofill::features::kAutofillAmbientAutofill},
        {});
  }

  ModelExecutionEnterprisePolicyValue policy_value() const {
    return GetParam();
  }
  bool disabled_by_policy() const { return policy_value() == kDisable; }

  void SetUpOnMainThread() override {
    PolicyTest::SetUpOnMainThread();

    identity_test_env_adaptor_ =
        std::make_unique<IdentityTestEnvironmentProfileAdaptor>(
            browser()->GetProfile());

    EnableSignin();

    PolicyMap policies;
    SetPolicy(&policies, key::kAutofillPredictionSettings,
              base::Value(std::to_underlying(policy_value())));
    UpdateProviderPolicy(policies);

    // The base test fixture creates a tab before we set the policy. We create a
    // new tab so a new `AutofillClient` is created.
    AddBlankTabAndShow(browser());
    ASSERT_TRUE(embedded_test_server()->Start());
  }

  content::WebContents* GetWebContents() {
    return browser()->tab_strip_model()->GetActiveWebContents();
  }

  void SetUpInProcessBrowserTestFixture() override {
    PolicyTest::SetUpInProcessBrowserTestFixture();

    create_services_subscription_ =
        BrowserContextDependencyManager::GetInstance()
            ->RegisterCreateServicesCallbackForTesting(base::BindRepeating(
                &AutofillAiPolicyTest::OnWillCreateBrowserContextServices,
                base::Unretained(this)));
  }

  void VerifySettingsUrlIsReachable(std::string_view sub_page) {
    ASSERT_TRUE(ui_test_utils::NavigateToURL(
        browser(), GURL(base::StrCat({"chrome://settings/", sub_page}))));
    EXPECT_TRUE(content::WaitForLoadStop(GetWebContents()));
    EXPECT_EQ(GetWebContents()->GetURL().GetPath(),
              base::StrCat({"/", sub_page}));
  }

 private:
  void EnableSignin() {
    auto account_info =
        identity_test_env_adaptor_->identity_test_env()
            ->MakePrimaryAccountAvailable("user@gmail.com",
                                          signin::ConsentLevel::kSignin);
    AccountCapabilitiesTestMutator mutator(&account_info);
    mutator.set_can_use_model_execution_features(true);
    identity_test_env_adaptor_->identity_test_env()
        ->UpdateAccountInfoForAccount(account_info);
    identity_test_env_adaptor_->identity_test_env()
        ->SetAutomaticIssueOfAccessTokens(true);
  }

  void OnWillCreateBrowserContextServices(content::BrowserContext* context) {
    IdentityTestEnvironmentProfileAdaptor::
        SetIdentityTestEnvironmentFactoriesOnBrowserContext(context);
  }

  base::test::ScopedFeatureList scoped_feature_list_;

  // Identity test support.
  std::unique_ptr<IdentityTestEnvironmentProfileAdaptor>
      identity_test_env_adaptor_;
  base::CallbackListSubscription create_services_subscription_;
};

INSTANTIATE_TEST_SUITE_P(,
                         AutofillAiPolicyTest,
                         testing::Values(kAllow,
                                         kAllowWithoutLogging,
                                         kDisable));

// Tests that the chrome://settings entry for Autofill AI is always reachable
// even if the policy is disabled.
IN_PROC_BROWSER_TEST_P(AutofillAiPolicyTest, SettingsNotDisabledByPolicy) {
  VerifySettingsUrlIsReachable(chrome::kIdentityDocsSubPage);
  VerifySettingsUrlIsReachable(chrome::kTravelSubPage);
  VerifySettingsUrlIsReachable(chrome::kShoppingSubPage);
}

}  // namespace
}  // namespace policy
