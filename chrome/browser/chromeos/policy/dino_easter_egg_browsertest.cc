// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <string>

#include "chrome/browser/policy/policy_test_utils.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/ui_test_utils.h"
#include "components/policy/core/common/policy_map.h"
#include "components/policy/policy_constants.h"
#include "components/strings/grit/components_strings.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "ui/base/l10n/l10n_util.h"

namespace {

// Tests the behavior of the AllowDinosaurEasterEgg policy.
// Bug Component b:1363915
// Contacts:
// * cros-edu-eng@google.com
// * dorianbrandon@google.com
// * jamescook@google.com (ported test from Tast)
class DinoEasterEggBrowserTest
    : public policy::PolicyTest,
      public testing::WithParamInterface<policy::PolicyTest::BooleanPolicy> {
 public:
  void SetUpInProcessBrowserTestFixture() override {
    policy::PolicyTest::SetUpInProcessBrowserTestFixture();

    // If the test parameter says to configure a policy, set the allow dinosaur
    // policy to true or false.
    if (GetParam() != BooleanPolicy::kNotConfigured) {
      policy::PolicyMap policy_map;
      SetPolicy(&policy_map, policy::key::kAllowDinosaurEasterEgg,
                base::Value(GetParam() == BooleanPolicy::kTrue));
      UpdateProviderPolicy(policy_map);
    }
  }

  // Returns the text of the snackbar div, or the empty string if it does not
  // exist.
  std::string GetSnackbarText() {
    content::WebContents* web_contents =
        browser()->tab_strip_model()->GetActiveWebContents();
    std::string js =
        "var hasText = document.querySelector('div.snackbar');"
        "hasText ? hasText.innerText : '';";
    return content::EvalJs(web_contents, js).ExtractString();
  }
};

INSTANTIATE_TEST_SUITE_P(
    BooleanPolicy,
    DinoEasterEggBrowserTest,
    testing::Values(policy::PolicyTest::BooleanPolicy::kNotConfigured,
                    policy::PolicyTest::BooleanPolicy::kFalse,
                    policy::PolicyTest::BooleanPolicy::kTrue));

IN_PROC_BROWSER_TEST_P(DinoEasterEggBrowserTest, AllowDinosaurEasterEgg) {
  // Navigate to the dino game.
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), GURL("chrome://dino")));

  std::string snackbar_text = GetSnackbarText();
  std::string expected_text;
  switch (GetParam()) {
    case BooleanPolicy::kNotConfigured:
      // Default is allowed, no error message.
      EXPECT_EQ(snackbar_text, "");
      break;
    case BooleanPolicy::kTrue:
      // Allowed, no error message.
      EXPECT_EQ(snackbar_text, "");
      break;
    case BooleanPolicy::kFalse:
      // Error message when game is disabled.
      EXPECT_EQ(snackbar_text,
                l10n_util::GetStringUTF8(IDS_ERRORPAGE_FUN_DISABLED));
      break;
  }
}

}  // namespace
