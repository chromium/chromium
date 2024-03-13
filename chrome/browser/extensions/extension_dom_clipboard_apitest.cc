// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/feature_list.h"
#include "base/strings/stringprintf.h"
#include "build/build_config.h"
#include "chrome/browser/extensions/extension_apitest.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/test/base/ui_test_utils.h"
#include "content/public/browser/web_contents.h"
#include "content/public/common/content_client.h"
#include "content/public/common/content_features.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "extensions/browser/background_script_executor.h"
#include "extensions/browser/script_executor.h"
#include "extensions/test/test_extension_dir.h"
#include "net/dns/mock_host_resolver.h"
#include "net/test/embedded_test_server/embedded_test_server.h"
#include "url/gurl.h"

namespace extensions {

namespace {

class ClipboardApiTest : public ExtensionApiTest {
 public:
  void SetUpOnMainThread() override {
    ExtensionApiTest::SetUpOnMainThread();
    host_resolver()->AddRule("*", "127.0.0.1");
  }

  bool LoadHostedApp(const std::string& app_name,
                     const std::string& launch_page);
  bool ExecuteCopyInSelectedTab();
  bool ExecutePasteInSelectedTab();
  bool ExecuteCommandInIframeInSelectedTab(const char* command);

 private:
  bool ExecuteScriptInSelectedTab(
      const std::string& script,
      int options = content::EXECUTE_SCRIPT_DEFAULT_OPTIONS);
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
  EXPECT_TRUE(ui_test_utils::NavigateToURL(browser(),
                                           base_url.Resolve(launch_page_path)));

  return true;
}

bool ClipboardApiTest::ExecuteCopyInSelectedTab() {
  const char kScript[] = "document.execCommand('copy')";
  return ExecuteScriptInSelectedTab(kScript);
}

bool ClipboardApiTest::ExecutePasteInSelectedTab() {
  const char kScript[] = "document.execCommand('paste')";
  return ExecuteScriptInSelectedTab(kScript);
}

bool ClipboardApiTest::ExecuteCommandInIframeInSelectedTab(
    const char* command) {
  const char kScript[] =
      "var ifr = document.createElement('iframe');\n"
      "document.body.appendChild(ifr);\n"
      "new Promise(res => {\n"
      "  window.resolve = res;\n"
      "  ifr.contentDocument.write('<script>parent.resolve("
      "    document.execCommand(\"%s\"))</script>');\n"
      "});";
  return ExecuteScriptInSelectedTab(base::StringPrintf(kScript, command));
}

bool ClipboardApiTest::ExecuteScriptInSelectedTab(const std::string& script,
                                                  int options) {
  return content::EvalJs(browser()->tab_strip_model()->GetActiveWebContents(),
                         script, options)
      .ExtractBool();
}

}  // namespace

// Flaky on Mac. See https://crbug.com/1242373.
#if BUILDFLAG(IS_MAC)
#define MAYBE_Extension DISABLED_Extension
#else
#define MAYBE_Extension Extension
#endif
IN_PROC_BROWSER_TEST_F(ClipboardApiTest, MAYBE_Extension) {
  ASSERT_TRUE(StartEmbeddedTestServer());
  ASSERT_TRUE(RunExtensionTest("clipboard/extension")) << message_;
}

// Flaky on Mac. See https://crbug.com/900301.
#if BUILDFLAG(IS_MAC)
#define MAYBE_ExtensionNoPermission DISABLED_ExtensionNoPermission
#else
#define MAYBE_ExtensionNoPermission ExtensionNoPermission
#endif
IN_PROC_BROWSER_TEST_F(ClipboardApiTest, MAYBE_ExtensionNoPermission) {
  ASSERT_TRUE(StartEmbeddedTestServer());
  ASSERT_TRUE(RunExtensionTest("clipboard/extension_no_permission"))
      << message_;
}

// Regression test for crbug.com/1051198
IN_PROC_BROWSER_TEST_F(ClipboardApiTest, BrowserPermissionCheck) {
  ASSERT_TRUE(StartEmbeddedTestServer());

  content::RenderFrameHost* render_frame_host = ui_test_utils::NavigateToURL(
      browser(), embedded_test_server()->GetURL("/english_page.html"));
  // No extensions are installed. Clipboard access should be disallowed.
  EXPECT_FALSE(
      content::GetContentClientForTesting()->browser()->IsClipboardPasteAllowed(
          render_frame_host));

  static constexpr char kManifest[] =
      R"({
         "name": "Ext",
         "manifest_version": 3,
         "version": "1",
         "background": {"service_worker": "background.js"},
         "permissions": ["scripting", "clipboardRead"],
         "host_permissions": ["<all_urls>"]
       })";
  TestExtensionDir test_dir;
  test_dir.WriteManifest(kManifest);
  test_dir.WriteFile(FILE_PATH_LITERAL("background.js"), "// blank ");

  const Extension* extension = LoadExtension(test_dir.UnpackedPath());
  ASSERT_TRUE(extension);

  // Even with an extension installed, clipboard access is disallowed for
  // the page.
  EXPECT_FALSE(
      content::GetContentClientForTesting()->browser()->IsClipboardPasteAllowed(
          render_frame_host));

  // Inject a script on the page through the extension.
  static constexpr char kScript[] =
      R"(
       (async () => {
         let tabs = await chrome.tabs.query({active: true});
         await chrome.scripting.executeScript(
             {target: {tabId: tabs[0].id},
             func: function() {}} );
         chrome.test.sendScriptResult('done');
       })();)";

  // This will execute the script and wait for it to complete, ensuring
  // the browser is aware of the executing content script.
  BackgroundScriptExecutor::ExecuteScript(
      profile(), extension->id(), kScript,
      BackgroundScriptExecutor::ResultCapture::kSendScriptResult);
  // Now the page should have access to the clipboard.
  EXPECT_TRUE(
      content::GetContentClientForTesting()->browser()->IsClipboardPasteAllowed(
          render_frame_host));
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

}  // namespace extensions
