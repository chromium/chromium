// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/safe_browsing/extension_telemetry/extension_telemetry_service.h"

#include "base/command_line.h"
#include "base/containers/contains.h"
#include "base/files/file_path.h"
#include "base/path_service.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/protobuf_matchers.h"
#include "base/test/scoped_feature_list.h"
#include "chrome/browser/enterprise/connectors/test/deep_scanning_test_utils.h"
#include "chrome/browser/extensions/extension_apitest.h"
#include "chrome/browser/extensions/extension_tab_util.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/safe_browsing/extension_telemetry/extension_telemetry_service_factory.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/common/chrome_paths.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/ui_test_utils.h"
#include "components/enterprise/connectors/core/reporting_constants.h"
#include "components/metrics/content/subprocess_metrics_provider.h"
#include "components/safe_browsing/content/browser/web_ui/safe_browsing_ui.h"
#include "components/safe_browsing/core/common/features.h"
#include "components/safe_browsing/core/common/proto/csd.pb.h"
#include "components/safe_browsing/core/common/safe_browsing_prefs.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/web_contents.h"
#include "content/public/test/browser_test.h"
#include "extensions/browser/extensions_test.h"
#include "extensions/common/api/declarative_net_request/test_utils.h"
#include "extensions/common/extension_features.h"
#include "extensions/test/extension_test_message_listener.h"
#include "extensions/test/result_catcher.h"
#include "extensions/test/test_extension_dir.h"
#include "net/dns/mock_host_resolver.h"
#include "testing/gtest/include/gtest/gtest.h"

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
using TestRule = extensions::declarative_net_request::TestRule;
using TestHeaderInfo = extensions::declarative_net_request::TestHeaderInfo;

class ExtensionTelemetryServiceBrowserTest
    : public extensions::ExtensionApiTest {
 public:
  ExtensionTelemetryServiceBrowserTest() {
    scoped_feature_list_.InitWithFeatures(
        /*enabled_features=*/
        {kExtensionTelemetryForEnterprise,
         kExtensionTelemetryReportContactedHosts,
         kExtensionTelemetryReportHostsContactedViaWebSocket,
         kExtensionTelemetryTabsApiSignal,
         kExtensionTelemetryTabsApiSignalCaptureVisibleTab,
         kExtensionTelemetryDeclarativeNetRequestActionSignal,
         extensions_features::kIncludeJSCallStackInExtensionApiRequest},
        /*disabled_features=*/
        {kExtensionTelemetryInterceptRemoteHostsContactedInRenderer});
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
        browser()->profile(), /*browser_test=*/true);
    // Enable enterprise policy.
    enterprise_connectors::test::SetOnSecurityEventReporting(
        /*prefs=*/prefs(),
        /*enabled=*/true,
        /*enabled_event_names=*/{},
        /*enabled_opt_in_events=*/
        {{enterprise_connectors::kExtensionTelemetryEvent, {"*"}}});
  }

  void TearDownOnMainThread() override {
    extensions::ExtensionApiTest::TearDownOnMainThread();
    event_report_validator_helper_.reset();
  }

 protected:
  content::WebContents* web_contents(Browser* browser) const {
    return browser->tab_strip_model()->GetActiveWebContents();
  }

  PrefService* prefs() { return browser()->profile()->GetPrefs(); }

  ExtensionTelemetryService* telemetry_service() {
    return ExtensionTelemetryServiceFactory::GetForProfile(
        browser()->profile());
  }

  bool IsTelemetryServiceEnabledForESB() {
    return telemetry_service()->esb_enabled_ &&
           !telemetry_service()->signal_processors_.empty() &&
           telemetry_service()->timer_.IsRunning();
  }

  bool IsTelemetryServiceEnabledForEnterprise() {
    return telemetry_service()->enterprise_enabled_ &&
           !telemetry_service()->enterprise_signal_processors_.empty() &&
           telemetry_service()->enterprise_timer_.IsRunning();
  }

  bool IsExtensionStoreEmpty() {
    return telemetry_service()->extension_store_.empty();
  }

  bool IsEnterpriseExtensionStoreEmpty() {
    return telemetry_service()->enterprise_extension_store_.empty();
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
    return telemetry_service()->CreateReportForEnterprise();
  }

  base::test::ScopedFeatureList scoped_feature_list_;
  base::FilePath test_extension_dir_;
  std::unique_ptr<enterprise_connectors::test::EventReportValidatorHelper>
      event_report_validator_helper_;
};

IN_PROC_BROWSER_TEST_F(ExtensionTelemetryServiceBrowserTest,
                       DetectsAndReportsRemoteHostContactedSignal) {
  SetSafeBrowsingState(browser()->profile()->GetPrefs(),
                       SafeBrowsingState::ENHANCED_PROTECTION);
  ASSERT_TRUE(StartEmbeddedTestServer());

  extensions::ResultCatcher result_catcher;
  // Load extension from the test extension directory.
  const auto* extension =
      LoadExtension(test_extension_dir_.AppendASCII("basic_crx"));
  ASSERT_TRUE(extension);
  ASSERT_TRUE(result_catcher.GetNextResult());

  // Successfully retrieve the extension telemetry instance.
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
  // Generate ESB telemetry report and verify the contents.
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
  EXPECT_EQ(extension_report.extension().name(), kExtensionName);
  EXPECT_EQ(extension_report.extension().version(), kExtensionVersion);
  // Verify the designated test extension's report has signal data.
  ASSERT_EQ(extension_report.signals().size(), 1);
  // Verify that extension store has been cleared after creating a telemetry
  // report.
  EXPECT_TRUE(IsExtensionStoreEmpty());
  // Verify signal proto from the reports.
  const ExtensionTelemetryReportRequest_SignalInfo& signal =
      extension_report.signals()[0];
  const RemoteHostContactedInfo& remote_host_contacted_info =
      signal.remote_host_contacted_info();
  ASSERT_EQ(remote_host_contacted_info.remote_host_size(), 2);
  EXPECT_FALSE(remote_host_contacted_info.collected_from_new_interception());

  const RemoteHostInfo& remote_host_info =
      remote_host_contacted_info.remote_host(0);
  EXPECT_EQ(remote_host_info.contact_count(), static_cast<uint32_t>(1));
  EXPECT_EQ(remote_host_info.url(), kExtensionContactedHost);
  EXPECT_EQ(remote_host_info.connection_protocol(), RemoteHostInfo::HTTP_HTTPS);
  EXPECT_EQ(remote_host_info.contacted_by(), RemoteHostInfo::EXTENSION);
  const RemoteHostInfo& remote_host_contacted_info_websocket =
      remote_host_contacted_info.remote_host(1);
  EXPECT_EQ(remote_host_contacted_info_websocket.contact_count(),
            static_cast<uint32_t>(1));
  EXPECT_EQ(remote_host_contacted_info_websocket.url(),
            kExtensionContactedHost);
  EXPECT_EQ(remote_host_contacted_info_websocket.connection_protocol(),
            RemoteHostInfo::WEBSOCKET);
  EXPECT_EQ(remote_host_contacted_info_websocket.contacted_by(),
            RemoteHostInfo::EXTENSION);

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
                       DetectsAndReportsCookiesGetAllSignal) {
  SetSafeBrowsingState(browser()->profile()->GetPrefs(),
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
  SetSafeBrowsingState(browser()->profile()->GetPrefs(),
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
  SetSafeBrowsingState(browser()->profile()->GetPrefs(),
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
  SetSafeBrowsingState(browser()->profile()->GetPrefs(),
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
  SetSafeBrowsingState(browser()->profile()->GetPrefs(),
                       SafeBrowsingState::ENHANCED_PROTECTION);
  ASSERT_TRUE(StartEmbeddedTestServer());

  static constexpr char kManifest[] =
      R"({
         "name": "Tabs API Extension",
         "version": "0.1",
         "manifest_version": 3,
         "permissions":["tabs"],
         "host_permissions": ["<all_urls>"],
         "background": { "service_worker" : "background.js" }
       })";
  static constexpr char kBackground[] =
      R"(
        var pass = chrome.test.callbackPass;
        function waitForAllTabs(callback) {
          // Wait for all tabs to load.
          function waitForTabs() {
            chrome.windows.getAll({"populate": true}, function(windows) {
              var ready = true;
              for (var i in windows) {
                for (var j in windows[i].tabs) {
                  if (windows[i].tabs[j].status != "complete") {
                    ready = false;
                    break;
                  }
                }
                if (!ready)
                  break;
              }
              if (ready)
                callback();
              else
                setTimeout(waitForTabs, 30);
            });
          }
          waitForTabs();
        }

        chrome.test.runTests([
          async function tabOps() {
            await chrome.tabs.create({url: 'http://www.google.com'});
            const second_tab = await chrome.tabs.create(
                {url: 'http://www.google.com'});
            await chrome.tabs.update({url:'http://www.example.com'});
            await chrome.tabs.remove(second_tab.id);
            chrome.test.succeed();
          },
          async function captureVisibleTabOp() {
            await chrome.windows.create({url: 'http://www.google.com'},
              pass(function(newWindow) {
                waitForAllTabs(pass(function() {
                  chrome.tabs.captureVisibleTab(newWindow.id, function() {
                    chrome.test.succeed();
                  });
                }));
              }));
          },
        ]);
      )";

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
    EXPECT_EQ(call_details.new_url(), "http://www.google.com/");

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
    EXPECT_EQ(call_details.current_url(), "http://www.google.com/");
    EXPECT_EQ(call_details.new_url(), "http://www.example.com/");
    EXPECT_EQ(call_details.js_callstacks_size(), 1);

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
    EXPECT_EQ(call_details.current_url(), "http://www.example.com/");
    EXPECT_EQ(call_details.new_url(), "");
    EXPECT_EQ(call_details.js_callstacks_size(), 1);

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
    EXPECT_EQ(call_details.current_url(), "http://www.google.com/");
    EXPECT_EQ(call_details.new_url(), "");

    // Check the JS call stack information.
    EXPECT_EQ(call_details.js_callstacks_size(), 1);
    const JSCallStack& callstack = call_details.js_callstacks(0);
    ASSERT_GE(callstack.frames_size(), 1);
    EXPECT_EQ(callstack.frames(0).script_name(), "/background.js");
    EXPECT_EQ(callstack.frames(0).function_name(), "<anonymous>");
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

// Test fixture with kExtensionTelemetryInterceptRemoteHostsContactedInRenderer
// enabled.
class
    ExtensionTelemetryServiceBrowserTestWithInterceptRemoteHostsContactedInRendererEnabled
    : public ExtensionTelemetryServiceBrowserTest {
 public:
  ExtensionTelemetryServiceBrowserTestWithInterceptRemoteHostsContactedInRendererEnabled() {
    scoped_feature_list_.Reset();
    scoped_feature_list_.InitWithFeatures(
        /*enabled_features=*/
        {kExtensionTelemetryInterceptRemoteHostsContactedInRenderer,
         kExtensionTelemetryReportContactedHosts,
         kExtensionTelemetryReportHostsContactedViaWebSocket},
        /*disabled_features=*/{});
  }
};

IN_PROC_BROWSER_TEST_F(
    ExtensionTelemetryServiceBrowserTestWithInterceptRemoteHostsContactedInRendererEnabled,
    InterceptsRemoteHostContactedSignalInRenderer) {
  base::HistogramTester histogram_tester;
  SetSafeBrowsingState(browser()->profile()->GetPrefs(),
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
    auto extension_report = base::ranges::find_if(
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
  // Using MergeHistogramDeltasForTesting syncs the browser and renderer process
  // logs.
  metrics::SubprocessMetricsProvider::MergeHistogramDeltasForTesting();
  // A log of "false" represents the data being sent, while a log of "true"
  // represents the data being received.
  histogram_tester.ExpectBucketCount(
      "SafeBrowsing.ExtensionTelemetry.WebSocketRequestDataSentOrReceived",
      false, 1);
  histogram_tester.ExpectBucketCount(
      "SafeBrowsing.ExtensionTelemetry.WebSocketRequestDataSentOrReceived",
      true, 1);
}

IN_PROC_BROWSER_TEST_F(
    ExtensionTelemetryServiceBrowserTestWithInterceptRemoteHostsContactedInRendererEnabled,
    DetectsWebRequestFromContentScript) {
  SetSafeBrowsingState(browser()->profile()->GetPrefs(),
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
    auto extension_report = base::ranges::find_if(
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

}  // namespace safe_browsing
