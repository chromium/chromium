// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <optional>

#include "ash/constants/ash_features.h"
#include "base/memory/raw_ptr.h"
#include "base/strings/strcat.h"
#include "base/strings/stringprintf.h"
#include "base/system/sys_info.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/simple_test_clock.h"
#include "chrome/browser/ash/system_web_apps/test_support/system_web_app_integration_test.h"
#include "chrome/browser/devtools/devtools_window_testing.h"
#include "chrome/browser/error_reporting/mock_chrome_js_error_report_processor.h"
#include "chrome/browser/extensions/api/crash_report_private/crash_report_private_api.h"
#include "chrome/browser/extensions/extension_apitest.h"
#include "chrome/browser/ui/web_applications/test/web_app_browsertest_util.h"
#include "chrome/browser/web_applications/test/web_app_install_test_utils.h"
#include "chrome/browser/web_applications/web_app_utils.h"
#include "chrome/common/chrome_features.h"
#include "components/crash/content/browser/error_reporting/mock_crash_endpoint.h"
#include "content/public/test/browser_task_environment.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "extensions/browser/extension_host.h"
#include "extensions/browser/process_manager.h"
#include "extensions/common/switches.h"
#include "extensions/test/extension_test_message_listener.h"
#include "extensions/test/test_extension_dir.h"
#include "testing/gmock/include/gmock/gmock-matchers.h"

namespace extensions {

namespace {

using ::testing::HasSubstr;
using ::testing::MatchesRegex;

constexpr const char* kTestExtensionId = "jjeoclcdfjddkdjokiejckgcildcflpp";

}  // namespace

class CrashReportPrivateApiTest : public ExtensionApiTest {
 public:
  CrashReportPrivateApiTest() = default;

  CrashReportPrivateApiTest(const CrashReportPrivateApiTest&) = delete;
  CrashReportPrivateApiTest& operator=(const CrashReportPrivateApiTest&) =
      delete;

  ~CrashReportPrivateApiTest() override = default;

  void SetUpOnMainThread() override {
    ExtensionApiTest::SetUpOnMainThread();

    static constexpr char kKey[] =
        "MIGfMA0GCSqGSIb3DQEBAQUAA4GNADCBiQKBgQC+uU63MD6T82Ldq5wjrDFn5mGmPnnnj"
        "WZBWxYXfpG4kVf0s+p24VkXwTXsxeI12bRm8/ft9sOq0XiLfgQEh5JrVUZqvFlaZYoS+g"
        "iZfUqzKFGMLa4uiSMDnvv+byxrqAepKz5G8XX/q5Wm5cvpdjwgiu9z9iM768xJy+Ca/G5"
        "qQwIDAQAB";
    static constexpr char kManifestTemplate[] =
        R"({
      "key": "%s",
      "name": "chrome.crashReportPrivate basic extension tests",
      "version": "1.0",
      "manifest_version": 2,
      "background": { "scripts": ["test.js"] },
      "permissions": ["crashReportPrivate"]
    })";

    TestExtensionDir test_dir;
    test_dir.WriteManifest(base::StringPrintf(kManifestTemplate, kKey));
    test_dir.WriteFile(FILE_PATH_LITERAL("test.js"),
                       R"(chrome.test.sendMessage('ready');)");

    ExtensionTestMessageListener listener("ready");
    extension_ = LoadExtension(test_dir.UnpackedPath());
    EXPECT_TRUE(listener.WaitUntilSatisfied());

    crash_endpoint_ =
        std::make_unique<MockCrashEndpoint>(embedded_test_server());
    processor_ = std::make_unique<ScopedMockChromeJsErrorReportProcessor>(
        *crash_endpoint_);
  }

  void SetUpCommandLine(base::CommandLine* command_line) override {
    command_line->AppendSwitchASCII(
        extensions::switches::kAllowlistedExtensionID, kTestExtensionId);
    ExtensionApiTest::SetUpCommandLine(command_line);
  }

 protected:
  const std::optional<MockCrashEndpoint::Report>& last_report() {
    return crash_endpoint_->last_report();
  }
  void clear_last_report() { crash_endpoint_->clear_last_report(); }
  raw_ptr<const Extension, DanglingUntriaged> extension_;
  std::unique_ptr<MockCrashEndpoint> crash_endpoint_;
  std::unique_ptr<ScopedMockChromeJsErrorReportProcessor> processor_;
};

IN_PROC_BROWSER_TEST_F(CrashReportPrivateApiTest, Basic) {
  static constexpr char kTestScript[] = R"(
    chrome.crashReportPrivate.reportError({
        message: "hi",
        url: "http://www.test.com",
      },
      () => chrome.test.sendScriptResult(""));
  )";
  ExecuteScriptInBackgroundPage(extension_->id(), kTestScript);

  const std::optional<MockCrashEndpoint::Report>& report = last_report();
  ASSERT_TRUE(report);
  EXPECT_EQ(report->GetQueryParam("app_locale"), "en-US");
  EXPECT_EQ(report->GetQueryParam("browser"), "Chrome");
  EXPECT_THAT(report->GetQueryParam("browser_process_uptime_ms").value_or(""),
              MatchesRegex("\\d+"));
  EXPECT_EQ(report->GetQueryParam("browser_version"), "1.2.3.4");
  EXPECT_EQ(report->GetQueryParam("channel"), "Stable");
  EXPECT_EQ(report->GetQueryParam("error_message"), "hi");
  EXPECT_EQ(report->GetQueryParam("full_url"), "http://www.test.com/");
  EXPECT_EQ(report->GetQueryParam("num-experiments"), "1");
  EXPECT_EQ(report->GetQueryParam("os"), "ChromeOS");
  EXPECT_EQ(report->GetQueryParam("prod"), "Chrome_ChromeOS");
  EXPECT_THAT(report->GetQueryParam("renderer_process_uptime_ms").value_or(""),
              MatchesRegex("\\d+"));
  EXPECT_EQ(report->GetQueryParam("source_system"), "crash_report_api");
  EXPECT_EQ(report->GetQueryParam("src"), "http://www.test.com/");
  EXPECT_EQ(report->GetQueryParam("type"), "JavascriptError");
  EXPECT_EQ(report->GetQueryParam("url"), "/");
  EXPECT_EQ(report->GetQueryParam("variations"),
            MockChromeJsErrorReportProcessor::kDefaultExperimentListString);
  EXPECT_EQ(report->GetQueryParam("ver"), "1.2.3.4");
  EXPECT_EQ(report->content(), "");
}

IN_PROC_BROWSER_TEST_F(CrashReportPrivateApiTest, ExtraParamsAndStackTrace) {
  static constexpr char kTestScript[] = R"-(
    chrome.crashReportPrivate.reportError({
        message: "hi",
        url: "http://www.test.com/foo",
        product: "Chrome (Chrome OS)",
        version: "1.0.0.0",
        lineNumber: 123,
        columnNumber: 456,
        debugId: "2751679EE:233977D75E03BAC9DA/255DD0",
        stackTrace: "   at <anonymous>:1:1",
      },
      () => chrome.test.sendScriptResult(""));
  )-";
  ExecuteScriptInBackgroundPage(extension_->id(), kTestScript);

  const std::optional<MockCrashEndpoint::Report>& report = last_report();
  ASSERT_TRUE(report);
  EXPECT_EQ(report->GetQueryParam("app_locale"), "en-US");
  EXPECT_EQ(report->GetQueryParam("browser"), "Chrome");
  EXPECT_THAT(report->GetQueryParam("browser_process_uptime_ms").value_or(""),
              MatchesRegex("\\d+"));
  EXPECT_EQ(report->GetQueryParam("browser_version"), "1.2.3.4");
  EXPECT_EQ(report->GetQueryParam("channel"), "Stable");
  EXPECT_EQ(report->GetQueryParam("column"), "456");
  EXPECT_EQ(report->GetQueryParam("debug_id"),
            "2751679EE:233977D75E03BAC9DA/255DD0");
  EXPECT_EQ(report->GetQueryParam("error_message"), "hi");
  EXPECT_EQ(report->GetQueryParam("full_url"), "http://www.test.com/foo");
  EXPECT_EQ(report->GetQueryParam("line"), "123");
  EXPECT_EQ(report->GetQueryParam("num-experiments"), "1");
  EXPECT_EQ(report->GetQueryParam("os"), "ChromeOS");
  // The product name is escaped twice. The first time, it becomes
  // "Chrome%20(Chrome%20OS)" and then the second escapes the '%' into '%25'.
  EXPECT_EQ(report->GetQueryParam("prod"), "Chrome%20(Chrome%20OS)");
  EXPECT_THAT(report->GetQueryParam("renderer_process_uptime_ms").value_or(""),
              MatchesRegex("\\d+"));
  EXPECT_EQ(report->GetQueryParam("source_system"), "crash_report_api");
  EXPECT_EQ(report->GetQueryParam("src"), "http://www.test.com/foo");
  EXPECT_EQ(report->GetQueryParam("type"), "JavascriptError");
  EXPECT_EQ(report->GetQueryParam("url"), "/foo");
  EXPECT_EQ(report->GetQueryParam("variations"),
            MockChromeJsErrorReportProcessor::kDefaultExperimentListString);
  EXPECT_EQ(report->GetQueryParam("ver"), "1.0.0.0");
  EXPECT_EQ(report->content(), "   at <anonymous>:1:1");
}

IN_PROC_BROWSER_TEST_F(CrashReportPrivateApiTest, StackTraceWithErrorMessage) {
  static constexpr char kTestScript[] = R"(
    chrome.crashReportPrivate.reportError({
        message: "hi",
        url: "http://www.test.com/foo",
        product: 'TestApp',
        version: '1.0.0.0',
        lineNumber: 123,
        columnNumber: 456,
        stackTrace: 'hi'
      },
      () => chrome.test.sendScriptResult(""));
  )";
  ExecuteScriptInBackgroundPage(extension_->id(), kTestScript);

  const std::optional<MockCrashEndpoint::Report>& report = last_report();
  ASSERT_TRUE(report);
  EXPECT_EQ(report->GetQueryParam("app_locale"), "en-US");
  EXPECT_EQ(report->GetQueryParam("browser"), "Chrome");
  EXPECT_THAT(report->GetQueryParam("browser_process_uptime_ms").value_or(""),
              MatchesRegex("\\d+"));
  EXPECT_EQ(report->GetQueryParam("browser_version"), "1.2.3.4");
  EXPECT_EQ(report->GetQueryParam("channel"), "Stable");
  EXPECT_EQ(report->GetQueryParam("column"), "456");
  EXPECT_EQ(report->GetQueryParam("error_message"), "hi");
  EXPECT_EQ(report->GetQueryParam("full_url"), "http://www.test.com/foo");
  EXPECT_EQ(report->GetQueryParam("line"), "123");
  EXPECT_EQ(report->GetQueryParam("num-experiments"), "1");
  EXPECT_EQ(report->GetQueryParam("os"), "ChromeOS");
  EXPECT_EQ(report->GetQueryParam("prod"), "TestApp");
  EXPECT_THAT(report->GetQueryParam("renderer_process_uptime_ms").value_or(""),
              MatchesRegex("\\d+"));
  EXPECT_EQ(report->GetQueryParam("source_system"), "crash_report_api");
  EXPECT_EQ(report->GetQueryParam("src"), "http://www.test.com/foo");
  EXPECT_EQ(report->GetQueryParam("type"), "JavascriptError");
  EXPECT_EQ(report->GetQueryParam("url"), "/foo");
  EXPECT_EQ(report->GetQueryParam("variations"),
            MockChromeJsErrorReportProcessor::kDefaultExperimentListString);
  EXPECT_EQ(report->GetQueryParam("ver"), "1.0.0.0");
  EXPECT_EQ(report->content(), "");
}

IN_PROC_BROWSER_TEST_F(CrashReportPrivateApiTest, RedactMessage) {
  // We use the feedback APIs redaction tool, which scrubs many different types
  // of PII. As a sanity check, test if MAC addresses are redacted.
  static constexpr char kTestScript[] = R"(
    chrome.crashReportPrivate.reportError({
        message: "06-00-00-00-00-00",
        url: "http://www.test.com/foo",
        product: 'TestApp',
        version: '1.0.0.0',
        lineNumber: 123,
        columnNumber: 456,
      },
      () => chrome.test.sendScriptResult(""));
  )";
  ExecuteScriptInBackgroundPage(extension_->id(), kTestScript);

  const std::optional<MockCrashEndpoint::Report>& report = last_report();
  ASSERT_TRUE(report);
  EXPECT_EQ(report->GetQueryParam("app_locale"), "en-US");
  EXPECT_EQ(report->GetQueryParam("browser"), "Chrome");
  EXPECT_THAT(report->GetQueryParam("browser_process_uptime_ms").value_or(""),
              MatchesRegex("\\d+"));
  EXPECT_EQ(report->GetQueryParam("browser_version"), "1.2.3.4");
  EXPECT_EQ(report->GetQueryParam("channel"), "Stable");
  EXPECT_EQ(report->GetQueryParam("column"), "456");
  EXPECT_EQ(report->GetQueryParam("error_message"),
            "(MAC OUI=06:00:00 IFACE=1)");
  EXPECT_EQ(report->GetQueryParam("full_url"), "http://www.test.com/foo");
  EXPECT_EQ(report->GetQueryParam("line"), "123");
  EXPECT_EQ(report->GetQueryParam("num-experiments"), "1");
  EXPECT_EQ(report->GetQueryParam("os"), "ChromeOS");
  EXPECT_EQ(report->GetQueryParam("prod"), "TestApp");
  EXPECT_THAT(report->GetQueryParam("renderer_process_uptime_ms").value_or(""),
              MatchesRegex("\\d+"));
  EXPECT_EQ(report->GetQueryParam("source_system"), "crash_report_api");
  EXPECT_EQ(report->GetQueryParam("src"), "http://www.test.com/foo");
  EXPECT_EQ(report->GetQueryParam("type"), "JavascriptError");
  EXPECT_EQ(report->GetQueryParam("url"), "/foo");
  EXPECT_EQ(report->GetQueryParam("variations"),
            MockChromeJsErrorReportProcessor::kDefaultExperimentListString);
  EXPECT_EQ(report->GetQueryParam("ver"), "1.0.0.0");
  EXPECT_EQ(report->content(), "");
}

IN_PROC_BROWSER_TEST_F(CrashReportPrivateApiTest, SuppressedIfDevtoolsOpen) {
  // Open devtools, should suppress crash report.
  ProcessManager* process_manager = ProcessManager::Get(profile());
  ExtensionHost* host =
      process_manager->GetBackgroundHostForExtension(extension_->id());
  ASSERT_TRUE(host);
  content::WebContents* web_contents = host->host_contents();
  DevToolsWindow* devtools_window =
      DevToolsWindowTesting::OpenDevToolsWindowSync(
          web_contents, false /** is devtools docked. */);
  static constexpr char kTestScript[] = R"(
    chrome.crashReportPrivate.reportError({
        message: "hi",
        url: "http://www.test.com",
      },
      () => {
        chrome.test.sendScriptResult(chrome.runtime.lastError ?
            chrome.runtime.lastError.message : "")
      });
  )";
  const std::optional<MockCrashEndpoint::Report>& report = last_report();

  // Ensure error is not reported since devtools is open.
  clear_last_report();
  EXPECT_EQ("", ExecuteScriptInBackgroundPage(extension_->id(), kTestScript));
  ASSERT_FALSE(report);

  DevToolsWindowTesting::CloseDevToolsWindow(devtools_window);

  // Ensure error is not reported after devtools has been closed.
  clear_last_report();
  EXPECT_EQ("", ExecuteScriptInBackgroundPage(extension_->id(), kTestScript));
  ASSERT_FALSE(report);
}

// Test REGULAR_TABBED is detected when |CrashReportPrivate| is called from a
// tab's |web_contents|.
IN_PROC_BROWSER_TEST_F(CrashReportPrivateApiTest, CalledFromWebContentsInTab) {
  // Navigate to the text |extension_| that has access to |CrashReportPrivate|.
  const GURL extension_context_url(
      "chrome-extension://jjeoclcdfjddkdjokiejckgcildcflpp/"
      "_generated_background_page.html");
  content::WebContents* web_content = GetActiveWebContents();
  EXPECT_TRUE(content::NavigateToURL(web_content, extension_context_url));

  static constexpr char kTestScript[] = R"(
    chrome.crashReportPrivate.reportError({
        message: "hi",
        url: "http://www.test.com",
      },
      () => window.domAutomationController.send(""));
  )";
  // Run the script in the |web_content| that has loaded |extension_| instead of
  // |ExecuteScriptInBackgroundPage| so
  // |chrome::FindBrowserWithTab(web_contents)| is not |nullptr|.
  EXPECT_EQ(true, ExecJs(web_content, kTestScript));

  auto report = crash_endpoint_->WaitForReport();
  EXPECT_EQ(report.GetQueryParam("app_locale"), "en-US");
  EXPECT_EQ(report.GetQueryParam("browser"), "Chrome");
  EXPECT_THAT(report.GetQueryParam("browser_process_uptime_ms").value_or(""),
              MatchesRegex("\\d+"));
  EXPECT_EQ(report.GetQueryParam("browser_version"), "1.2.3.4");
  EXPECT_EQ(report.GetQueryParam("channel"), "Stable");
  EXPECT_EQ(report.GetQueryParam("error_message"), "hi");
  EXPECT_EQ(report.GetQueryParam("full_url"), "http://www.test.com/");
  EXPECT_EQ(report.GetQueryParam("num-experiments"), "1");
  EXPECT_EQ(report.GetQueryParam("os"), "ChromeOS");
  EXPECT_EQ(report.GetQueryParam("prod"), "Chrome_ChromeOS");
  EXPECT_THAT(report.GetQueryParam("renderer_process_uptime_ms").value_or(""),
              MatchesRegex("\\d+"));
  EXPECT_EQ(report.GetQueryParam("source_system"), "crash_report_api");
  EXPECT_EQ(report.GetQueryParam("src"), "http://www.test.com/");
  EXPECT_EQ(report.GetQueryParam("type"), "JavascriptError");
  EXPECT_EQ(report.GetQueryParam("url"), "/");
  EXPECT_EQ(report.GetQueryParam("variations"),
            MockChromeJsErrorReportProcessor::kDefaultExperimentListString);
  EXPECT_EQ(report.GetQueryParam("ver"), "1.2.3.4");
  EXPECT_EQ(report.GetQueryParam("window_type"), "REGULAR_TABBED");
  EXPECT_EQ(report.content(), "");
}

using CrashReportPrivateCalledFromSwaTest = ash::SystemWebAppIntegrationTest;

// Test WEB_APP is detected when |CrashReportPrivate| is called from an app
// window.
IN_PROC_BROWSER_TEST_P(CrashReportPrivateCalledFromSwaTest,
                       CalledFromWebContentsInWebAppWindow) {
  WaitForTestSystemAppInstall();
  // Set up test server to listen to handle crash reports & serve fake web app
  // content. Note: Creating a |MockCrashEndpoint| starts the server.
  MockCrashEndpoint endpoint(embedded_test_server());
  ScopedMockChromeJsErrorReportProcessor processor(endpoint);
  ASSERT_TRUE(embedded_test_server()->Started());
  // Create and launch a test web app, opens in an app window.
  GURL start_url = embedded_test_server()->GetURL("/test_app.html");
  auto web_app_info =
      web_app::WebAppInstallInfo::CreateWithStartUrlForTesting(start_url);
  webapps::AppId app_id =
      web_app::test::InstallWebApp(profile(), std::move(web_app_info));
  Browser* app_browser = web_app::LaunchWebAppBrowserAndWait(profile(), app_id);

  content::WebContents* web_content =
      app_browser->tab_strip_model()->GetActiveWebContents();
  // Navigate to chrome://media-app which was access to |CrashReportPrivate|
  // from the |WebContents| in the web app window.
  const GURL extension_context_url("chrome://media-app");
  EXPECT_TRUE(content::NavigateToURL(web_content, extension_context_url));

  static constexpr char kTestScript[] = R"(
    chrome.crashReportPrivate.reportError({
        message: "hi",
        url: "http://www.test.com",
      },
      () => window.domAutomationController.send(""));
  )";
  EXPECT_EQ(true, ExecJs(web_content, kTestScript));

  auto report = endpoint.WaitForReport();
  EXPECT_EQ(report.GetQueryParam("app_locale"), "en-US");
  EXPECT_EQ(report.GetQueryParam("browser"), "Chrome");
  EXPECT_THAT(report.GetQueryParam("browser_process_uptime_ms").value_or(""),
              MatchesRegex("\\d+"));
  EXPECT_EQ(report.GetQueryParam("browser_version"), "1.2.3.4");
  EXPECT_EQ(report.GetQueryParam("channel"), "Stable");
  EXPECT_EQ(report.GetQueryParam("error_message"), "hi");
  EXPECT_EQ(report.GetQueryParam("full_url"), "http://www.test.com/");
  EXPECT_EQ(report.GetQueryParam("num-experiments"), "1");
  EXPECT_EQ(report.GetQueryParam("os"), "ChromeOS");
  EXPECT_EQ(report.GetQueryParam("prod"), "Chrome_ChromeOS");
  EXPECT_THAT(report.GetQueryParam("renderer_process_uptime_ms").value_or(""),
              MatchesRegex("\\d+"));
  EXPECT_EQ(report.GetQueryParam("source_system"), "crash_report_api");
  EXPECT_EQ(report.GetQueryParam("src"), "http://www.test.com/");
  EXPECT_EQ(report.GetQueryParam("type"), "JavascriptError");
  EXPECT_EQ(report.GetQueryParam("url"), "/");
  EXPECT_EQ(report.GetQueryParam("variations"),
            MockChromeJsErrorReportProcessor::kDefaultExperimentListString);
  EXPECT_EQ(report.GetQueryParam("ver"), "1.2.3.4");
  EXPECT_EQ(report.GetQueryParam("window_type"), "WEB_APP");
  EXPECT_EQ(report.content(), "");
}

// Test SWA_WINDOW is detected when |CrashReportPrivate| is called from a
// System Web App window |web_contents|.
IN_PROC_BROWSER_TEST_P(CrashReportPrivateCalledFromSwaTest,
                       CalledFromWebContentsInSwaWindow) {
  WaitForTestSystemAppInstall();
  content::WebContents* web_content = LaunchApp(ash::SystemWebAppType::MEDIA);
  MockCrashEndpoint endpoint(embedded_test_server());
  ScopedMockChromeJsErrorReportProcessor processor(endpoint);

  static constexpr char kTestScript[] = R"(
    chrome.crashReportPrivate.reportError({
        message: "hi",
        url: "http://www.test.com",
      },
      () => window.domAutomationController.send(""));
  )";
  EXPECT_EQ(true, ExecJs(web_content, kTestScript));

  auto report = endpoint.WaitForReport();
  EXPECT_EQ(report.GetQueryParam("app_locale"), "en-US");
  EXPECT_EQ(report.GetQueryParam("browser"), "Chrome");
  EXPECT_THAT(report.GetQueryParam("browser_process_uptime_ms").value_or(""),
              MatchesRegex("\\d+"));
  EXPECT_EQ(report.GetQueryParam("browser_version"), "1.2.3.4");
  EXPECT_EQ(report.GetQueryParam("channel"), "Stable");
  EXPECT_EQ(report.GetQueryParam("error_message"), "hi");
  EXPECT_EQ(report.GetQueryParam("full_url"), "http://www.test.com/");
  EXPECT_EQ(report.GetQueryParam("num-experiments"), "1");
  EXPECT_EQ(report.GetQueryParam("os"), "ChromeOS");
  EXPECT_EQ(report.GetQueryParam("prod"), "Chrome_ChromeOS");
  EXPECT_THAT(report.GetQueryParam("renderer_process_uptime_ms").value_or(""),
              MatchesRegex("\\d+"));
  EXPECT_EQ(report.GetQueryParam("source_system"), "crash_report_api");
  EXPECT_EQ(report.GetQueryParam("src"), "http://www.test.com/");
  EXPECT_EQ(report.GetQueryParam("type"), "JavascriptError");
  EXPECT_EQ(report.GetQueryParam("url"), "/");
  EXPECT_EQ(report.GetQueryParam("variations"),
            MockChromeJsErrorReportProcessor::kDefaultExperimentListString);
  EXPECT_EQ(report.GetQueryParam("ver"), "1.2.3.4");
  EXPECT_EQ(report.GetQueryParam("window_type"), "SYSTEM_WEB_APP");
  EXPECT_EQ(report.content(), "");
}

INSTANTIATE_SYSTEM_WEB_APP_MANAGER_TEST_SUITE_REGULAR_PROFILE_P(
    CrashReportPrivateCalledFromSwaTest);

}  // namespace extensions
