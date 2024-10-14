// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <string>
#include <unordered_map>

#include "base/values.h"
#include "chrome/browser/autofill_prediction_improvements/chrome_autofill_prediction_improvements_client.h"
#include "chrome/browser/policy/policy_test_utils.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/tabs/public/tab_features.h"
#include "chrome/test/base/ui_test_utils.h"
#include "components/autofill_prediction_improvements/core/browser/autofill_prediction_improvements_features.h"
#include "components/optimization_guide/core/feature_registry/feature_registration.h"
#include "components/password_manager/core/browser/password_autofill_manager.h"
#include "components/policy/core/common/policy_map.h"
#include "components/policy/policy_constants.h"
#include "content/public/browser/web_contents.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "url/gurl.h"

namespace policy {
namespace {

class AutofillPredictionImprovementsPolicyTest
    : public PolicyTest,
      public testing::WithParamInterface<int> {
 public:
  int policy_value() const { return GetParam(); }
  bool policy_is_disabled() const { return policy_value() == 2; }

  void SetUpOnMainThread() override {
    InProcessBrowserTest::SetUpOnMainThread();

    PolicyMap policies;
    SetPolicy(&policies, key::kAutofillPredictionSettings,
              base::Value(policy_value()));
    UpdateProviderPolicy(policies);

    // The base test fixture creates a tab before we set the policy. We create a
    // new tab so a new ChromeAutofillPredictionImprovementsClient is created.
    AddBlankTabAndShow(browser());
    ASSERT_TRUE(embedded_test_server()->Start());
  }

  content::WebContents* GetWebContents() {
    return browser()->tab_strip_model()->GetActiveWebContents();
  }

 private:
  base::test::ScopedFeatureList scoped_feature_list_{
      autofill_prediction_improvements::kAutofillPredictionImprovements};
};

INSTANTIATE_TEST_SUITE_P(,
                         AutofillPredictionImprovementsPolicyTest,
                         testing::Values(0, 1, 2));

// Tests that the chrome://settings entry for Autofill Predictions Improvement
// is reachable iff the policy is enabled.
IN_PROC_BROWSER_TEST_P(AutofillPredictionImprovementsPolicyTest,
                       SettingsDisabledByPolicy) {
  ASSERT_TRUE(ui_test_utils::NavigateToURL(
      browser(), GURL("chrome://settings/autofillPredictionImprovements")));
  EXPECT_EQ(autofill_prediction_improvements::
                IsAutofillPredictionImprovementsSupported(
                    browser()->profile()->GetPrefs()),
            !policy_is_disabled());
  EXPECT_EQ(GetWebContents()->GetURL().path(),
            policy_is_disabled() ? "/" : "/autofillPredictionImprovements");
}

// Tests that AutofillPredictionsImprovementDelegate exists iff it is allowed by
// the policy.
IN_PROC_BROWSER_TEST_P(AutofillPredictionImprovementsPolicyTest,
                       DelegateDisabledByPolicy) {
  ASSERT_TRUE(ui_test_utils::NavigateToURL(
      browser(), embedded_test_server()->GetURL(
                     "/autofill/autofill_address_enabled.html")));
  ChromeAutofillPredictionImprovementsClient* client =
      CHECK_DEREF(tabs::TabInterface::MaybeGetFromContents(GetWebContents()))
          .GetTabFeatures()
          ->chrome_autofill_prediction_improvements_client();
  EXPECT_EQ(client == nullptr, policy_is_disabled());
}

}  // namespace
}  // namespace policy
