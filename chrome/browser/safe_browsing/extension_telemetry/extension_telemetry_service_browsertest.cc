// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/safe_browsing/extension_telemetry/extension_telemetry_service.h"

#include <memory>
#include <string>
#include <string_view>
#include <vector>

#include "base/callback_list.h"
#include "base/check.h"
#include "base/command_line.h"
#include "base/files/file_path.h"
#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "base/path_service.h"
#include "base/strings/strcat.h"
#include "base/strings/string_util.h"
#include "base/test/protobuf_matchers.h"
#include "base/test/run_until.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/test_future.h"
#include "base/values.h"
#include "chrome/browser/enterprise/connectors/reporting/extension_telemetry_event_router.h"
#include "chrome/browser/enterprise/connectors/reporting/extension_telemetry_event_router_factory.h"
#include "chrome/browser/enterprise/connectors/reporting/realtime_reporting_client.h"
#include "chrome/browser/enterprise/connectors/reporting/realtime_reporting_client_factory.h"
#include "chrome/browser/enterprise/connectors/test/deep_scanning_test_utils.h"
#include "chrome/browser/extensions/extension_apitest.h"
#include "chrome/browser/extensions/extension_tab_util.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/safe_browsing/extension_telemetry/extension_telemetry_service_factory.h"
#include "chrome/browser/safe_browsing/extension_telemetry/search_hijacking_detector.h"
#include "chrome/browser/search_engines/template_url_service_factory.h"
#include "chrome/browser/ui/browser_window/public/browser_window_interface.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/common/chrome_paths.h"
#include "chrome/common/pref_names.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/search_test_utils.h"
#include "chrome/test/base/ui_test_utils.h"
#include "components/enterprise/connectors/core/reporting_constants.h"
#include "components/enterprise/isolated_mode/isolated_mode_features.h"
#include "components/enterprise/isolated_mode/scoped_isolated_mode_override_for_testing.h"
#include "components/keyed_service/content/browser_context_dependency_manager.h"
#include "components/keyed_service/content/browser_context_keyed_service_factory.h"
#include "components/omnibox/browser/autocomplete_match.h"
#include "components/policy/core/common/cloud/cloud_policy_client.h"
#include "components/policy/core/common/cloud/mock_cloud_policy_client.h"
#include "components/safe_browsing/content/browser/web_ui/safe_browsing_ui.h"
#include "components/safe_browsing/core/common/features.h"
#include "components/safe_browsing/core/common/proto/csd.pb.h"
#include "components/safe_browsing/core/common/safe_browsing_prefs.h"
#include "components/search_engines/template_url.h"
#include "components/search_engines/template_url_service.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/web_contents.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "extensions/browser/background_script_executor.h"
#include "extensions/browser/extensions_test.h"
#include "extensions/common/api/declarative_net_request/test_utils.h"
#include "extensions/common/extension.h"
#include "extensions/common/extension_features.h"
#include "extensions/common/extension_id.h"
#include "extensions/test/extension_test_message_listener.h"
#include "extensions/test/result_catcher.h"
#include "extensions/test/test_extension_dir.h"
#include "net/dns/mock_host_resolver.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

namespace safe_browsing {

namespace {

constexpr const char kExtensionName[] = "Simple Test Extension";
constexpr const char kExtensionVersion[] = "0.0.1";
constexpr const char kExtensionContactedHost[] = "example.com";

}  // namespace

using ::base::test::EqualsProto;
using JSCallStack = ExtensionTelemetryReportRequest_SignalInfo_JSCallStack;
using CookiesGetAllInfo =
    ExtensionTelemetryReportRequest_SignalInfo_CookiesGetAllInfo;
using DeclarativeNetRequestInfo =
    ExtensionTelemetryReportRequest_SignalInfo_DeclarativeNetRequestInfo;
using DeclarativeNetRequestActionInfo =
    ExtensionTelemetryReportRequest_SignalInfo_DeclarativeNetRequestActionInfo;
using GetAllArgsInfo =
    ExtensionTelemetryReportRequest_SignalInfo_CookiesGetAllInfo_GetAllArgsInfo;
using CookiesGetInfo =
    ExtensionTelemetryReportRequest_SignalInfo_CookiesGetInfo;
using GetArgsInfo =
    ExtensionTelemetryReportRequest_SignalInfo_CookiesGetInfo_GetArgsInfo;
using RemoteHostContactedInfo =
    ExtensionTelemetryReportRequest_SignalInfo_RemoteHostContactedInfo;
using RemoteHostInfo =
    ExtensionTelemetryReportRequest_SignalInfo_RemoteHostContactedInfo_RemoteHostInfo;
using DOMAccessInfo = ExtensionTelemetryReportRequest_SignalInfo_DOMAccessInfo;
using ScriptInjectionInfo =
    ExtensionTelemetryReportRequest_SignalInfo_ScriptInjectionInfo;
using TestRule = extensions::declarative_net_request::TestRule;
using TestHeaderInfo = extensions::declarative_net_request::TestHeaderInfo;

class ExtensionTelemetryServiceBrowserTest
    : public extensions::ExtensionApiTest {
 public:
  ExtensionTelemetryServiceBrowserTest() {
    scoped_feature_list_.InitWithFeatures(
        /*enabled_features=*/
        {kExtensionTelemetrySearchHijackingSignal,
         extensions_features::kIncludeJSCallStackInExtensionApiRequest,
         extensions_features::kEnterpriseExtensionDOMActivityTelemetry},
        /*disabled_features=*/{});
    CHECK(base::PathService::Get(chrome::DIR_TEST_DATA, &test_extension_dir_));
    test_extension_dir_ =
        test_extension_dir_.AppendASCII("safe_browsing/extension_telemetry");
  }

  void SetUpOnMainThread() override {
    extensions::ExtensionApiTest::SetUpOnMainThread();
    host_resolver()->AddRule("*", "127.0.0.1");

    // Helper to set up enterprise reporting and enable by default.
    event_report_validator_helper_ = std::make_unique<
        enterprise_connectors::test::EventReportValidatorHelper>(
        browser()->GetProfile(), /*browser_test=*/true);
    // Enable enterprise policy.
    enterprise_connectors::test::SetOnSecurityEventReporting(
        /*prefs=*/prefs(),
        /*enabled=*/true,
        /*enabled_event_names=*/{},
        /*enabled_opt_in_events=*/
        {{enterprise_connectors::kExtensionTelemetryEvent, {"*"}}});

    // Set the last upload time to Now() so that StartUploadCheck (delayed by
    // 15s at startup) does not trigger CreateAndUploadReport() and clear the
    // collected signals in the middle of tests.
    SetLastUploadTimeForExtensionTelemetry(*prefs(), base::Time::Now());
  }

  void TearDownOnMainThread() override {
    extensions::ExtensionApiTest::TearDownOnMainThread();
    event_report_validator_helper_.reset();
  }

 protected:
  content::WebContents* web_contents(BrowserWindowInterface* browser) const {
    return browser->GetTabStripModel()->GetActiveWebContents();
  }

  PrefService* prefs() { return browser()->GetProfile()->GetPrefs(); }

  ExtensionTelemetryService* telemetry_service() {
    return ExtensionTelemetryServiceFactory::GetForProfile(
        browser()->GetProfile());
  }

  bool IsTelemetryServiceEnabledForESB() {
    return IsEnabledForESB(*telemetry_service()) &&
           !telemetry_service()->signal_processors_.empty() &&
           telemetry_service()->timer_.IsRunning();
  }

  bool IsTelemetryServiceEnabledForEnterprise() {
    return IsTelemetryServiceEnabledForEnterprise(*telemetry_service());
  }

  bool IsExtensionStoreEmpty() {
    return telemetry_service()->extension_store_.empty();
  }

  bool IsEnterpriseExtensionStoreEmpty() {
    return telemetry_service()->enterprise_extension_store_.empty();
  }

  static bool IsEnabledForESB(const ExtensionTelemetryService& service) {
    return service.esb_enabled_;
  }

  static bool IsTelemetryServiceEnabledForEnterprise(
      const ExtensionTelemetryService& service) {
    return service.enterprise_enabled_ &&
           !service.enterprise_signal_processors_.empty() &&
           service.enterprise_timer_.IsRunning();
  }

  // Returns whether `service` holds enterprise signals from `extension_id`.
  static bool HasEnterpriseSignals(
      const ExtensionTelemetryService& service,
      const extensions::ExtensionId& extension_id) {
    return service.enterprise_extension_store_.contains(extension_id);
  }

  // Returns whether `service` holds ESB signals from `extension_id`.
  static bool HasEsbSignals(const ExtensionTelemetryService& service,
                            const extensions::ExtensionId& extension_id) {
    return service.extension_store_.contains(extension_id);
  }

  static void SendEnterpriseReport(ExtensionTelemetryService& service) {
    service.CreateAndSendEnterpriseReport();
  }

  // Makes the reporting client of `profile` upload through `client` until the
  // returned runner is destroyed.
  [[nodiscard]] static base::ScopedClosureRunner UseCloudPolicyClient(
      Profile* profile,
      policy::CloudPolicyClient* client) {
    auto* reporting_client =
        enterprise_connectors::RealtimeReportingClientFactory::GetForProfile(
            profile);
    // Detach the client set by `event_report_validator_helper_`, if any.
    reporting_client->SetBrowserCloudPolicyClientForTesting(nullptr);
    reporting_client->SetBrowserCloudPolicyClientForTesting(client);
    return base::ScopedClosureRunner(
        base::BindOnce(&enterprise_connectors::RealtimeReportingClient::
                           SetBrowserCloudPolicyClientForTesting,
                       base::Unretained(reporting_client), nullptr));
  }

  // Loads an extension with `incognito_mode` ("spanning" or "split") as its
  // "incognito" manifest key, enabled in Off-The-Record profiles. The
  // background of each of its instances sends "ready" to the test when it
  // starts, and then only runs the scripts sent by the tests. With
  // `with_content_script`, the extension also has a content script that
  // fetches a file from the page's origin and then sends "fetched" to the
  // test.
  extensions::ExtensionId LoadTestExtension(std::string_view incognito_mode,
                                            bool with_content_script = false) {
    extensions::TestExtensionDir& extension_dir =
        extension_dirs_.emplace_back();
    static constexpr char kManifest[] =
        R"({
           "name": "Test Extension",
           "version": "0.1",
           "manifest_version": 3,
           "incognito": "$1",
           "background": { "service_worker": "background.js" },
           "content_scripts": [$2],
           "permissions": ["cookies", "scripting", "tabs"],
           "host_permissions": ["<all_urls>"]
         })";
    std::string content_script;
    if (with_content_script) {
      content_script =
          R"({"matches": ["<all_urls>"], "js": ["content_script.js"]})";
      extension_dir.WriteFile(
          FILE_PATH_LITERAL("content_script.js"),
          "fetch('/extensions/test_file.txt')"
          ".then(() => chrome.test.sendMessage('fetched'));");
    }
    extension_dir.WriteManifest(base::ReplaceStringPlaceholders(
        kManifest, {std::string(incognito_mode), content_script},
        /*offsets=*/nullptr));
    extension_dir.WriteFile(FILE_PATH_LITERAL("background.js"),
                            "chrome.test.sendMessage('ready');");
    ExtensionTestMessageListener ready_listener("ready");
    const extensions::Extension* extension = LoadExtension(
        extension_dir.UnpackedPath(), {.allow_in_incognito = true});
    CHECK(extension);
    CHECK(ready_listener.WaitUntilSatisfied());
    return extension->id();
  }

  // Navigates the active tab of `browser` to a page on `host` and waits for
  // the content script of `LoadTestExtension()` to finish its fetch. Returns
  // whether both succeed.
  [[nodiscard]] bool NavigateAndWaitForContentScriptFetch(
      BrowserWindowInterface* browser,
      std::string_view host) {
    ExtensionTestMessageListener fetched_listener("fetched");
    return ui_test_utils::NavigateToURL(
               browser, embedded_test_server()->GetURL(host, "/empty.html")) &&
           fetched_listener.WaitUntilSatisfied();
  }

  // Enables the reporting of DOM activity signals, such as script injections,
  // on top of the extension telemetry signals.
  void EnableDomActivityReporting() {
    enterprise_connectors::test::SetOnSecurityEventReporting(
        /*prefs=*/prefs(),
        /*enabled=*/true,
        /*enabled_event_names=*/{},
        /*enabled_opt_in_events=*/
        {{enterprise_connectors::kExtensionTelemetryEvent, {"*"}},
         {enterprise_connectors::kExtensionDOMActivityEvent, {"*"}}});
  }

  // Returns the ID that extensions use for the active tab of `browser`.
  int ActiveTabId(BrowserWindowInterface* browser) const {
    return extensions::ExtensionTabUtil::GetTabId(web_contents(browser));
  }

  // Runs `call`, a promise-returning extension API call, in the background of
  // the extension's instance in `profile` and waits for it to finish.
  void RunInExtensionBackground(Profile* profile,
                                const extensions::ExtensionId& extension_id,
                                std::string_view call) {
    EXPECT_EQ(
        extensions::BackgroundScriptExecutor::ExecuteScript(
            profile, extension_id,
            base::StrCat(
                {call, ".then(() => chrome.test.sendScriptResult('done'))",
                 ".catch(e => chrome.test.sendScriptResult(String(e)));"}),
            extensions::BackgroundScriptExecutor::ResultCapture::
                kSendScriptResult),
        base::Value("done"));
  }

  // Navigates the tab with `tab_id` to `url` through chrome.tabs.update(),
  // called by the extension's instance in the regular profile.
  void UpdateTabFromExtension(const extensions::ExtensionId& extension_id,
                              int tab_id,
                              const GURL& url) {
    RunInExtensionBackground(
        browser()->GetProfile(), extension_id,
        content::JsReplace("chrome.tabs.update($1, {url: $2})", tab_id, url));
  }

  // Reads the "test" cookie of https://example.com/ from the cookie store
  // with `store_id` through chrome.cookies.`method`() ("get" or "getAll"),
  // called by the extension's instance in the regular profile.
  void GetCookiesFromExtension(const extensions::ExtensionId& extension_id,
                               std::string_view method,
                               std::string_view store_id) {
    RunInExtensionBackground(
        browser()->GetProfile(), extension_id,
        base::StrCat(
            {"chrome.cookies.", method,
             content::JsReplace("({url: $1, name: 'test', storeId: $2})",
                                "https://example.com/", store_id)}));
  }

  using ExtensionInfo = ExtensionTelemetryReportRequest_ExtensionInfo;
  const ExtensionInfo* GetExtensionInfoFromExtensionStore(
      const extensions::ExtensionId& extension_id) {
    auto iter = telemetry_service()->extension_store_.find(extension_id);
    if (iter == telemetry_service()->extension_store_.end()) {
      return nullptr;
    }
    return iter->second.get();
  }

  using TelemetryReport = ExtensionTelemetryReportRequest;
  std::unique_ptr<TelemetryReport> GetTelemetryReport() {
    return telemetry_service()->CreateReport();
  }

  std::unique_ptr<TelemetryReport> GetTelemetryReportForEnterprise() {
    return GetTelemetryReportForEnterprise(*telemetry_service());
  }

  static std::unique_ptr<TelemetryReport> GetTelemetryReportForEnterprise(
      ExtensionTelemetryService& service) {
    return service.CreateReportForEnterprise();
  }

  // Returns whether a signal of `extension_id` in `report` mentions `text`,
  // such as a URL, a host or a cookie name. The reports store them verbatim.
  static bool ReportMentions(const TelemetryReport& report,
                             const extensions::ExtensionId& extension_id,
                             std::string_view text) {
    for (const auto& extension_report : report.reports()) {
      if (extension_report.extension().id() != extension_id) {
        continue;
      }
      for (const auto& signal : extension_report.signals()) {
        if (signal.SerializeAsString().find(text) != std::string::npos) {
          return true;
        }
      }
    }
    return false;
  }

  // Returns whether the signals of `extension_id` in `report` mention
  // `reported` and not `unreported`.
  [[nodiscard]] static testing::AssertionResult ReportOnlyMentions(
      const TelemetryReport* report,
      const extensions::ExtensionId& extension_id,
      std::string_view reported,
      std::string_view unreported) {
    if (!report) {
      return testing::AssertionFailure() << "there is no report";
    }
    const bool mentions_reported =
        ReportMentions(*report, extension_id, reported);
    const bool mentions_unreported =
        ReportMentions(*report, extension_id, unreported);
    if (mentions_reported && !mentions_unreported) {
      return testing::AssertionSuccess();
    }
    return testing::AssertionFailure()
           << "the signals of " << extension_id
           << (mentions_reported ? " mention " : " do not mention ") << reported
           << " and" << (mentions_unreported ? " mention " : " do not mention ")
           << unreported;
  }

  base::test::ScopedFeatureList scoped_feature_list_;
  base::FilePath test_extension_dir_;
  std::unique_ptr<enterprise_connectors::test::EventReportValidatorHelper>
      event_report_validator_helper_;

 private:
  // The directories of the extensions loaded by `LoadTestExtension()`.
  std::vector<extensions::TestExtensionDir> extension_dirs_;
};

IN_PROC_BROWSER_TEST_F(ExtensionTelemetryServiceBrowserTest,
                       DetectsAndReportsCookiesGetAllSignal) {
  SetSafeBrowsingState(browser()->GetProfile()->GetPrefs(),
                       SafeBrowsingState::ENHANCED_PROTECTION);
  ASSERT_TRUE(StartEmbeddedTestServer());

  static constexpr char kManifest[] =
      R"({
         "name": "Simple Cookies Extension",
         "version": "0.1",
         "manifest_version": 3,
         "background": { "service_worker": "background.js" },
         "permissions": ["cookies"],
         "host_permissions": ["<all_urls>"]
       })";
  static constexpr char kBackground[] =
      R"(
        var TEST_DOMAIN = 'cookies.com';
        var TEST_BASIC_COOKIE = {
          url: 'https://extensions.' + TEST_DOMAIN,
          name: 'test_basic_cookie',
          value: 'helloworld'
        };

        chrome.test.runTests([
          async function getCookies() {
            await chrome.cookies.set(TEST_BASIC_COOKIE);
            for (let i = 0; i < 2; ++i) {
              await chrome.cookies.getAll({
                domain: TEST_DOMAIN,
                name: TEST_BASIC_COOKIE.name,
                url: TEST_BASIC_COOKIE.url
                });
              }
              chrome.test.succeed();
              },
         ]);)";

  extensions::TestExtensionDir test_dir;
  test_dir.WriteManifest(kManifest);
  test_dir.WriteFile(FILE_PATH_LITERAL("background.js"), kBackground);

  extensions::ResultCatcher result_catcher;
  const auto* extension = LoadExtension(test_dir.UnpackedPath());
  ASSERT_TRUE(extension);
  ASSERT_TRUE(result_catcher.GetNextResult());

  ASSERT_NE(telemetry_service(), nullptr);
  ASSERT_TRUE(IsTelemetryServiceEnabledForESB());

  // Verify the contents of telemetry report generated for ESB users.
  std::unique_ptr<TelemetryReport> telemetry_report_pb = GetTelemetryReport();
  ASSERT_NE(telemetry_report_pb, nullptr);
  // Retrieve the report corresponding to the test extension.
  int report_index = -1;
  for (int i = 0; i < telemetry_report_pb->reports_size(); i++) {
    if (telemetry_report_pb->reports(i).extension().id() == extension->id()) {
      report_index = i;
    }
  }
  ASSERT_NE(report_index, -1);

  const auto& extension_report = telemetry_report_pb->reports(report_index);
  EXPECT_EQ(extension_report.extension().id(), extension->id());
  EXPECT_EQ(extension_report.extension().name(), "Simple Cookies Extension");
  EXPECT_EQ(extension_report.extension().version(), "0.1");
  // Verify the designated test extension's report has signal data.
  ASSERT_EQ(extension_report.signals().size(), 1);
  // Verify that extension store has been cleared after creating a telemetry
  // report.
  EXPECT_TRUE(IsExtensionStoreEmpty());

  // Verify signal proto from the reports.
  const ExtensionTelemetryReportRequest_SignalInfo& signal =
      extension_report.signals()[0];
  const CookiesGetAllInfo& cookies_get_all_info = signal.cookies_get_all_info();
  ASSERT_EQ(cookies_get_all_info.get_all_args_info_size(), 1);
  ASSERT_EQ(cookies_get_all_info.max_exceeded_args_count(), 0u);

  const GetAllArgsInfo& get_all_args_info =
      cookies_get_all_info.get_all_args_info(0);
  EXPECT_EQ(get_all_args_info.count(), 2u);
  EXPECT_EQ(get_all_args_info.domain(), "cookies.com");
  EXPECT_EQ(get_all_args_info.name(), "test_basic_cookie");
  EXPECT_EQ(get_all_args_info.path(), "");
  EXPECT_FALSE(get_all_args_info.has_secure());
  EXPECT_EQ(get_all_args_info.store_id(), "0");
  EXPECT_EQ(get_all_args_info.url(), "https://extensions.cookies.com/");
  EXPECT_FALSE(get_all_args_info.has_is_session());
  // Check the JS call stack information.
  ASSERT_EQ(get_all_args_info.js_callstacks_size(), 1);
  const JSCallStack& callstack = get_all_args_info.js_callstacks(0);
  ASSERT_GE(callstack.frames_size(), 1);
  EXPECT_EQ(callstack.frames(0).script_name(), "/background.js");
  EXPECT_EQ(callstack.frames(0).function_name(), "getCookies");

  // Verify enterprise telemetry reporting.
  ASSERT_TRUE(IsTelemetryServiceEnabledForEnterprise());
  std::unique_ptr<TelemetryReport> enterprise_telemetry_report_pb =
      GetTelemetryReportForEnterprise();
  const auto& enterprise_extension_report =
      enterprise_telemetry_report_pb->reports(0);

  EXPECT_THAT(extension_report, EqualsProto(enterprise_extension_report));
  EXPECT_TRUE(IsEnterpriseExtensionStoreEmpty());
}

IN_PROC_BROWSER_TEST_F(ExtensionTelemetryServiceBrowserTest,
                       DetectsAndReportsCookiesGetSignal) {
  SetSafeBrowsingState(browser()->GetProfile()->GetPrefs(),
                       SafeBrowsingState::ENHANCED_PROTECTION);
  ASSERT_TRUE(StartEmbeddedTestServer());

  static constexpr char kManifest[] =
      R"({
         "name": "Simple Cookies Extension",
         "version": "0.1",
         "manifest_version": 3,
         "background": { "service_worker": "background.js" },
         "permissions": ["cookies"],
         "host_permissions": ["<all_urls>"]
       })";
  static constexpr char kBackground[] =
      R"(
        var TEST_BASIC_COOKIE = {
          url: 'https://extensions.cookies.com',
          name: 'test_basic_cookie',
          value: 'helloworld'
        };

        chrome.test.runTests([
          async function getCookies() {
            await chrome.cookies.set(TEST_BASIC_COOKIE);
            for (let i = 0; i < 2; ++i) {
              await chrome.cookies.get({
                name: TEST_BASIC_COOKIE.name,
                url: TEST_BASIC_COOKIE.url
              });
            }
            chrome.test.succeed();
          },
         ]);)";

  extensions::TestExtensionDir test_dir;
  test_dir.WriteManifest(kManifest);
  test_dir.WriteFile(FILE_PATH_LITERAL("background.js"), kBackground);

  extensions::ResultCatcher result_catcher;
  const auto* extension = LoadExtension(test_dir.UnpackedPath());
  ASSERT_TRUE(extension);
  ASSERT_TRUE(result_catcher.GetNextResult());

  ASSERT_NE(telemetry_service(), nullptr);
  ASSERT_TRUE(IsTelemetryServiceEnabledForESB());

  // Verify the contents of telemetry report generated for ESB users.
  std::unique_ptr<TelemetryReport> telemetry_report_pb = GetTelemetryReport();
  ASSERT_NE(telemetry_report_pb, nullptr);
  // Retrieve the report corresponding to the test extension.
  int report_index = -1;
  for (int i = 0; i < telemetry_report_pb->reports_size(); i++) {
    if (telemetry_report_pb->reports(i).extension().id() == extension->id()) {
      report_index = i;
    }
  }
    ASSERT_NE(report_index, -1);

    const auto& extension_report = telemetry_report_pb->reports(report_index);
    EXPECT_EQ(extension_report.extension().id(), extension->id());
    EXPECT_EQ(extension_report.extension().name(), "Simple Cookies Extension");
    EXPECT_EQ(extension_report.extension().version(), "0.1");
    // Verify the designated test extension's report has signal data.
    ASSERT_EQ(extension_report.signals().size(), 1);
    // Verify that extension store has been cleared after creating a telemetry
    // report.
    EXPECT_TRUE(IsExtensionStoreEmpty());

    // Verify signal proto from the reports.
    const ExtensionTelemetryReportRequest_SignalInfo& signal =
        extension_report.signals()[0];
    const CookiesGetInfo& cookies_get_info = signal.cookies_get_info();
    ASSERT_EQ(cookies_get_info.get_args_info_size(), 1);
    ASSERT_EQ(cookies_get_info.max_exceeded_args_count(),
              static_cast<uint32_t>(0));

    const GetArgsInfo& get_args_info = cookies_get_info.get_args_info(0);
    EXPECT_EQ(get_args_info.count(), static_cast<uint32_t>(2));
    EXPECT_EQ(get_args_info.name(), "test_basic_cookie");
    EXPECT_EQ(get_args_info.store_id(), "0");
    EXPECT_EQ(get_args_info.url(), "https://extensions.cookies.com/");
    // Check the JS call stack information.
    ASSERT_EQ(get_args_info.js_callstacks_size(), 1);
    const JSCallStack& callstack = get_args_info.js_callstacks(0);
    ASSERT_GE(callstack.frames_size(), 1);
    EXPECT_EQ(callstack.frames(0).script_name(), "/background.js");
    EXPECT_EQ(callstack.frames(0).function_name(), "getCookies");

    // Verify enterprise telemetry reporting.
    ASSERT_TRUE(IsTelemetryServiceEnabledForEnterprise());
    std::unique_ptr<TelemetryReport> enterprise_telemetry_report_pb =
        GetTelemetryReportForEnterprise();
    const auto& enterprise_extension_report =
        enterprise_telemetry_report_pb->reports(0);

    EXPECT_THAT(extension_report, EqualsProto(enterprise_extension_report));
    EXPECT_TRUE(IsEnterpriseExtensionStoreEmpty());
}

IN_PROC_BROWSER_TEST_F(ExtensionTelemetryServiceBrowserTest,
                       DetectsAndReportsDeclarativeNetRequestSignal) {
  SetSafeBrowsingState(browser()->GetProfile()->GetPrefs(),
                       SafeBrowsingState::ENHANCED_PROTECTION);
  ASSERT_TRUE(StartEmbeddedTestServer());

  static constexpr char kManifest[] = R"(
    {
      "name": "Simple DeclarativeNetRequest Extension",
      "version": "0.1",
      "manifest_version": 3,
      "background": { "service_worker": "background.js" },
      "permissions": ["declarativeNetRequest"],
      "host_permissions": ["<all_urls>"]
    })";
  static constexpr char kBackground[] = R"(
    const modifyHeadersRule = {
      id: 1,
      priority: 1,
      condition: {urlFilter: 'dynamic', resourceTypes: ['main_frame']},
      action: {
        type: 'modifyHeaders',
        requestHeaders: [{header: 'header1', operation: 'set', value: 'value1'}]
      },
    };
    const redirectRule = {
      id: 2,
      priority: 1,
      condition: {'urlFilter': 'filter' },
      action: {type: 'redirect', redirect: {'url' : 'http://google.com' }},
    };
    chrome.test.runTests([
      async function updateDynamicRules() {
        await chrome.declarativeNetRequest.updateDynamicRules(
          {addRules: [modifyHeadersRule]}, () => {
            chrome.test.assertNoLastError();
            chrome.test.succeed();
          });
        },
        async function updateSessionRules() {
          await chrome.declarativeNetRequest.updateSessionRules(
            {addRules: [redirectRule]}, () => {
              chrome.test.assertNoLastError();
              chrome.test.succeed();
            });
          },
    ]);)";

  extensions::TestExtensionDir test_dir;
  test_dir.WriteManifest(kManifest);
  test_dir.WriteFile(FILE_PATH_LITERAL("background.js"), kBackground);

  extensions::ResultCatcher result_catcher;
  const auto* extension = LoadExtension(test_dir.UnpackedPath());
  ASSERT_TRUE(extension);
  ASSERT_TRUE(result_catcher.GetNextResult());

  ASSERT_NE(telemetry_service(), nullptr);
  ASSERT_TRUE(IsTelemetryServiceEnabledForESB());
  {
    // Verify the contents of telemetry report generated.
    std::unique_ptr<TelemetryReport> telemetry_report_pb = GetTelemetryReport();
    ASSERT_NE(telemetry_report_pb, nullptr);
    // Retrieve the report corresponding to the test extension.
    int report_index = -1;
    for (int i = 0; i < telemetry_report_pb->reports_size(); i++) {
      if (telemetry_report_pb->reports(i).extension().id() == extension->id()) {
        report_index = i;
      }
    }
    ASSERT_NE(report_index, -1);

    const auto& extension_report = telemetry_report_pb->reports(report_index);
    // Verify signal proto from the reports.
    const ExtensionTelemetryReportRequest_SignalInfo& signal =
        extension_report.signals()[0];
    const DeclarativeNetRequestInfo& dnr_info =
        signal.declarative_net_request_info();
    EXPECT_EQ(dnr_info.rules_size(), 2);
    EXPECT_EQ(dnr_info.max_exceeded_rules_count(), static_cast<uint32_t>(0));

    // Verify Redirect rule.
    TestRule expected_redirect_rule =
        extensions::declarative_net_request::CreateGenericRule();
    expected_redirect_rule.id = 2;
    expected_redirect_rule.priority = 1;
    expected_redirect_rule.action->type = "redirect";
    expected_redirect_rule.action->redirect.emplace();
    expected_redirect_rule.action->redirect->url = "http://google.com";
    expected_redirect_rule.condition->url_filter = "filter";

    EXPECT_EQ(dnr_info.rules(0),
              expected_redirect_rule.ToValue().DebugString());

    // Verify ModifyHeader rule.
    TestRule expected_mh_rule =
        extensions::declarative_net_request::CreateGenericRule();
    expected_mh_rule.id = 1;
    expected_mh_rule.priority = 1;
    expected_mh_rule.action->type = "modifyHeaders";
    expected_mh_rule.action->request_headers.emplace();
    expected_mh_rule.action->request_headers = std::vector<TestHeaderInfo>(
        {TestHeaderInfo("header1", "set", "value1")});
    expected_mh_rule.condition->url_filter = "dynamic";
    expected_mh_rule.condition->resource_types =
        std::vector<std::string>({"main_frame"});

    EXPECT_EQ(dnr_info.rules(1), expected_mh_rule.ToValue().DebugString());
  }
}

IN_PROC_BROWSER_TEST_F(ExtensionTelemetryServiceBrowserTest,
                       DetectsAndReportsDeclarativeNetRequestActionSignal) {
  UseHttpsTestServer();
  SetSafeBrowsingState(browser()->GetProfile()->GetPrefs(),
                       SafeBrowsingState::ENHANCED_PROTECTION);
  ASSERT_TRUE(StartEmbeddedTestServer());

  static constexpr char kManifest[] = R"(
    {
      "name": "DeclarativeNetRequest Action Extension",
      "version": "0.1",
      "manifest_version": 3,
      "background": { "service_worker": "background.js" },
      "permissions": ["declarativeNetRequest"],
      "host_permissions": ["<all_urls>"]
    })";
  static constexpr char kBackground[] = R"(
    const redirectRule = {
      id: 1,
      priority: 1,
      condition: {urlFilter: 'example.com', resourceTypes: ['main_frame'] },
      action: {type: 'redirect', redirect: {'url' : 'http://google.com/pages/index.html' }},
    };
    chrome.test.runTests([
      async function updateSessionRules() {
        await chrome.declarativeNetRequest.updateSessionRules(
          {addRules: [redirectRule]}, () => {
            chrome.test.assertNoLastError();
            chrome.test.sendMessage('done');
            chrome.test.succeed();
          });
        },
    ]);)";

  extensions::TestExtensionDir test_dir;
  test_dir.WriteManifest(kManifest);
  test_dir.WriteFile(FILE_PATH_LITERAL("background.js"), kBackground);

  ExtensionTestMessageListener listener("done");
  extensions::ResultCatcher result_catcher;
  const auto* extension = LoadExtension(test_dir.UnpackedPath());
  ASSERT_TRUE(extension);
  ASSERT_TRUE(result_catcher.GetNextResult());
  ASSERT_TRUE(listener.WaitUntilSatisfied());

  // Load a url on "example.com". It should be redirected.
  GURL url = embedded_test_server()->GetURL("example.com", "/pages/page.html");

  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), url));
  GURL final_url = web_contents(browser())->GetLastCommittedURL();
  EXPECT_EQ(GURL("http://google.com/pages/index.html"), final_url);

  ASSERT_NE(telemetry_service(), nullptr);
  ASSERT_TRUE(IsTelemetryServiceEnabledForESB());

  // Verify the contents of telemetry report generated.
  std::unique_ptr<TelemetryReport> telemetry_report_pb = GetTelemetryReport();
  ASSERT_NE(telemetry_report_pb, nullptr);
  // Retrieve the report corresponding to the test extension.
  int report_index = -1;
  for (int i = 0; i < telemetry_report_pb->reports_size(); i++) {
    if (telemetry_report_pb->reports(i).extension().id() == extension->id()) {
      report_index = i;
    }
  }
  ASSERT_NE(report_index, -1);

  const auto& extension_report = telemetry_report_pb->reports(report_index);
  EXPECT_EQ(extension_report.extension().id(), extension->id());
  EXPECT_EQ(extension_report.extension().name(),
            "DeclarativeNetRequest Action Extension");
  EXPECT_EQ(extension_report.extension().version(), "0.1");
  // Verify the designated test extension's report has signal data. Telemetry
  // report has 2 signals: declarativeNetRequest and declarativeNetRequest
  // action.
  ASSERT_EQ(extension_report.signals().size(), 2);
  // Verify that extension store has been cleared after creating a telemetry
  // report.
  EXPECT_TRUE(IsExtensionStoreEmpty());

  // Verify the first signal proto from the report is the declarativeNetRequest
  // signal, which was created when the updateSessionRules method was invoked to
  // configure the redirect rule.
  const ExtensionTelemetryReportRequest_SignalInfo& dnr_signal =
      extension_report.signals()[0];
  const DeclarativeNetRequestInfo& dnr_info =
      dnr_signal.declarative_net_request_info();
  EXPECT_EQ(dnr_info.rules_size(), 1);

  // Verify the second signal proto from the report is the
  // declarativeNetRequest action signal, which was created when the web request
  // was redirected as a result of the request URL matching that in the
  // declarativeNetRequest action.
  const ExtensionTelemetryReportRequest_SignalInfo& dnr_action_signal =
      extension_report.signals()[1];
  const DeclarativeNetRequestActionInfo& dnr_action_info =
      dnr_action_signal.declarative_net_request_action_info();
  EXPECT_EQ(dnr_action_info.action_details_size(), 1);

  // Verify redirect action info.
  const DeclarativeNetRequestActionInfo::ActionDetails& action_detail =
      dnr_action_info.action_details(0);
  EXPECT_EQ(action_detail.count(), 1u);
  EXPECT_EQ(action_detail.type(), DeclarativeNetRequestActionInfo::REDIRECT);
  GURL request_url = embedded_test_server()->GetURL("example.com", "/pages/");
  EXPECT_EQ(action_detail.request_url(), request_url);
  EXPECT_EQ(action_detail.redirect_url(), "http://google.com/pages/");
}

IN_PROC_BROWSER_TEST_F(ExtensionTelemetryServiceBrowserTest,
                       DetectsAndReportsTabsApiSignal) {
  SetSafeBrowsingState(browser()->GetProfile()->GetPrefs(),
                       SafeBrowsingState::ENHANCED_PROTECTION);
  ASSERT_TRUE(StartEmbeddedTestServer());

  GURL google_url = embedded_test_server()->GetURL("google.com", "/empty.html");
  GURL example_url =
      embedded_test_server()->GetURL("example.com", "/empty.html");
  std::string sanitized_google_url = google_url.GetWithoutFilename().spec();
  std::string sanitized_example_url = example_url.GetWithoutFilename().spec();

  static constexpr char kManifest[] =
      R"({
         "name": "Tabs API Extension",
         "version": "0.1",
         "manifest_version": 3,
         "permissions":["tabs"],
         "host_permissions": ["<all_urls>"],
         "background": { "service_worker" : "background.js" }
       })";
  static constexpr char kBackgroundTemplate[] =
      R"(
        const loadedTabs = new Map();
        const waitingResolvers = new Map();

        chrome.tabs.onUpdated.addListener((tabId, changeInfo, tab) => {
          if (changeInfo.status === 'complete' && tab.url) {
            loadedTabs.set(tabId, tab.url);
            if (waitingResolvers.has(tabId)) {
              const {expectedUrl, resolve} = waitingResolvers.get(tabId);
              if (!expectedUrl || tab.url.startsWith(expectedUrl)) {
                waitingResolvers.delete(tabId);
                resolve(tab);
              }
            }
          } else if (changeInfo.status === 'loading') {
            loadedTabs.delete(tabId);
          }
        });

        function waitForTabLoad(tabId, expectedUrl) {
          return new Promise((resolve) => {
            if (loadedTabs.has(tabId) &&
                (!expectedUrl || loadedTabs.get(tabId).startsWith(expectedUrl))) {
              resolve();
              return;
            }
            waitingResolvers.set(tabId, {expectedUrl, resolve});
          });
        }

        const GOOGLE_URL = '$1';
        const EXAMPLE_URL = '$2';

        chrome.test.runTests([
          async function tabOps() {
            const first_tab =
                await chrome.tabs.create({url: GOOGLE_URL});
            await waitForTabLoad(first_tab.id, GOOGLE_URL);

            const second_tab =
                await chrome.tabs.create({url: GOOGLE_URL});
            await waitForTabLoad(second_tab.id, GOOGLE_URL);

            await chrome.tabs.update(second_tab.id, {url: EXAMPLE_URL});
            await waitForTabLoad(second_tab.id, EXAMPLE_URL);

            await chrome.tabs.remove(second_tab.id);

            const newWindow =
                await chrome.windows.create({url: GOOGLE_URL});
            await waitForTabLoad(newWindow.tabs[0].id, GOOGLE_URL);
            await chrome.tabs.captureVisibleTab(newWindow.id);
            chrome.test.succeed();
          },
        ]);
      )";

  std::string background = base::ReplaceStringPlaceholders(
      kBackgroundTemplate, {google_url.spec(), example_url.spec()}, nullptr);

  extensions::TestExtensionDir test_dir;
  test_dir.WriteManifest(kManifest);
  test_dir.WriteFile(FILE_PATH_LITERAL("background.js"), background);

  extensions::ResultCatcher result_catcher;
  const auto* extension = LoadExtension(test_dir.UnpackedPath());
  ASSERT_TRUE(extension);
  ASSERT_TRUE(result_catcher.GetNextResult());

  ASSERT_NE(telemetry_service(), nullptr);
  ASSERT_TRUE(IsTelemetryServiceEnabledForESB());

  // Verify the contents of telemetry report generated for ESB users.
  std::unique_ptr<TelemetryReport> telemetry_report_pb = GetTelemetryReport();
  ASSERT_NE(telemetry_report_pb, nullptr);
  // Retrieve the report corresponding to the test extension.
  int report_index = -1;
  for (int i = 0; i < telemetry_report_pb->reports_size(); i++) {
    if (telemetry_report_pb->reports(i).extension().id() == extension->id()) {
      report_index = i;
    }
  }
  ASSERT_NE(report_index, -1);

  const auto& extension_report = telemetry_report_pb->reports(report_index);
  EXPECT_EQ(extension_report.extension().id(), extension->id());
  EXPECT_EQ(extension_report.extension().name(), "Tabs API Extension");
  EXPECT_EQ(extension_report.extension().version(), "0.1");
  // Verify the designated test extension's report has signal data.
  ASSERT_EQ(extension_report.signals().size(), 1);
  // Verify that extension store has been cleared after creating a telemetry
  // report.
  EXPECT_TRUE(IsExtensionStoreEmpty());

  // Verify signal proto from the reports.
  const ExtensionTelemetryReportRequest_SignalInfo& signal =
      extension_report.signals()[0];

  // Verify the number of unique call details.
  using TabsApiInfo = ExtensionTelemetryReportRequest_SignalInfo_TabsApiInfo;
  const TabsApiInfo& tabs_api_info = signal.tabs_api_info();
  ASSERT_EQ(tabs_api_info.call_details_size(), 4);

  // Verify the contents of each call details.
  {
    const TabsApiInfo::CallDetails& call_details =
        tabs_api_info.call_details(0);
    EXPECT_EQ(call_details.count(), 2u);
    EXPECT_EQ(call_details.method(), TabsApiInfo::CREATE);
    EXPECT_EQ(call_details.current_url(), "");
    EXPECT_EQ(call_details.new_url(), sanitized_google_url);

    // Check the JS call stack information.
    EXPECT_EQ(call_details.js_callstacks_size(), 2);
    const JSCallStack& callstack0 = call_details.js_callstacks(0);
    ASSERT_GE(callstack0.frames_size(), 1);
    EXPECT_EQ(callstack0.frames(0).script_name(), "/background.js");
    EXPECT_EQ(callstack0.frames(0).function_name(), "tabOps");

    const JSCallStack& callstack1 = call_details.js_callstacks(1);
    ASSERT_GE(callstack1.frames_size(), 1);
    EXPECT_EQ(callstack1.frames(0).script_name(), "/background.js");
    EXPECT_EQ(callstack1.frames(0).function_name(), "tabOps");
  }
  {
    const TabsApiInfo::CallDetails& call_details =
        tabs_api_info.call_details(1);
    EXPECT_EQ(call_details.count(), 1u);
    EXPECT_EQ(call_details.method(), TabsApiInfo::UPDATE);
    EXPECT_EQ(call_details.current_url(), sanitized_google_url);
    EXPECT_EQ(call_details.new_url(), sanitized_example_url);

    // Check the JS call stack information.
    EXPECT_EQ(call_details.js_callstacks_size(), 1);
    const JSCallStack& callstack = call_details.js_callstacks(0);
    ASSERT_GE(callstack.frames_size(), 1);
    EXPECT_EQ(callstack.frames(0).script_name(), "/background.js");
    EXPECT_EQ(callstack.frames(0).function_name(), "tabOps");
  }
  {
    const TabsApiInfo::CallDetails& call_details =
        tabs_api_info.call_details(2);
    EXPECT_EQ(call_details.count(), 1u);
    EXPECT_EQ(call_details.method(), TabsApiInfo::REMOVE);
    EXPECT_EQ(call_details.current_url(), sanitized_example_url);
    EXPECT_EQ(call_details.new_url(), "");

    // Check the JS call stack information.
    EXPECT_EQ(call_details.js_callstacks_size(), 1);
    const JSCallStack& callstack = call_details.js_callstacks(0);
    ASSERT_GE(callstack.frames_size(), 1);
    EXPECT_EQ(callstack.frames(0).script_name(), "/background.js");
    EXPECT_EQ(callstack.frames(0).function_name(), "tabOps");
  }
  {
    const TabsApiInfo::CallDetails& call_details =
        tabs_api_info.call_details(3);
    EXPECT_EQ(call_details.count(), 1u);
    EXPECT_EQ(call_details.method(), TabsApiInfo::CAPTURE_VISIBLE_TAB);
    EXPECT_EQ(call_details.current_url(), sanitized_google_url);
    EXPECT_EQ(call_details.new_url(), "");

    // Check the JS call stack information.
    EXPECT_EQ(call_details.js_callstacks_size(), 1);
    const JSCallStack& callstack = call_details.js_callstacks(0);
    ASSERT_GE(callstack.frames_size(), 1);
    EXPECT_EQ(callstack.frames(0).script_name(), "/background.js");
    EXPECT_EQ(callstack.frames(0).function_name(), "tabOps");
  }

  // Verify enterprise telemetry reporting.
  ASSERT_TRUE(IsTelemetryServiceEnabledForEnterprise());
  std::unique_ptr<TelemetryReport> enterprise_telemetry_report_pb =
      GetTelemetryReportForEnterprise();
  const auto& enterprise_extension_report =
      enterprise_telemetry_report_pb->reports(0);

  EXPECT_THAT(extension_report, EqualsProto(enterprise_extension_report));
  EXPECT_TRUE(IsEnterpriseExtensionStoreEmpty());
}

IN_PROC_BROWSER_TEST_F(ExtensionTelemetryServiceBrowserTest,
                       InterceptsRemoteHostContactedSignalInRenderer) {
  SetSafeBrowsingState(browser()->GetProfile()->GetPrefs(),
                       SafeBrowsingState::ENHANCED_PROTECTION);
  ASSERT_TRUE(StartEmbeddedTestServer());
  extensions::ResultCatcher result_catcher;
  // Load extension from the test extension directory.
  const auto* extension =
      LoadExtension(test_extension_dir_.AppendASCII("basic_crx"));
  ASSERT_TRUE(extension);
  ASSERT_TRUE(result_catcher.GetNextResult());

  ASSERT_NE(telemetry_service(), nullptr);
  ASSERT_TRUE(IsTelemetryServiceEnabledForESB());
  // Process signal.
  {
    // Verify that the registered extension information is saved in the
    // telemetry service's extension store.
    const ExtensionInfo* info =
        GetExtensionInfoFromExtensionStore(extension->id());
    EXPECT_EQ(extension->name(), kExtensionName);
    EXPECT_EQ(extension->id(), extension->id());
    EXPECT_EQ(info->version(), kExtensionVersion);
  }
  // Generate telemetry report and verify.
  {
    // Verify the contents of telemetry report generated.
    std::unique_ptr<TelemetryReport> telemetry_report_pb = GetTelemetryReport();
    ASSERT_NE(telemetry_report_pb, nullptr);
    auto extension_report = std::ranges::find_if(
        telemetry_report_pb->reports(), [&](const auto& report) {
          return report.extension().id() == extension->id();
        });
    EXPECT_EQ(extension_report->extension().id(), extension->id());
    EXPECT_EQ(extension_report->extension().name(), kExtensionName);
    EXPECT_EQ(extension_report->extension().version(), kExtensionVersion);
    // Verify the designated test extension's report has signal data.
    ASSERT_EQ(extension_report->signals().size(), 1);
    // Verify that extension store has been cleared after creating a telemetry
    // report.
    EXPECT_TRUE(IsExtensionStoreEmpty());

    // Verify signal proto from the reports.
    const ExtensionTelemetryReportRequest_SignalInfo& signal =
        extension_report->signals()[0];
    const RemoteHostContactedInfo& remote_host_contacted_info =
        signal.remote_host_contacted_info();
    ASSERT_EQ(remote_host_contacted_info.remote_host_size(), 2);
    EXPECT_TRUE(remote_host_contacted_info.collected_from_new_interception());

    const RemoteHostInfo& remote_host_info =
        remote_host_contacted_info.remote_host(0);
    EXPECT_EQ(remote_host_info.contact_count(), 1u);
    EXPECT_EQ(remote_host_info.url(), kExtensionContactedHost);
    EXPECT_EQ(remote_host_info.connection_protocol(),
              RemoteHostInfo::HTTP_HTTPS);
    EXPECT_EQ(remote_host_info.contacted_by(), RemoteHostInfo::EXTENSION);
    const RemoteHostInfo& remote_host_contacted_info_websocket =
        remote_host_contacted_info.remote_host(1);
    EXPECT_EQ(remote_host_contacted_info_websocket.contact_count(), 1u);
    EXPECT_EQ(remote_host_contacted_info_websocket.url(),
              kExtensionContactedHost);
    EXPECT_EQ(remote_host_contacted_info_websocket.connection_protocol(),
              RemoteHostInfo::WEBSOCKET);
    EXPECT_EQ(remote_host_contacted_info_websocket.contacted_by(),
              RemoteHostInfo::EXTENSION);
  }
}

IN_PROC_BROWSER_TEST_F(ExtensionTelemetryServiceBrowserTest,
                       DetectsWebRequestFromContentScript) {
  SetSafeBrowsingState(browser()->GetProfile()->GetPrefs(),
                       SafeBrowsingState::ENHANCED_PROTECTION);
  ASSERT_TRUE(StartEmbeddedTestServer());

  static constexpr char kManifest[] =
      R"({
        "name": "Content Script Extension",
        "manifest_version": 3,
        "version": "0.0.1",
        "permissions": ["scripting"],
        "host_permissions": ["*://example.com/*"],
        "content_scripts": [
          {
            "matches": ["<all_urls>"],
            "js": ["content_script.js"],
            "run_at": "document_start",
            "all_frames": true
          }
        ]
      })";

  static constexpr char kContentScript[] =
      R"(
        chrome.test.getConfig(function(config) {
          var baseUrl = 'http://example.com:' + config.testServer.port;
          chrome.test.runTests([async function makeRequest() {
            let url = `${baseUrl}/extensions/test_file.txt`;
            let response = await fetch(url);
            let text = await response.text();
            chrome.test.assertEq('Hello!', text);
            const baseURLForWebSocket = 'ws://example.com:' + config.testServer.port;
            let socket = new WebSocket(baseURLForWebSocket);
            socket.close();
            chrome.test.sendMessage('done');
            chrome.test.succeed();
          }]);
        });
      )";

  extensions::TestExtensionDir test_dir;
  test_dir.WriteManifest(kManifest);
  test_dir.WriteFile(FILE_PATH_LITERAL("content_script.js"), kContentScript);
  const auto* extension = LoadExtension(test_dir.UnpackedPath());
  ASSERT_TRUE(extension);

  // Navigate to URL, trigger content script and wait.
  ExtensionTestMessageListener listener("done");
  GURL url = embedded_test_server()->GetURL("example.com",
                                            "/extensions/test_file.txt");
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), url));
  EXPECT_TRUE(listener.WaitUntilSatisfied());

  // Successfully retrieve the extension telemetry instance.
  ASSERT_NE(telemetry_service(), nullptr);
  ASSERT_TRUE(IsTelemetryServiceEnabledForESB());

  // Generate telemetry report and verify.
  {
    // Verify the contents of telemetry report generated.
    std::unique_ptr<TelemetryReport> telemetry_report_pb = GetTelemetryReport();
    ASSERT_NE(telemetry_report_pb, nullptr);
    auto extension_report = std::ranges::find_if(
        telemetry_report_pb->reports(), [&](const auto& report) {
          return report.extension().id() == extension->id();
        });
    EXPECT_EQ(extension_report->extension().id(), extension->id());
    EXPECT_EQ(extension_report->extension().name(), "Content Script Extension");
    EXPECT_EQ(extension_report->extension().version(), kExtensionVersion);
    // Verify the designated test extension's report has signal data.
    ASSERT_EQ(extension_report->signals().size(), 1);
    // Verify that extension store has been cleared after creating a telemetry
    // report.
    EXPECT_TRUE(IsExtensionStoreEmpty());

    // Verify signal proto from the reports.
    const ExtensionTelemetryReportRequest_SignalInfo& signal =
        extension_report->signals()[0];
    const RemoteHostContactedInfo& remote_host_contacted_info =
        signal.remote_host_contacted_info();
    ASSERT_EQ(remote_host_contacted_info.remote_host_size(), 2);
    EXPECT_TRUE(remote_host_contacted_info.collected_from_new_interception());

    const RemoteHostInfo& remote_host_info =
        remote_host_contacted_info.remote_host(0);
    EXPECT_EQ(remote_host_info.contact_count(), 1u);
    EXPECT_EQ(remote_host_info.url(), kExtensionContactedHost);
    EXPECT_EQ(remote_host_info.connection_protocol(),
              RemoteHostInfo::HTTP_HTTPS);
    EXPECT_EQ(remote_host_info.contacted_by(), RemoteHostInfo::CONTENT_SCRIPT);

    const RemoteHostInfo& remote_host_contacted_info_websocket =
        remote_host_contacted_info.remote_host(1);
    EXPECT_EQ(remote_host_contacted_info_websocket.contact_count(), 1u);
    EXPECT_EQ(remote_host_contacted_info_websocket.url(),
              kExtensionContactedHost);
    EXPECT_EQ(remote_host_contacted_info_websocket.connection_protocol(),
              RemoteHostInfo::WEBSOCKET);
    EXPECT_EQ(remote_host_contacted_info_websocket.contacted_by(),
              RemoteHostInfo::CONTENT_SCRIPT);
  }
}

IN_PROC_BROWSER_TEST_F(ExtensionTelemetryServiceBrowserTest,
                       DetectsAndReportsSearchHijackingSignal) {
  SetSafeBrowsingState(browser()->GetProfile()->GetPrefs(),
                       SafeBrowsingState::ENHANCED_PROTECTION);
  ASSERT_TRUE(StartEmbeddedTestServer());

  // Load a minimal extension.
  static constexpr char kManifest[] =
      R"({
         "name": "Test Extension",
         "version": "0.1",
         "manifest_version": 3
       })";
  extensions::TestExtensionDir test_dir;
  test_dir.WriteManifest(kManifest);
  const auto* extension = LoadExtension(test_dir.UnpackedPath());
  ASSERT_TRUE(extension);

  ASSERT_NE(telemetry_service(), nullptr);
  ASSERT_TRUE(IsTelemetryServiceEnabledForESB());

  // Set up DSE.
  TemplateURLService* template_url_service =
      TemplateURLServiceFactory::GetForProfile(browser()->GetProfile());
  ASSERT_TRUE(template_url_service);
  search_test_utils::WaitForTemplateURLServiceToLoad(template_url_service);
  TemplateURLData data;
  data.SetShortName(u"Test");
  data.SetKeyword(u"test");
  data.SetURL("http://test.com/search?q={searchTerms}");
  TemplateURL* template_url =
      template_url_service->Add(std::make_unique<TemplateURL>(data));
  template_url_service->SetUserSelectedDefaultSearchProvider(template_url);

  // Configure the search hijacking detector for testing.
  SearchHijackingDetector* search_hijacking_detector =
      telemetry_service()->search_hijacking_detector_for_testing();
  search_hijacking_detector->SetHeuristicCheckInterval(base::Seconds(0));
  search_hijacking_detector->SetHeuristicThreshold(5);

  // Simulate search events.
  AutocompleteMatch match;
  match.destination_url = GURL("http://test.com/search?q=foo");

  for (int i = 0; i < 10; ++i) {
    telemetry_service()->OnOmniboxSearch(match);
  }
  for (int i = 0; i < 3; ++i) {
    telemetry_service()->OnDseSerpLoaded();
  }

  // Manually trigger the heuristic check.
  search_hijacking_detector->MaybeCheckForHeuristicMatch();

  // Generate telemetry report and verify.
  std::unique_ptr<TelemetryReport> telemetry_report_pb = GetTelemetryReport();
  ASSERT_NE(telemetry_report_pb, nullptr);
  ASSERT_TRUE(telemetry_report_pb->has_search_hijacking_signal());
  const auto& signal = telemetry_report_pb->search_hijacking_signal();
  EXPECT_EQ(signal.omnibox_search_count(), 10);
  EXPECT_EQ(signal.serp_landing_count(), 3);
}

IN_PROC_BROWSER_TEST_F(ExtensionTelemetryServiceBrowserTest,
                       DetectsAndReportsDOMAccessSignal) {
  // Enable enterprise policy with DOM activity event.
  enterprise_connectors::test::SetOnSecurityEventReporting(
      /*prefs=*/prefs(),
      /*enabled=*/true,
      /*enabled_event_names=*/{},
      /*enabled_opt_in_events=*/
      {{enterprise_connectors::kExtensionTelemetryEvent, {"*"}},
       {enterprise_connectors::kExtensionDOMActivityEvent, {"*"}}});
  ASSERT_TRUE(StartEmbeddedTestServer());

  static constexpr char kManifest[] =
      R"({
        "name": "DOM Access Extension",
        "version": "0.1",
        "manifest_version": 3,
        "content_scripts": [
          {
            "matches": ["<all_urls>"],
            "js": ["content_script.js"],
            "run_at": "document_start"
          }
        ]
      })";
  static constexpr char kContentScript[] =
      R"(
        console.log(document.cookie);
        chrome.test.notifyPass();
      )";

  extensions::TestExtensionDir test_dir;
  test_dir.WriteManifest(kManifest);
  test_dir.WriteFile(FILE_PATH_LITERAL("content_script.js"), kContentScript);

  extensions::ResultCatcher result_catcher;
  const auto* extension = LoadExtension(test_dir.UnpackedPath());
  ASSERT_TRUE(extension);

  // Navigate to a page to trigger the content script.
  ASSERT_TRUE(ui_test_utils::NavigateToURL(
      browser(), embedded_test_server()->GetURL("example.com", "/empty.html")));
  ASSERT_TRUE(result_catcher.GetNextResult());

  ASSERT_TRUE(IsTelemetryServiceEnabledForEnterprise());

  // Generate telemetry report and verify.
  std::unique_ptr<TelemetryReport> telemetry_report_pb =
      GetTelemetryReportForEnterprise();
  ASSERT_NE(telemetry_report_pb, nullptr);

  // Retrieve the report corresponding to the test extension.
  int report_index = -1;
  for (int i = 0; i < telemetry_report_pb->reports_size(); i++) {
    if (telemetry_report_pb->reports(i).extension().id() == extension->id()) {
      report_index = i;
    }
  }
  ASSERT_NE(report_index, -1);

  const auto& extension_report = telemetry_report_pb->reports(report_index);
  bool found_dom_access_signal = false;
  for (const auto& signal : extension_report.signals()) {
    if (signal.has_dom_access_info()) {
      const auto& dom_access_info = signal.dom_access_info();
      for (const auto& dom_access : dom_access_info.dom_accesses()) {
        if (dom_access.api_name() == "Document.cookie") {
          found_dom_access_signal = true;
          break;
        }
      }
    }
  }
  EXPECT_TRUE(found_dom_access_signal);
}

IN_PROC_BROWSER_TEST_F(ExtensionTelemetryServiceBrowserTest,
                       DetectsAndReportsScriptInjectionSignal) {
  // Enable enterprise policy with DOM activity event.
  enterprise_connectors::test::SetOnSecurityEventReporting(
      /*prefs=*/prefs(),
      /*enabled=*/true,
      /*enabled_event_names=*/{},
      /*enabled_opt_in_events=*/
      {{enterprise_connectors::kExtensionTelemetryEvent, {"*"}},
       {enterprise_connectors::kExtensionDOMActivityEvent, {"*"}}});
  ASSERT_TRUE(StartEmbeddedTestServer());

  static constexpr char kManifest[] =
      R"({
        "name": "Script Injection Extension",
        "version": "0.1",
        "manifest_version": 3,
        "permissions": ["scripting"],
        "host_permissions": ["<all_urls>"],
        "background": { "service_worker": "background.js" }
      })";
  static constexpr char kBackground[] =
      R"(
        chrome.test.runTests([
          async function injectScript() {
            const tabs = await chrome.tabs.query({active: true, currentWindow: true});
            await chrome.scripting.executeScript({
              target: {tabId: tabs[0].id},
              func: () => { console.log('injected'); }
            });
            chrome.test.succeed();
          }
        ]);
      )";

  extensions::TestExtensionDir test_dir;
  test_dir.WriteManifest(kManifest);
  test_dir.WriteFile(FILE_PATH_LITERAL("background.js"), kBackground);

  // Navigate to a page first so there's a target for injection.
  ASSERT_TRUE(ui_test_utils::NavigateToURL(
      browser(), embedded_test_server()->GetURL("example.com", "/empty.html")));

  extensions::ResultCatcher result_catcher;
  const auto* extension = LoadExtension(test_dir.UnpackedPath());
  ASSERT_TRUE(extension);
  ASSERT_TRUE(result_catcher.GetNextResult());

  ASSERT_TRUE(IsTelemetryServiceEnabledForEnterprise());

  // Generate telemetry report and verify.
  std::unique_ptr<TelemetryReport> telemetry_report_pb =
      GetTelemetryReportForEnterprise();
  ASSERT_NE(telemetry_report_pb, nullptr);

  int report_index = -1;
  for (int i = 0; i < telemetry_report_pb->reports_size(); i++) {
    if (telemetry_report_pb->reports(i).extension().id() == extension->id()) {
      report_index = i;
    }
  }
  ASSERT_NE(report_index, -1);

  const auto& extension_report = telemetry_report_pb->reports(report_index);
  bool found_script_injection_signal = false;
  for (const auto& signal : extension_report.signals()) {
    if (signal.has_script_injection_info()) {
      found_script_injection_signal = true;
      break;
    }
  }
  EXPECT_TRUE(found_script_injection_signal);
}

// Enables Milestone 2 so that `GetForSignal()` checks the target's profile
// type, but without Isolated Mode replacing Incognito.
class ExtensionTelemetryServiceIncognitoBrowserTest
    : public ExtensionTelemetryServiceBrowserTest {
 public:
  ExtensionTelemetryServiceIncognitoBrowserTest() {
    milestone2_feature_list_.InitAndEnableFeature(
        enterprise_isolated_mode::kEnterpriseIsolatedModeMilestone2);
  }

 private:
  base::test::ScopedFeatureList milestone2_feature_list_;
};

IN_PROC_BROWSER_TEST_F(ExtensionTelemetryServiceIncognitoBrowserTest,
                       GetForSignalKeepsIncognitoSignalsInCaller) {
  Profile* incognito_profile = CreateIncognitoBrowser()->GetProfile();
  ASSERT_TRUE(incognito_profile->IsIncognitoProfile());
  EXPECT_EQ(ExtensionTelemetryService::GetForSignal(browser()->GetProfile(),
                                                    incognito_profile),
            telemetry_service());
}

IN_PROC_BROWSER_TEST_F(ExtensionTelemetryServiceIncognitoBrowserTest,
                       DoesNotCreateServiceForIncognitoProfile) {
  ASSERT_NE(telemetry_service(), nullptr);

  Profile* incognito_profile = CreateIncognitoBrowser()->GetProfile();

  ASSERT_TRUE(incognito_profile->IsIncognitoProfile());
  EXPECT_EQ(ExtensionTelemetryServiceFactory::GetForProfile(incognito_profile),
            nullptr);
}

IN_PROC_BROWSER_TEST_F(
    ExtensionTelemetryServiceIncognitoBrowserTest,
    ReportsSpanningTabsSignalOnIncognitoTabToRegularService) {
  SetSafeBrowsingState(prefs(), SafeBrowsingState::ENHANCED_PROTECTION);
  ASSERT_TRUE(StartEmbeddedTestServer());
  const extensions::ExtensionId extension_id = LoadTestExtension("spanning");
  BrowserWindowInterface* incognito_browser = CreateIncognitoBrowser();
  ASSERT_TRUE(incognito_browser->GetProfile()->IsIncognitoProfile());

  UpdateTabFromExtension(extension_id, ActiveTabId(incognito_browser),
                         embedded_test_server()->GetURL("/title1.html"));

  EXPECT_TRUE(HasEnterpriseSignals(*telemetry_service(), extension_id));
  EXPECT_TRUE(HasEsbSignals(*telemetry_service(), extension_id));
}

IN_PROC_BROWSER_TEST_F(
    ExtensionTelemetryServiceIncognitoBrowserTest,
    ReportsSpanningCookiesSignalForIncognitoStoreToRegularService) {
  SetSafeBrowsingState(prefs(), SafeBrowsingState::ENHANCED_PROTECTION);
  const extensions::ExtensionId extension_id = LoadTestExtension("spanning");
  ASSERT_TRUE(CreateIncognitoBrowser()->GetProfile()->IsIncognitoProfile());

  GetCookiesFromExtension(extension_id, "getAll", /*store_id=*/"1");

  EXPECT_TRUE(HasEnterpriseSignals(*telemetry_service(), extension_id));
  EXPECT_TRUE(HasEsbSignals(*telemetry_service(), extension_id));
}

class ExtensionTelemetryServiceIsolatedModeBrowserTest
    : public ExtensionTelemetryServiceBrowserTest,
      public testing::WithParamInterface<bool> {
 public:
  ExtensionTelemetryServiceIsolatedModeBrowserTest() {
    isolated_mode_feature_list_.InitWithFeatureState(
        enterprise_isolated_mode::kEnterpriseIsolatedModeMilestone2,
        IsIsolatedModeTelemetryEnabled());
  }

  bool IsIsolatedModeTelemetryEnabled() const { return GetParam(); }

 protected:
  // Opens an Isolated Mode window and returns it.
  BrowserWindowInterface* OpenIsolatedWindow() {
    BrowserWindowInterface* isolated_browser = CreateIncognitoBrowser();
    CHECK(isolated_browser->GetProfile()->IsEnterpriseIsolatedModeProfile());
    return isolated_browser;
  }

  // Returns the Isolated Mode profile opened by `OpenIsolatedWindow()`.
  Profile* isolated_profile() {
    return browser()->GetProfile()->GetPrimaryOTRProfile(
        /*create_if_needed=*/false);
  }

  // Returns the Isolated Mode profile's service. It only exists when
  // Isolated Mode profiles have their own telemetry service.
  ExtensionTelemetryService* isolated_service() {
    return ExtensionTelemetryServiceFactory::GetForProfile(isolated_profile());
  }

  // Returns the service that reports what an extension's instance in the
  // regular profile does to Isolated Mode tabs and cookie stores. When
  // Isolated Mode profiles have their own telemetry service, it is that
  // service. Otherwise, such signals resolve from the caller's profile, so it
  // is the regular profile's service.
  ExtensionTelemetryService* ServiceForIsolatedActivity() {
    return IsIsolatedModeTelemetryEnabled() ? isolated_service()
                                            : telemetry_service();
  }

  // Returns whether the Isolated Mode profile has a service that holds
  // enterprise signals from `extension_id`.
  bool IsolatedServiceHasEnterpriseSignals(
      const extensions::ExtensionId& extension_id) {
    ExtensionTelemetryService* service = isolated_service();
    return service && HasEnterpriseSignals(*service, extension_id);
  }

  // Makes the profiles created while the returned subscription is alive get no
  // `ExtensionTelemetryEventRouter`. A null testing factory makes the service
  // null.
  [[nodiscard]] static base::CallbackListSubscription
  DisableEventRouterForNewProfiles() {
    return BrowserContextDependencyManager::GetInstance()
        ->RegisterCreateServicesCallbackForTesting(
            base::BindRepeating([](content::BrowserContext* context) {
              enterprise_connectors::ExtensionTelemetryEventRouterFactory::
                  GetInstance()
                      ->SetTestingFactory(
                          context,
                          BrowserContextKeyedServiceFactory::TestingFactory());
            }));
  }

 private:
  base::test::ScopedFeatureList isolated_mode_feature_list_;
  enterprise_isolated_mode::ScopedIsolatedModeOverrideForTesting
      scoped_isolated_mode_;
};

IN_PROC_BROWSER_TEST_P(ExtensionTelemetryServiceIsolatedModeBrowserTest,
                       CreatesServiceForIsolatedProfile) {
  SetSafeBrowsingState(prefs(), SafeBrowsingState::ENHANCED_PROTECTION);
  OpenIsolatedWindow();

  if (!IsIsolatedModeTelemetryEnabled()) {
    EXPECT_EQ(isolated_service(), nullptr);
    return;
  }
  ASSERT_NE(isolated_service(), nullptr);
  EXPECT_NE(isolated_service(), telemetry_service());
}

IN_PROC_BROWSER_TEST_P(ExtensionTelemetryServiceIsolatedModeBrowserTest,
                       IsolatedServiceOnlyReportsToEnterprise) {
  SetSafeBrowsingState(prefs(), SafeBrowsingState::ENHANCED_PROTECTION);
  OpenIsolatedWindow();

  if (!IsIsolatedModeTelemetryEnabled()) {
    EXPECT_EQ(isolated_service(), nullptr);
    return;
  }
  ASSERT_NE(isolated_service(), nullptr);
  // The regular profile has ESB enabled and, from `SetUpOnMainThread()`, the
  // reporting policy. The Isolated Mode profile inherits both through its
  // prefs, but its service only reports to the enterprise pipeline.
  EXPECT_FALSE(IsEnabledForESB(*isolated_service()));
  EXPECT_TRUE(IsTelemetryServiceEnabledForEnterprise(*isolated_service()));
}

IN_PROC_BROWSER_TEST_P(ExtensionTelemetryServiceIsolatedModeBrowserTest,
                       GetForSignal) {
  Profile* regular_profile = browser()->GetProfile();
  OpenIsolatedWindow();
  ExtensionTelemetryService* regular_service = telemetry_service();

  // Signals not involving the Isolated Mode profile are unchanged.
  EXPECT_EQ(
      ExtensionTelemetryService::GetForSignal(regular_profile, regular_profile),
      regular_service);
  EXPECT_EQ(ExtensionTelemetryService::GetForSignal(regular_profile,
                                                    /*target=*/nullptr),
            regular_service);

  if (!IsIsolatedModeTelemetryEnabled()) {
    // Signals resolve from the caller, as before.
    EXPECT_EQ(ExtensionTelemetryService::GetForSignal(regular_profile,
                                                      isolated_profile()),
              regular_service);
    EXPECT_EQ(ExtensionTelemetryService::GetForSignal(isolated_profile(),
                                                      /*target=*/nullptr),
              nullptr);
    return;
  }

  ASSERT_NE(isolated_service(), nullptr);
  EXPECT_EQ(ExtensionTelemetryService::GetForSignal(regular_profile,
                                                    isolated_profile()),
            isolated_service());
  EXPECT_EQ(ExtensionTelemetryService::GetForSignal(isolated_profile(),
                                                    /*target=*/nullptr),
            isolated_service());
}

IN_PROC_BROWSER_TEST_P(ExtensionTelemetryServiceIsolatedModeBrowserTest,
                       ReportsSpanningTabsSignalOnRegularTab) {
  SetSafeBrowsingState(prefs(), SafeBrowsingState::ENHANCED_PROTECTION);
  ASSERT_TRUE(StartEmbeddedTestServer());
  const extensions::ExtensionId extension_id = LoadTestExtension("spanning");
  const int regular_tab_id = ActiveTabId(browser());

  UpdateTabFromExtension(extension_id, regular_tab_id,
                         embedded_test_server()->GetURL("/title1.html"));

  EXPECT_TRUE(HasEnterpriseSignals(*telemetry_service(), extension_id));
  EXPECT_TRUE(HasEsbSignals(*telemetry_service(), extension_id));
}

IN_PROC_BROWSER_TEST_P(ExtensionTelemetryServiceIsolatedModeBrowserTest,
                       RoutesSpanningTabsSignalOnIsolatedTab) {
  SetSafeBrowsingState(prefs(), SafeBrowsingState::ENHANCED_PROTECTION);
  ASSERT_TRUE(StartEmbeddedTestServer());
  const extensions::ExtensionId extension_id = LoadTestExtension("spanning");
  BrowserWindowInterface* isolated_browser = OpenIsolatedWindow();
  const int isolated_tab_id = ActiveTabId(isolated_browser);

  UpdateTabFromExtension(extension_id, isolated_tab_id,
                         embedded_test_server()->GetURL("/title1.html"));

  if (!IsIsolatedModeTelemetryEnabled()) {
    // The signal goes to the caller's service, as before.
    EXPECT_TRUE(HasEnterpriseSignals(*telemetry_service(), extension_id));
    EXPECT_TRUE(HasEsbSignals(*telemetry_service(), extension_id));
    return;
  }
  ASSERT_NE(isolated_service(), nullptr);
  EXPECT_TRUE(HasEnterpriseSignals(*isolated_service(), extension_id));
  EXPECT_FALSE(HasEnterpriseSignals(*telemetry_service(), extension_id));
  EXPECT_FALSE(HasEsbSignals(*telemetry_service(), extension_id));
}

IN_PROC_BROWSER_TEST_P(ExtensionTelemetryServiceIsolatedModeBrowserTest,
                       ReportsSpanningCookiesSignalForRegularStore) {
  SetSafeBrowsingState(prefs(), SafeBrowsingState::ENHANCED_PROTECTION);
  const extensions::ExtensionId extension_id = LoadTestExtension("spanning");

  GetCookiesFromExtension(extension_id, "getAll", /*store_id=*/"0");

  EXPECT_TRUE(HasEnterpriseSignals(*telemetry_service(), extension_id));
  EXPECT_TRUE(HasEsbSignals(*telemetry_service(), extension_id));
}

IN_PROC_BROWSER_TEST_P(ExtensionTelemetryServiceIsolatedModeBrowserTest,
                       RoutesSpanningCookiesSignalForIsolatedStore) {
  SetSafeBrowsingState(prefs(), SafeBrowsingState::ENHANCED_PROTECTION);
  const extensions::ExtensionId extension_id = LoadTestExtension("spanning");
  OpenIsolatedWindow();

  GetCookiesFromExtension(extension_id, "getAll", /*store_id=*/"1");

  if (!IsIsolatedModeTelemetryEnabled()) {
    // The signal goes to the caller's service, as before.
    EXPECT_TRUE(HasEnterpriseSignals(*telemetry_service(), extension_id));
    EXPECT_TRUE(HasEsbSignals(*telemetry_service(), extension_id));
    return;
  }
  ASSERT_NE(isolated_service(), nullptr);
  EXPECT_TRUE(HasEnterpriseSignals(*isolated_service(), extension_id));
  EXPECT_FALSE(HasEnterpriseSignals(*telemetry_service(), extension_id));
  EXPECT_FALSE(HasEsbSignals(*telemetry_service(), extension_id));
}

IN_PROC_BROWSER_TEST_P(ExtensionTelemetryServiceIsolatedModeBrowserTest,
                       ReportsSpanningCookiesGetSignalForRegularStore) {
  SetSafeBrowsingState(prefs(), SafeBrowsingState::ENHANCED_PROTECTION);
  const extensions::ExtensionId extension_id = LoadTestExtension("spanning");

  GetCookiesFromExtension(extension_id, "get", /*store_id=*/"0");

  EXPECT_TRUE(HasEnterpriseSignals(*telemetry_service(), extension_id));
  EXPECT_TRUE(HasEsbSignals(*telemetry_service(), extension_id));
}

IN_PROC_BROWSER_TEST_P(ExtensionTelemetryServiceIsolatedModeBrowserTest,
                       RoutesSpanningCookiesGetSignalForIsolatedStore) {
  SetSafeBrowsingState(prefs(), SafeBrowsingState::ENHANCED_PROTECTION);
  const extensions::ExtensionId extension_id = LoadTestExtension("spanning");
  OpenIsolatedWindow();

  GetCookiesFromExtension(extension_id, "get", /*store_id=*/"1");

  if (!IsIsolatedModeTelemetryEnabled()) {
    // The signal goes to the caller's service, as before.
    EXPECT_TRUE(HasEnterpriseSignals(*telemetry_service(), extension_id));
    EXPECT_TRUE(HasEsbSignals(*telemetry_service(), extension_id));
    return;
  }
  ASSERT_NE(isolated_service(), nullptr);
  EXPECT_TRUE(HasEnterpriseSignals(*isolated_service(), extension_id));
  EXPECT_FALSE(HasEnterpriseSignals(*telemetry_service(), extension_id));
  EXPECT_FALSE(HasEsbSignals(*telemetry_service(), extension_id));
}

IN_PROC_BROWSER_TEST_P(ExtensionTelemetryServiceIsolatedModeBrowserTest,
                       RoutesIsolatedModeDomActivityToIsolatedService) {
  enterprise_connectors::test::SetOnSecurityEventReporting(
      /*prefs=*/prefs(),
      /*enabled=*/true,
      /*enabled_event_names=*/{},
      /*enabled_opt_in_events=*/
      {{enterprise_connectors::kExtensionTelemetryEvent, {"*"}},
       {enterprise_connectors::kExtensionDOMActivityEvent, {"*"}}});
  ASSERT_TRUE(StartEmbeddedTestServer());

  static constexpr char kManifest[] =
      R"({
        "name": "DOM Access Extension",
        "version": "0.1",
        "manifest_version": 3,
        "content_scripts": [
          {
            "matches": ["<all_urls>"],
            "js": ["content_script.js"],
            "run_at": "document_start"
          }
        ]
      })";
  static constexpr char kContentScript[] =
      R"(
        console.log(document.cookie);
        chrome.test.notifyPass();
      )";
  extensions::TestExtensionDir test_dir;
  test_dir.WriteManifest(kManifest);
  test_dir.WriteFile(FILE_PATH_LITERAL("content_script.js"), kContentScript);
  const extensions::Extension* extension =
      LoadExtension(test_dir.UnpackedPath(), {.allow_in_incognito = true});
  ASSERT_TRUE(extension);
  const extensions::ExtensionId extension_id = extension->id();

  BrowserWindowInterface* isolated_browser = OpenIsolatedWindow();

  extensions::ResultCatcher result_catcher;
  ASSERT_TRUE(ui_test_utils::NavigateToURL(
      isolated_browser,
      embedded_test_server()->GetURL("example.com", "/empty.html")));
  ASSERT_TRUE(result_catcher.GetNextResult());

  if (!IsIsolatedModeTelemetryEnabled()) {
    // With Milestone 2 off, Isolated Mode activity is handled like other
    // Off-The-Record activity.
    ASSERT_TRUE(base::test::RunUntil([&] {
      return HasEnterpriseSignals(*telemetry_service(), extension_id);
    }));
    return;
  }

  ASSERT_NE(isolated_service(), nullptr);
  ASSERT_TRUE(base::test::RunUntil(
      [&] { return HasEnterpriseSignals(*isolated_service(), extension_id); }));
  EXPECT_FALSE(HasEnterpriseSignals(*telemetry_service(), extension_id));

  // DOM activity in a regular tab is reported by the regular profile's
  // service.
  ASSERT_TRUE(ui_test_utils::NavigateToURL(
      browser(), embedded_test_server()->GetURL("example.com", "/empty.html")));
  ASSERT_TRUE(result_catcher.GetNextResult());
  ASSERT_TRUE(base::test::RunUntil([&] {
    return HasEnterpriseSignals(*telemetry_service(), extension_id);
  }));
}

IN_PROC_BROWSER_TEST_P(ExtensionTelemetryServiceIsolatedModeBrowserTest,
                       DoesNotReportRegularTabSignalToIsolatedService) {
  ASSERT_TRUE(StartEmbeddedTestServer());
  const extensions::ExtensionId extension_id = LoadTestExtension("spanning");
  OpenIsolatedWindow();

  UpdateTabFromExtension(extension_id, ActiveTabId(browser()),
                         embedded_test_server()->GetURL("/title1.html"));

  ASSERT_TRUE(HasEnterpriseSignals(*telemetry_service(), extension_id));
  EXPECT_FALSE(IsolatedServiceHasEnterpriseSignals(extension_id));
}

IN_PROC_BROWSER_TEST_P(ExtensionTelemetryServiceIsolatedModeBrowserTest,
                       RoutesSpanningTabsCreateSignalInIsolatedWindow) {
  ASSERT_TRUE(StartEmbeddedTestServer());
  const extensions::ExtensionId extension_id = LoadTestExtension("spanning");
  BrowserWindowInterface* isolated_browser = OpenIsolatedWindow();

  RunInExtensionBackground(
      browser()->GetProfile(), extension_id,
      content::JsReplace(
          "chrome.tabs.create({windowId: $1, url: $2})",
          extensions::ExtensionTabUtil::GetWindowId(isolated_browser),
          embedded_test_server()->GetURL("/title1.html")));

  ASSERT_NE(ServiceForIsolatedActivity(), nullptr);
  ASSERT_TRUE(
      HasEnterpriseSignals(*ServiceForIsolatedActivity(), extension_id));
  EXPECT_EQ(HasEnterpriseSignals(*telemetry_service(), extension_id),
            !IsIsolatedModeTelemetryEnabled());
}

IN_PROC_BROWSER_TEST_P(ExtensionTelemetryServiceIsolatedModeBrowserTest,
                       DoesNotCreateServiceWithoutEventRouter) {
  ASSERT_NE(telemetry_service(), nullptr);
  base::CallbackListSubscription subscription =
      DisableEventRouterForNewProfiles();

  OpenIsolatedWindow();

  ASSERT_EQ(enterprise_connectors::ExtensionTelemetryEventRouter::Get(
                isolated_profile()),
            nullptr);
  EXPECT_EQ(isolated_service(), nullptr);
  // When Isolated Mode profiles have their own telemetry service, signals
  // targeting an Isolated Mode profile that has none are dropped instead of
  // falling back to the caller's service. Otherwise, they resolve from the
  // caller.
  EXPECT_EQ(ExtensionTelemetryService::GetForSignal(browser()->GetProfile(),
                                                    isolated_profile()),
            IsIsolatedModeTelemetryEnabled() ? nullptr : telemetry_service());
}

IN_PROC_BROWSER_TEST_P(ExtensionTelemetryServiceIsolatedModeBrowserTest,
                       RoutesSplitExtensionTabsSignalToIsolatedService) {
  SetSafeBrowsingState(prefs(), SafeBrowsingState::ENHANCED_PROTECTION);
  ASSERT_TRUE(StartEmbeddedTestServer());
  const extensions::ExtensionId extension_id = LoadTestExtension("split");
  ExtensionTestMessageListener ready_listener("ready");
  BrowserWindowInterface* isolated_browser = OpenIsolatedWindow();
  ASSERT_TRUE(ready_listener.WaitUntilSatisfied());

  RunInExtensionBackground(
      isolated_profile(), extension_id,
      content::JsReplace(
          "chrome.tabs.update($1, {url: $2})", ActiveTabId(isolated_browser),
          embedded_test_server()->GetURL("example.com", "/title1.html")));
  UpdateTabFromExtension(
      extension_id, ActiveTabId(browser()),
      embedded_test_server()->GetURL("google.com", "/title1.html"));

  // The extension's instance in the Isolated Mode profile navigated to
  // example.com. Only that profile's service reports it. When Isolated Mode
  // profiles don't have their own telemetry service, the signal is dropped.
  EXPECT_TRUE(ReportOnlyMentions(GetTelemetryReportForEnterprise().get(),
                                 extension_id, "google.com", "example.com"));
  EXPECT_TRUE(ReportOnlyMentions(GetTelemetryReport().get(), extension_id,
                                 "google.com", "example.com"));
  ASSERT_EQ(IsolatedServiceHasEnterpriseSignals(extension_id),
            IsIsolatedModeTelemetryEnabled());
  if (IsIsolatedModeTelemetryEnabled()) {
    EXPECT_TRUE(ReportOnlyMentions(
        GetTelemetryReportForEnterprise(*isolated_service()).get(),
        extension_id, "example.com", "google.com"));
  }
}

IN_PROC_BROWSER_TEST_P(ExtensionTelemetryServiceIsolatedModeBrowserTest,
                       RoutesSplitExtensionCookiesSignalToIsolatedService) {
  SetSafeBrowsingState(prefs(), SafeBrowsingState::ENHANCED_PROTECTION);
  const extensions::ExtensionId extension_id = LoadTestExtension("split");
  ExtensionTestMessageListener ready_listener("ready");
  OpenIsolatedWindow();
  ASSERT_TRUE(ready_listener.WaitUntilSatisfied());

  RunInExtensionBackground(isolated_profile(), extension_id,
                           "chrome.cookies.getAll({name: 'isolated-cookie'})");
  RunInExtensionBackground(browser()->GetProfile(), extension_id,
                           "chrome.cookies.getAll({name: 'regular-cookie'})");

  // The extension's instance in the Isolated Mode profile read
  // isolated-cookie. Only that profile's service reports it. When Isolated
  // Mode profiles don't have their own telemetry service, the signal is
  // dropped.
  EXPECT_TRUE(ReportOnlyMentions(GetTelemetryReportForEnterprise().get(),
                                 extension_id, "regular-cookie",
                                 "isolated-cookie"));
  EXPECT_TRUE(ReportOnlyMentions(GetTelemetryReport().get(), extension_id,
                                 "regular-cookie", "isolated-cookie"));
  ASSERT_EQ(IsolatedServiceHasEnterpriseSignals(extension_id),
            IsIsolatedModeTelemetryEnabled());
  if (IsIsolatedModeTelemetryEnabled()) {
    EXPECT_TRUE(ReportOnlyMentions(
        GetTelemetryReportForEnterprise(*isolated_service()).get(),
        extension_id, "isolated-cookie", "regular-cookie"));
  }
}

IN_PROC_BROWSER_TEST_P(ExtensionTelemetryServiceIsolatedModeBrowserTest,
                       RoutesContentScriptWebRequestToIsolatedService) {
  SetSafeBrowsingState(prefs(), SafeBrowsingState::ENHANCED_PROTECTION);
  ASSERT_TRUE(StartEmbeddedTestServer());
  const extensions::ExtensionId extension_id =
      LoadTestExtension("spanning", /*with_content_script=*/true);
  BrowserWindowInterface* isolated_browser = OpenIsolatedWindow();

  ASSERT_TRUE(
      NavigateAndWaitForContentScriptFetch(isolated_browser, "example.com"));
  ASSERT_TRUE(NavigateAndWaitForContentScriptFetch(browser(), "google.com"));

  // The Isolated Mode profile's renderer made the request to example.com,
  // which is sent well before the regular tab's request to google.com. Only
  // the Isolated Mode profile's service reports it. When Isolated Mode
  // profiles don't have their own telemetry service, the signal is dropped.
  ASSERT_TRUE(base::test::RunUntil([&] {
    return HasEnterpriseSignals(*telemetry_service(), extension_id) &&
           HasEsbSignals(*telemetry_service(), extension_id) &&
           IsolatedServiceHasEnterpriseSignals(extension_id) ==
               IsIsolatedModeTelemetryEnabled();
  }));
  EXPECT_TRUE(ReportOnlyMentions(GetTelemetryReportForEnterprise().get(),
                                 extension_id, "google.com", "example.com"));
  EXPECT_TRUE(ReportOnlyMentions(GetTelemetryReport().get(), extension_id,
                                 "google.com", "example.com"));
  if (IsIsolatedModeTelemetryEnabled()) {
    EXPECT_TRUE(ReportOnlyMentions(
        GetTelemetryReportForEnterprise(*isolated_service()).get(),
        extension_id, "example.com", "google.com"));
  }
}

IN_PROC_BROWSER_TEST_P(ExtensionTelemetryServiceIsolatedModeBrowserTest,
                       RoutesScriptInjectionOnIsolatedTabToIsolatedService) {
  EnableDomActivityReporting();
  ASSERT_TRUE(StartEmbeddedTestServer());
  const extensions::ExtensionId extension_id = LoadTestExtension("spanning");
  BrowserWindowInterface* isolated_browser = OpenIsolatedWindow();
  ASSERT_TRUE(ui_test_utils::NavigateToURL(
      isolated_browser,
      embedded_test_server()->GetURL("example.com", "/empty.html")));

  RunInExtensionBackground(
      browser()->GetProfile(), extension_id,
      content::JsReplace("chrome.scripting.executeScript("
                         "{target: {tabId: $1}, func: () => {}})",
                         ActiveTabId(isolated_browser)));

  ASSERT_NE(ServiceForIsolatedActivity(), nullptr);
  ASSERT_TRUE(base::test::RunUntil([&] {
    return HasEnterpriseSignals(*ServiceForIsolatedActivity(), extension_id);
  }));
  EXPECT_EQ(HasEnterpriseSignals(*telemetry_service(), extension_id),
            !IsIsolatedModeTelemetryEnabled());
}

IN_PROC_BROWSER_TEST_P(
    ExtensionTelemetryServiceIsolatedModeBrowserTest,
    UploadsIsolatedActivityThroughOwnProfileReportingClient) {
  const extensions::ExtensionId extension_id = LoadTestExtension("spanning");
  OpenIsolatedWindow();
  policy::MockCloudPolicyClient regular_client;
  regular_client.SetDMToken("dm_token");
  policy::MockCloudPolicyClient isolated_client;
  isolated_client.SetDMToken("dm_token");
  base::ScopedClosureRunner reset_regular_client =
      UseCloudPolicyClient(browser()->GetProfile(), &regular_client);
  base::ScopedClosureRunner reset_isolated_client =
      UseCloudPolicyClient(isolated_profile(), &isolated_client);
  // The service that reports the Isolated Mode activity uploads the report
  // through the reporting client of its own profile.
  policy::MockCloudPolicyClient& uploading_client =
      IsIsolatedModeTelemetryEnabled() ? isolated_client : regular_client;
  policy::MockCloudPolicyClient& idle_client =
      IsIsolatedModeTelemetryEnabled() ? regular_client : isolated_client;
  base::test::TestFuture<void> uploaded;
  EXPECT_CALL(uploading_client, UploadSecurityEvent)
      .WillOnce(base::test::InvokeFuture(uploaded));
  EXPECT_CALL(idle_client, UploadSecurityEvent).Times(0);
  GetCookiesFromExtension(extension_id, "getAll", /*store_id=*/"1");
  ASSERT_NE(ServiceForIsolatedActivity(), nullptr);
  ASSERT_TRUE(
      HasEnterpriseSignals(*ServiceForIsolatedActivity(), extension_id));

  SendEnterpriseReport(*ServiceForIsolatedActivity());

  EXPECT_TRUE(uploaded.Wait());
}

IN_PROC_BROWSER_TEST_P(
    ExtensionTelemetryServiceIsolatedModeBrowserTest,
    DoesNotReportIsolatedActivityToEnterpriseWithConnectorOff) {
  SetSafeBrowsingState(prefs(), SafeBrowsingState::ENHANCED_PROTECTION);
  enterprise_connectors::test::SetOnSecurityEventReporting(
      /*prefs=*/prefs(), /*enabled=*/false, /*enabled_event_names=*/{},
      /*enabled_opt_in_events=*/{});
  ASSERT_TRUE(StartEmbeddedTestServer());
  const extensions::ExtensionId extension_id = LoadTestExtension("spanning");
  BrowserWindowInterface* isolated_browser = OpenIsolatedWindow();

  UpdateTabFromExtension(
      extension_id, ActiveTabId(isolated_browser),
      embedded_test_server()->GetURL("example.com", "/title1.html"));
  UpdateTabFromExtension(
      extension_id, ActiveTabId(browser()),
      embedded_test_server()->GetURL("google.com", "/title1.html"));

  // When Isolated Mode profiles have their own telemetry service, the signal
  // about the Isolated Mode tab is dropped. Otherwise, it goes to the caller's
  // service, which only reports to ESB.
  std::unique_ptr<TelemetryReport> esb_report = GetTelemetryReport();
  ASSERT_TRUE(esb_report);
  ASSERT_TRUE(ReportMentions(*esb_report, extension_id, "google.com"));
  EXPECT_EQ(ReportMentions(*esb_report, extension_id, "example.com"),
            !IsIsolatedModeTelemetryEnabled());
  EXPECT_FALSE(HasEnterpriseSignals(*telemetry_service(), extension_id));
  EXPECT_FALSE(IsolatedServiceHasEnterpriseSignals(extension_id));
}

INSTANTIATE_TEST_SUITE_P(,
                         ExtensionTelemetryServiceIsolatedModeBrowserTest,
                         testing::Bool(),
                         [](const testing::TestParamInfo<bool>& info) {
                           return info.param ? "IsolatedModeTelemetryEnabled"
                                             : "IsolatedModeTelemetryDisabled";
                         });

}  // namespace safe_browsing
