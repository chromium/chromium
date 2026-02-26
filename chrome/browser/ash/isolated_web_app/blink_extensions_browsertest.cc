// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/browser/ui/web_applications/test/isolated_web_app_test_utils.h"
#include "chrome/browser/ui/web_applications/test/web_app_browsertest_util.h"
#include "chrome/browser/web_applications/isolated_web_apps/isolated_web_app_url_info.h"
#include "chrome/browser/web_applications/isolated_web_apps/test/isolated_web_app_builder.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/ui_test_utils.h"
#include "content/public/common/content_switches.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"

namespace ash {

namespace {

webapps::AppId InstallIsolatedWebAppAndReturnAppId(Profile* profile) {
  web_app::IsolatedWebAppUrlInfo url_info =
      web_app::IsolatedWebAppBuilder(
          web_app::ManifestBuilder().SetName("Blink Extension Test IWA"))
          .BuildBundle()
          ->InstallChecked(profile);
  return url_info.app_id();
}

}  // namespace

// Verifies the behavior of Blink extensions for Isolated Web Apps in ChromeOS
// when the `BlinkExtensionChromeOS` flag is enabled.
class BlinkExtensionsWithFlagSetTest
    : public web_app::IsolatedWebAppBrowserTestHarness {
 public:
  BlinkExtensionsWithFlagSetTest() = default;

  void SetUpCommandLine(base::CommandLine* command_line) override {
    web_app::IsolatedWebAppBrowserTestHarness::SetUpCommandLine(command_line);
    // TODO(crbug.com/480133572): Enable the flag automatically.
    command_line->AppendSwitchASCII(switches::kEnableBlinkFeatures,
                                    "BlinkExtensionChromeOS");
  }
};

IN_PROC_BROWSER_TEST_F(BlinkExtensionsWithFlagSetTest,
                       IsolatedWebAppCanAccessExtensions) {
  webapps::AppId app_id = InstallIsolatedWebAppAndReturnAppId(profile());
  content::RenderFrameHost* frame = OpenApp(app_id);

  EXPECT_EQ(true, content::EvalJs(frame, "'chromeos' in window"));
  EXPECT_EQ(true,
            content::EvalJs(frame, "'isolatedWebApp' in window.chromeos"));
}

// In practice the `BlinkExtensionChromeOS` feature remains disabled for regular
// websites, so `window.chromeos` should be undefined. We force enable the flag
// in this test to verify `chromeos.isolatedWebApp` is still undefined in that
// case.
IN_PROC_BROWSER_TEST_F(BlinkExtensionsWithFlagSetTest,
                       RegularPageCannotAccessExtensions) {
  ASSERT_TRUE(
      ui_test_utils::NavigateToURL(browser(), GURL("https://example.com")));
  content::WebContents* web_contents =
      browser()->tab_strip_model()->GetActiveWebContents();

  // `window.chromeos` should exist because the flag is enabled.
  EXPECT_EQ(true, content::EvalJs(web_contents, "'chromeos' in window"));
  // window.chromeos.isolatedWebApp should not exist because it's
  // IsolatedContext.
  EXPECT_EQ(false, content::EvalJs(web_contents,
                                   "'isolatedWebApp' in window.chromeos"));
}

// Verifies the behavior of Blink extensions for Isolated Web Apps in ChromeOS
// when the `BlinkExtensionChromeOS` flag is left in its default disabled value.
using BlinkExtensionsWithDefaultFlagTest =
    web_app::IsolatedWebAppBrowserTestHarness;

IN_PROC_BROWSER_TEST_F(BlinkExtensionsWithDefaultFlagTest,
                       ExtensionsAreUndefined) {
  webapps::AppId app_id = InstallIsolatedWebAppAndReturnAppId(profile());
  content::RenderFrameHost* app_frame = OpenApp(app_id);

  EXPECT_EQ(false, content::EvalJs(app_frame, "'chromeos' in window"));
}

}  // namespace ash
