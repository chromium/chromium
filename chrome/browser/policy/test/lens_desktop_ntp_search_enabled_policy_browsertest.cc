// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/test/scoped_feature_list.h"
#include "chrome/browser/policy/policy_test_utils.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/common/url_constants.h"
#include "chrome/test/base/ui_test_utils.h"
#include "components/policy/core/common/policy_map.h"
#include "components/policy/policy_constants.h"
#include "components/search/ntp_features.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/web_contents.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace policy {

namespace {

bool RealboxContainsVisibleElement(content::WebContents* contents,
                                   const std::string& id) {
  bool result;
  EXPECT_TRUE(content::ExecuteScriptAndExtractBool(
      contents,
      "var elem = "
      "document.getElementsByTagName('ntp-app')[0].shadowRoot.getElementById('"
      "realbox').shadowRoot.getElementById('" +
          id +
          "');"
          "domAutomationController.send(!!elem && !elem.hidden);",
      &result));
  return result;
}

}  // namespace

// Similar to PolicyTest, but enable the New Tab page lens search flag before
// the browser starts.
class LensDesktopNTPSearchEnabledPolicyTest : public PolicyTest {
 public:
  LensDesktopNTPSearchEnabledPolicyTest() {
    scoped_feature_list_.InitAndEnableFeature(
        ntp_features::kNtpRealboxLensSearch);
  }
  ~LensDesktopNTPSearchEnabledPolicyTest() override = default;

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
};

IN_PROC_BROWSER_TEST_F(LensDesktopNTPSearchEnabledPolicyTest,
                       LensSearchButtonHidden) {
  // Verifies that the Lens search button icon can be hidden from the New Tab
  // page. A policy change takes immediate effect upon reload of the New Tab
  // page for the current profile. Browser restart is not required.
  const std::string kLensSearchButtonId = "lensSearchButton";

  // Open new tab page and look for the lens search button.
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(),
                                           GURL(chrome::kChromeUINewTabURL)));
  content::WebContents* contents =
      browser()->tab_strip_model()->GetActiveWebContents();
  EXPECT_TRUE(RealboxContainsVisibleElement(contents, kLensSearchButtonId));

  // Turn off the lens search button.
  PolicyMap policies;
  policies.Set(key::kLensDesktopNTPSearchEnabled, POLICY_LEVEL_MANDATORY,
               POLICY_SCOPE_USER, POLICY_SOURCE_CLOUD, base::Value(false),
               nullptr);
  UpdateProviderPolicy(policies);

  // The lens search button should now be hidden.
  ASSERT_TRUE(ui_test_utils::NavigateToURLWithDisposition(
      browser(), GURL(chrome::kChromeUINewTabURL),
      WindowOpenDisposition::NEW_FOREGROUND_TAB,
      ui_test_utils::BROWSER_TEST_WAIT_FOR_LOAD_STOP));
  contents = browser()->tab_strip_model()->GetActiveWebContents();
  EXPECT_FALSE(RealboxContainsVisibleElement(contents, kLensSearchButtonId));
}

}  // namespace policy
