// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "build/build_config.h"
#include "chrome/browser/extensions/extension_apitest.h"
#include "chrome/browser/ui/browser_tabstrip.h"
#include "chrome/test/base/ui_test_utils.h"
#include "content/public/common/content_switches.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/test_navigation_observer.h"
#include "extensions/test/extension_test_message_listener.h"
#include "extensions/test/result_catcher.h"
#include "extensions/test/test_extension_dir.h"

namespace extensions {

// Failed run on ChromeOS CI builder. https://crbug.com/1245240
#if BUILDFLAG(IS_CHROMEOS)
#define MAYBE_GetViews DISABLED_GetViews
#else
#define MAYBE_GetViews GetViews
#endif
IN_PROC_BROWSER_TEST_F(ExtensionApiTest, MAYBE_GetViews) {
  ASSERT_TRUE(RunExtensionTest("get_views")) << message_;
}

class ExtensionApiTestWithoutAutomationController : public ExtensionApiTest {
 public:
  void SetUpInProcessBrowserTestFixture() override {
    // This switch must be removed in SetUpInProcessBrowserTestFixture() instead
    // of SetUpCommandLine() because BrowserTestBase::SetUp() adds the switch
    // after SetUpCommandLine() is called.
    base::CommandLine* command_line = base::CommandLine::ForCurrentProcess();

    base::CommandLine new_command_line(command_line->GetProgram());
    base::CommandLine::SwitchMap switches = command_line->GetSwitches();
    // Disable kDomAutomationController to avoid creating the V8 context for
    // every frame. This interferes with the behavior we are trying to emulate
    // for the regression test.
    switches.erase(switches::kDomAutomationController);

    for (const auto& it : switches)
      new_command_line.AppendSwitchNative(it.first, it.second);

    *command_line = new_command_line;
  }
};

// Regression test for http://crbug.com/1349787.
IN_PROC_BROWSER_TEST_F(ExtensionApiTestWithoutAutomationController,
                       GetWebAccessibleExtensionView) {
  ASSERT_TRUE(embedded_test_server()->Start());
  ASSERT_TRUE(ui_test_utils::NavigateToURL(
      browser(), embedded_test_server()->GetURL("/echo")));

  TestExtensionDir test_dir;
  test_dir.WriteManifest(R"({
        "name": "GetViews Test",
        "manifest_version": 2,
        "version": "0.1",
        "background": {"scripts": ["background.js"]},
        "web_accessible_resources": ["page.html"]
      })");
  test_dir.WriteFile(FILE_PATH_LITERAL("background.js"), R"(
        chrome.test.sendMessage('ready', function() {
          var views = chrome.extension.getViews();
          chrome.test.assertEq(2, views.length);

          let paths = views.map((v) => v.location.pathname).sort();
          chrome.test.assertEq(
              ['/_generated_background_page.html', '/page.html'], paths);

          chrome.test.notifyPass();
        });
      )");
  test_dir.WriteFile(FILE_PATH_LITERAL("page.html"), "<html></html>");

  ResultCatcher result_catcher;
  ExtensionTestMessageListener listener("ready", ReplyBehavior::kWillReply);
  const Extension* extension = LoadExtension(test_dir.UnpackedPath());
  ASSERT_TRUE(extension);
  EXPECT_TRUE(listener.WaitUntilSatisfied());

  auto* tab = chrome::AddSelectedTabWithURL(
      browser(), extension->GetResourceURL("page.html"),
      ui::PAGE_TRANSITION_LINK);
  content::TestNavigationObserver observer(tab);
  observer.Wait();

  listener.Reply("ok");
  EXPECT_TRUE(result_catcher.GetNextResult()) << result_catcher.message();
}

}  // namespace extensions
