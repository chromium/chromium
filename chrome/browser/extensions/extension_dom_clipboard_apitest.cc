// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/feature_list.h"
#include "base/strings/stringprintf.h"
#include "build/build_config.h"
#include "chrome/browser/extensions/extension_apitest.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/test/base/ui_test_utils.h"
#include "content/public/browser/web_contents.h"
#include "content/public/common/content_features.h"
#include "content/public/test/browser_test_utils.h"
#include "net/dns/mock_host_resolver.h"
#include "net/test/embedded_test_server/embedded_test_server.h"
#include "url/gurl.h"

namespace {

class ClipboardApiTest : public extensions::ExtensionApiTest {
 public:
  void SetUpOnMainThread() override {
    extensions::ExtensionApiTest::SetUpOnMainThread();
    host_resolver()->AddRule("*", "127.0.0.1");
  }

  bool LoadHostedApp(const std::string& app_name,
                     const std::string& launch_page);
  bool ExecuteCopyInSelectedTab();
  bool ExecutePasteInSelectedTab();
  bool ExecuteCommandInIframeInSelectedTab(const char* command);

 private:
  bool ExecuteScriptInSelectedTab(const std::string& script);
};

bool ClipboardApiTest::LoadHostedApp(const std::string& app_name,
                                     const std::string& launch_page) {
  if (!StartEmbeddedTestServer()) {
    message_ = "Failed to start test server.";
    return false;
  }

  if (!LoadExtension(test_data_dir_.AppendASCII("clipboard")
                                   .AppendASCII(app_name))) {
    message_ = "Failed to load hosted app.";
    return false;
  }

  GURL base_url = embedded_test_server()->GetURL(
      "/extensions/api_test/clipboard/");
  GURL::Replacements replace_host;
  replace_host.SetHostStr("localhost");
  base_url = base_url.ReplaceComponents(replace_host);

  std::string launch_page_path =
      base::StringPrintf("%s/%s", app_name.c_str(), launch_page.c_str());
  ui_test_utils::NavigateToURL(browser(), base_url.Resolve(launch_page_path));

  return true;
}

bool ClipboardApiTest::ExecuteCopyInSelectedTab() {
  const char kScript[] =
      "window.domAutomationController.send(document.execCommand('copy'))";
  return ExecuteScriptInSelectedTab(kScript);
}

bool ClipboardApiTest::ExecutePasteInSelectedTab() {
  const char kScript[] =
      "window.domAutomationController.send(document.execCommand('paste'))";
  return ExecuteScriptInSelectedTab(kScript);
}

bool ClipboardApiTest::ExecuteCommandInIframeInSelectedTab(
    const char* command) {
  const char kScript[] =
      "var ifr = document.createElement('iframe');\n"
      "document.body.appendChild(ifr);\n"
      "ifr.contentDocument.write('<script>parent.domAutomationController.send("
          "document.execCommand(\"%s\"))</script>');";
  return ExecuteScriptInSelectedTab(base::StringPrintf(kScript, command));
}

bool ClipboardApiTest::ExecuteScriptInSelectedTab(const std::string& script) {
  bool result;
  CHECK(content::ExecuteScriptAndExtractBool(
        browser()->tab_strip_model()->GetActiveWebContents(),
        script,
        &result));
  return result;
}

}  // namespace

IN_PROC_BROWSER_TEST_F(ClipboardApiTest, Extension) {
  ASSERT_TRUE(StartEmbeddedTestServer());
  ASSERT_TRUE(RunExtensionTest("clipboard/extension")) << message_;
}

// Flaky on Mac. See https://crbug.com/900301.
#if defined(OS_MACOSX)
#define MAYBE_ExtensionNoPermission DISABLED_ExtensionNoPermission
#else
#define MAYBE_ExtensionNoPermission ExtensionNoPermission
#endif
IN_PROC_BROWSER_TEST_F(ClipboardApiTest, MAYBE_ExtensionNoPermission) {
  ASSERT_TRUE(StartEmbeddedTestServer());
  ASSERT_TRUE(RunExtensionTest("clipboard/extension_no_permission"))
      << message_;
}

IN_PROC_BROWSER_TEST_F(ClipboardApiTest, HostedApp) {
  ASSERT_TRUE(LoadHostedApp("hosted_app", "main.html")) << message_;

  EXPECT_TRUE(ExecuteCopyInSelectedTab()) << message_;
  EXPECT_TRUE(ExecutePasteInSelectedTab()) << message_;
  EXPECT_TRUE(ExecuteCommandInIframeInSelectedTab("copy")) << message_;
  EXPECT_TRUE(ExecuteCommandInIframeInSelectedTab("paste")) << message_;
}

IN_PROC_BROWSER_TEST_F(ClipboardApiTest, HostedAppNoPermission) {
  ASSERT_TRUE(LoadHostedApp("hosted_app_no_permission", "main.html"))
      << message_;

  // TODO(dcheng): The test coverage here is incomplete. The content test utils
  // for executing script force a user gesture, so it's impossible to test
  // the no user gesture case without a lot of code duplication.
  EXPECT_TRUE(ExecuteCopyInSelectedTab()) << message_;
  EXPECT_FALSE(ExecutePasteInSelectedTab()) << message_;

  // User acitvation doesn't propagate to a child frame.
  EXPECT_FALSE(ExecuteCommandInIframeInSelectedTab("copy")) << message_;
  EXPECT_FALSE(ExecuteCommandInIframeInSelectedTab("paste")) << message_;
}
