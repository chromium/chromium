// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/command_line.h"
#include "build/build_config.h"
#include "chrome/browser/policy/policy_test_utils.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/common/url_constants.h"
#include "chrome/grit/locale_settings.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/ui_test_utils.h"
#include "components/policy/core/common/policy_map.h"
#include "components/policy/policy_constants.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/web_contents.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "ui/base/l10n/l10n_util.h"

namespace policy {

namespace {

bool ContainsVisibleElement(content::WebContents* contents,
                            const std::string& id) {
  bool result;
  EXPECT_TRUE(content::ExecuteScriptAndExtractBool(
      contents,
      "var elem = document.getElementById('" + id +
          "');"
          "domAutomationController.send(!!elem && !elem.hidden);",
      &result));
  return result;
}

}  // namespace

// Similar to PolicyTest, but force to enable the new tab material design flag
// before the browser start.
class PolicyWebStoreIconTest : public PolicyTest {
 public:
  PolicyWebStoreIconTest() = default;
  ~PolicyWebStoreIconTest() override = default;

  void SetUpInProcessBrowserTestFixture() override {
    PolicyTest::SetUpInProcessBrowserTestFixture();
  }

  void SetUpCommandLine(base::CommandLine* command_line) override {
    PolicyTest::SetUpCommandLine(command_line);
  }
};

IN_PROC_BROWSER_TEST_F(PolicyWebStoreIconTest, AppsWebStoreIconHidden) {
  // Verifies that the web store icon can be hidden from the chrome://apps
  // page. A policy change takes immediate effect on the apps page for the
  // current profile. Browser restart is not required.

  // Open new tab page and look for the web store icons.
  ASSERT_TRUE(
      ui_test_utils::NavigateToURL(browser(), GURL(chrome::kChromeUIAppsURL)));
  content::WebContents* contents =
      browser()->tab_strip_model()->GetActiveWebContents();

#if !BUILDFLAG(IS_CHROMEOS)
  // Look for web store's app ID in the apps page.
  EXPECT_TRUE(
      ContainsVisibleElement(contents, "ahfgeienlihckogmohjhadlkjgocpleb"));
#endif

  // The next NTP has no footer.
  if (ContainsVisibleElement(contents, "footer"))
    EXPECT_TRUE(ContainsVisibleElement(contents, "chrome-web-store-link"));

  // Turn off the web store icons.
  PolicyMap policies;
  policies.Set(key::kHideWebStoreIcon, POLICY_LEVEL_MANDATORY,
               POLICY_SCOPE_USER, POLICY_SOURCE_CLOUD, base::Value(true),
               nullptr);
  UpdateProviderPolicy(policies);

  // The web store icons should now be hidden.
  ASSERT_TRUE(
      ui_test_utils::NavigateToURL(browser(), GURL(chrome::kChromeUIAppsURL)));
  EXPECT_FALSE(
      ContainsVisibleElement(contents, "ahfgeienlihckogmohjhadlkjgocpleb"));
  EXPECT_FALSE(ContainsVisibleElement(contents, "chrome-web-store-link"));
}

}  // namespace policy
