// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/strings/stringprintf.h"
#include "build/build_config.h"
#include "build/chromeos_buildflags.h"
#include "chrome/browser/extensions/extension_apitest.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/extensions/extension_action_test_helper.h"
#include "chrome/test/base/ui_test_utils.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "content/public/test/test_utils.h"
#include "extensions/test/result_catcher.h"
#include "extensions/test/test_extension_dir.h"
#include "media/base/media_switches.h"

class AutoplayExtensionBrowserTest : public extensions::ExtensionApiTest {
 public:
  void SetUpCommandLine(base::CommandLine* command_line) override {
    extensions::ExtensionBrowserTest::SetUpCommandLine(command_line);
    command_line->AppendSwitchASCII(
        switches::kAutoplayPolicy,
        switches::autoplay::kDocumentUserActivationRequiredPolicy);
  }
};

IN_PROC_BROWSER_TEST_F(AutoplayExtensionBrowserTest, AutoplayAllowed) {
  ASSERT_TRUE(RunExtensionTest("autoplay")) << message_;
}

// TODO(crbug.com/40742402): AutoplayAllowedInIframe sporadically (~10%?) times
// out on Linux.
// TODO(crbug.com/40118868): Revisit once build flag switch of lacros-chrome is
// complete.
#if BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_CHROMEOS_LACROS)
#define MAYBE_AutoplayAllowedInIframe DISABLED_AutoplayAllowedInIframe
#else
#define MAYBE_AutoplayAllowedInIframe AutoplayAllowedInIframe
#endif  // BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_CHROMEOS_LACROS)
IN_PROC_BROWSER_TEST_F(AutoplayExtensionBrowserTest,
                       MAYBE_AutoplayAllowedInIframe) {
  ASSERT_TRUE(StartEmbeddedTestServer());

  const extensions::Extension* extension =
      LoadExtension(test_data_dir_.AppendASCII("autoplay_iframe"));
  ASSERT_TRUE(extension) << message_;

  std::unique_ptr<ExtensionActionTestHelper> browser_action_test_util =
      ExtensionActionTestHelper::Create(browser());
  extensions::ResultCatcher catcher;
  browser_action_test_util->Press(extension->id());
  EXPECT_TRUE(catcher.GetNextResult()) << catcher.message();
}

IN_PROC_BROWSER_TEST_F(AutoplayExtensionBrowserTest,
                       AutoplayAllowedInHostedApp) {
  ASSERT_TRUE(StartEmbeddedTestServer());
  GURL app_url = embedded_test_server()->GetURL(
      "/extensions/autoplay_hosted_app/main.html");

  constexpr const char kHostedAppManifest[] =
      R"( { "name": "Hosted App Autoplay Test",
            "version": "1",
            "manifest_version": 2,
            "app": {
              "launch": {
                "web_url": "%s"
              }
            }
          } )";
  extensions::TestExtensionDir test_app_dir;
  test_app_dir.WriteManifest(
      base::StringPrintf(kHostedAppManifest, app_url.spec().c_str()));

  const extensions::Extension* extension =
      LoadExtension(test_app_dir.UnpackedPath());
  ASSERT_TRUE(extension) << message_;

  Browser* app_browser = LaunchAppBrowser(extension);
  content::WebContents* web_contents =
      app_browser->tab_strip_model()->GetActiveWebContents();
  EXPECT_TRUE(content::WaitForLoadStop(web_contents));

  EXPECT_EQ(true, content::EvalJs(web_contents, "runTest();",
                                  content::EXECUTE_SCRIPT_NO_USER_GESTURE));
}
