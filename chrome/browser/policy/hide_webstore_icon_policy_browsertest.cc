// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/command_line.h"
#include "chrome/browser/policy/policy_test_utils.h"
#include "chrome/browser/ui/search/instant_test_utils.h"
#include "chrome/browser/ui/search/local_ntp_test_utils.h"
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

content::RenderFrameHost* GetMostVisitedIframe(content::WebContents* tab) {
  for (content::RenderFrameHost* frame : tab->GetAllFrames()) {
    if (frame->GetFrameName() == "mv-single")
      return frame;
  }
  return nullptr;
}

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

bool ContainsWebstoreTile(content::RenderFrameHost* iframe) {
  int num_webstore_tiles = 0;
  EXPECT_TRUE(instant_test_utils::GetIntFromJS(
      iframe,
      "document.querySelectorAll(\".md-tile[href='" +
          l10n_util::GetStringUTF8(IDS_WEBSTORE_URL) + "']\").length",
      &num_webstore_tiles));
  return num_webstore_tiles == 1;
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
  ui_test_utils::NavigateToURL(browser(), GURL(chrome::kChromeUIAppsURL));
  content::WebContents* contents =
      browser()->tab_strip_model()->GetActiveWebContents();

#if !defined(OS_CHROMEOS)
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
  ui_test_utils::NavigateToURL(browser(), GURL(chrome::kChromeUIAppsURL));
  EXPECT_FALSE(
      ContainsVisibleElement(contents, "ahfgeienlihckogmohjhadlkjgocpleb"));
  EXPECT_FALSE(ContainsVisibleElement(contents, "chrome-web-store-link"));
}

IN_PROC_BROWSER_TEST_F(PolicyWebStoreIconTest, NTPWebStoreIconShown) {
  // This test is to verify that the web store icons is shown when no policy
  // applies. See WebStoreIconPolicyTest.NTPWebStoreIconHidden for verification
  // when a policy is in effect.

  // Open new tab page and look for the web store icons.
  content::WebContents* active_tab =
      local_ntp_test_utils::OpenNewTab(browser(), GURL("about:blank"));
  local_ntp_test_utils::NavigateToNTPAndWaitUntilLoaded(browser());

  content::RenderFrameHost* iframe = GetMostVisitedIframe(active_tab);

  // Look though all the tiles and see whether there is a webstore icon.
  // Make sure that there is one web store icon.
  EXPECT_TRUE(ContainsWebstoreTile(iframe));
}

// Similar to PolicyWebStoreIconShownTest, but applies the HideWebStoreIcon
// policy before the browser is started. This is required because the list that
// includes the WebStoreIcon on the NTP is initialized at browser start.
class PolicyWebStoreIconHiddenTest : public PolicyTest {
 public:
  PolicyWebStoreIconHiddenTest() = default;
  ~PolicyWebStoreIconHiddenTest() override = default;

  void SetUpInProcessBrowserTestFixture() override {
    PolicyTest::SetUpInProcessBrowserTestFixture();
    PolicyMap policies;
    policies.Set(key::kHideWebStoreIcon, POLICY_LEVEL_MANDATORY,
                 POLICY_SCOPE_USER, POLICY_SOURCE_CLOUD, base::Value(true),
                 nullptr);
    provider_.UpdateChromePolicy(policies);
  }

  void SetUpCommandLine(base::CommandLine* command_line) override {
    PolicyTest::SetUpCommandLine(command_line);
  }
};

IN_PROC_BROWSER_TEST_F(PolicyWebStoreIconHiddenTest, NTPWebStoreIconHidden) {
  // Verifies that the web store icon can be hidden from the new tab page. Check
  // to see NTPWebStoreIconShown for behavior when the policy is not applied.

  // Open new tab page and look for the web store icon
  content::WebContents* active_tab =
      local_ntp_test_utils::OpenNewTab(browser(), GURL("about:blank"));
  local_ntp_test_utils::NavigateToNTPAndWaitUntilLoaded(browser());

  content::RenderFrameHost* iframe = GetMostVisitedIframe(active_tab);

  // Applying the policy before the browser started, the web store icon should
  // now be hidden.
  EXPECT_FALSE(ContainsWebstoreTile(iframe));
}

}  // namespace policy
