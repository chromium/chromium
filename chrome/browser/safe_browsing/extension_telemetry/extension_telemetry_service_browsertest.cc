// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/command_line.h"
#include "base/containers/contains.h"
#include "base/files/file_path.h"
#include "base/path_service.h"
#include "base/test/scoped_feature_list.h"
#include "chrome/browser/extensions/extension_apitest.h"
#include "chrome/browser/extensions/extension_tab_util.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/safe_browsing/extension_telemetry/extension_telemetry_service.h"
#include "chrome/browser/safe_browsing/extension_telemetry/extension_telemetry_service_factory.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/common/chrome_paths.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/ui_test_utils.h"
#include "components/safe_browsing/content/browser/web_ui/safe_browsing_ui.h"
#include "components/safe_browsing/core/common/features.h"
#include "components/safe_browsing/core/common/proto/csd.pb.h"
#include "components/safe_browsing/core/common/safe_browsing_prefs.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/web_contents.h"
#include "content/public/test/browser_test.h"
#include "extensions/browser/extensions_test.h"
#include "extensions/common/api/declarative_net_request/test_utils.h"
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
using CookiesGetAllInfo =
    ExtensionTelemetryReportRequest_SignalInfo_CookiesGetAllInfo;
using DeclarativeNetRequestInfo =
    ExtensionTelemetryReportRequest_SignalInfo_DeclarativeNetRequestInfo;
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
        {kExtensionTelemetry, kExtensionTelemetryReportContactedHosts,
         kExtensionTelemetryReportHostsContactedViaWebSocket,
         kExtensionTelemetryTabsApiSignal},
        /*disabled_features=*/
        {kExtensionTelemetryInterceptRemoteHostsContactedInRenderer});
    CHECK(base::PathService::Get(chrome::DIR_TEST_DATA, &test_extension_dir_));
    test_extension_dir_ =
        test_extension_dir_.AppendASCII("safe_browsing/extension_telemetry");
  }
  void SetUpOnMainThread() override {
    extensions::ExtensionApiTest::SetUpOnMainThread();
    host_resolver()->AddRule("*", "127.0.0.1");
  }

 protected:
  bool IsTelemetryServiceEnabled(ExtensionTelemetryService* telemetry_service) {
    return telemetry_service->enabled() &&
           !telemetry_service->signal_processors_.empty() &&
           telemetry_service->timer_.IsRunning();
  }

  bool IsExtensionStoreEmpty(ExtensionTelemetryService* telemetry_service) {
    return telemetry_service->extension_store_.empty();
  }

  using ExtensionInfo = ExtensionTelemetryReportRequest_ExtensionInfo;
  const ExtensionInfo* GetExtensionInfoFromExtensionStore(
      ExtensionTelemetryService* telemetry_service,
      const extensions::ExtensionId& extension_id) {
    auto iter = telemetry_service->extension_store_.find(extension_id);
    if (iter == telemetry_service->extension_store_.end()) {
      return nullptr;
    }
    return iter->second.get();
  }

  using TelemetryReport = ExtensionTelemetryReportRequest;
  std::unique_ptr<TelemetryReport> GetTelemetryReport(
      ExtensionTelemetryService* telemetry_service) {
    return telemetry_service->CreateReport();
  }

  base::test::ScopedFeatureList scoped_feature_list_;
  base::FilePath test_extension_dir_;
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
  // Retrieve extension telemetry service instance.
  auto* telemetry_service =
      ExtensionTelemetryServiceFactory::GetForProfile(profile());
  // Successfully retrieve the extension telemetry instance.
  ASSERT_NE(telemetry_service, nullptr);
  ASSERT_TRUE(IsTelemetryServiceEnabled(telemetry_service));
  // Process signal.
  {
    // Verify that the registered extension information is saved in the
    // telemetry service's extension store.
    const ExtensionInfo* info =
        GetExtensionInfoFromExtensionStore(telemetry_service, extension->id());
    EXPECT_EQ(extension->name(), kExtensionName);
    EXPECT_EQ(extension->id(), extension->id());
    EXPECT_EQ(info->version(), kExtensionVersion);
  }
  // Generate telemetry report and verify.
  {
    // Verify the contents of telemetry report generated.
    std::unique_ptr<TelemetryReport> telemetry_report_pb =
        GetTelemetryReport(telemetry_service);
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
    EXPECT_TRUE(IsExtensionStoreEmpty(telemetry_service));
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
    EXPECT_EQ(remote_host_info.connection_protocol(),
              RemoteHostInfo::HTTP_HTTPS);
    const RemoteHostInfo& remote_host_contacted_info_websocket =
        remote_host_contacted_info.remote_host(1);
    EXPECT_EQ(remote_host_contacted_info_websocket.contact_count(),
              static_cast<uint32_t>(1));
    EXPECT_EQ(remote_host_contacted_info_websocket.url(),
              kExtensionContactedHost);
    EXPECT_EQ(remote_host_contacted_info_websocket.connection_protocol(),
              RemoteHostInfo::WEBSOCKET);
  }
}

IN_PROC_BROWSER_TEST_F(ExtensionTelemetryServiceBrowserTest,
                       DetectsAndReportsCookiesGetAllSignal) {
  SetSafeBrowsingState(browser()->profile()->GetPrefs(),
                       SafeBrowsingState::ENHANCED_PROTECTION);
  ASSERT_TRUE(StartEmbeddedTestServer());

  constexpr char kManifest[] =
      R"({
         "name": "Simple Cookies Extension",
         "version": "0.1",
         "manifest_version": 3,
         "background": { "service_worker": "background.js" },
         "permissions": ["cookies"],
         "host_permissions": ["<all_urls>"]
       })";
  constexpr char kBackground[] =
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

  // Retrieve extension telemetry service instance.
  auto* telemetry_service =
      ExtensionTelemetryServiceFactory::GetForProfile(profile());
  ASSERT_NE(telemetry_service, nullptr);
  ASSERT_TRUE(IsTelemetryServiceEnabled(telemetry_service));

  // Verify the contents of telemetry report generated.
  std::unique_ptr<TelemetryReport> telemetry_report_pb =
      GetTelemetryReport(telemetry_service);
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
  EXPECT_TRUE(IsExtensionStoreEmpty(telemetry_service));

  // Verify signal proto from the reports.
  const ExtensionTelemetryReportRequest_SignalInfo& signal =
      extension_report.signals()[0];
  const CookiesGetAllInfo& cookies_get_all_info = signal.cookies_get_all_info();
  ASSERT_EQ(cookies_get_all_info.get_all_args_info_size(), 1);
  ASSERT_EQ(cookies_get_all_info.max_exceeded_args_count(),
            static_cast<uint32_t>(0));

  const GetAllArgsInfo& get_all_args_info =
      cookies_get_all_info.get_all_args_info(0);
  EXPECT_EQ(get_all_args_info.count(), static_cast<uint32_t>(2));
  EXPECT_EQ(get_all_args_info.domain(), "cookies.com");
  EXPECT_EQ(get_all_args_info.name(), "test_basic_cookie");
  EXPECT_EQ(get_all_args_info.path(), "");
  EXPECT_FALSE(get_all_args_info.has_secure());
  EXPECT_EQ(get_all_args_info.store_id(), "0");
  EXPECT_EQ(get_all_args_info.url(), "https://extensions.cookies.com/");
  EXPECT_FALSE(get_all_args_info.has_is_session());
}

IN_PROC_BROWSER_TEST_F(ExtensionTelemetryServiceBrowserTest,
                       DetectsAndReportsCookiesGetSignal) {
  SetSafeBrowsingState(browser()->profile()->GetPrefs(),
                       SafeBrowsingState::ENHANCED_PROTECTION);
  ASSERT_TRUE(StartEmbeddedTestServer());

  constexpr char kManifest[] =
      R"({
         "name": "Simple Cookies Extension",
         "version": "0.1",
         "manifest_version": 3,
         "background": { "service_worker": "background.js" },
         "permissions": ["cookies"],
         "host_permissions": ["<all_urls>"]
       })";
  constexpr char kBackground[] =
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

  // Retrieve extension telemetry service instance.
  auto* telemetry_service =
      ExtensionTelemetryServiceFactory::GetForProfile(profile());
  ASSERT_NE(telemetry_service, nullptr);
  ASSERT_TRUE(IsTelemetryServiceEnabled(telemetry_service));
  {
    // Verify the contents of telemetry report generated.
    std::unique_ptr<TelemetryReport> telemetry_report_pb =
        GetTelemetryReport(telemetry_service);
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
    EXPECT_TRUE(IsExtensionStoreEmpty(telemetry_service));

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
  }
}

IN_PROC_BROWSER_TEST_F(ExtensionTelemetryServiceBrowserTest,
                       DetectsAndReportsDeclarativeNetRequestSignal) {
  SetSafeBrowsingState(browser()->profile()->GetPrefs(),
                       SafeBrowsingState::ENHANCED_PROTECTION);
  ASSERT_TRUE(StartEmbeddedTestServer());

  constexpr char kManifest[] = R"(
    {
      "name": "Simple DeclarativeNetRequest Extension",
      "version": "0.1",
      "manifest_version": 3,
      "background": { "service_worker": "background.js" },
      "permissions": ["declarativeNetRequest"],
      "host_permissions": ["<all_urls>"]
    })";
  constexpr char kBackground[] = R"(
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

  // Retrieve extension telemetry service instance.
  auto* telemetry_service =
      ExtensionTelemetryServiceFactory::GetForProfile(profile());
  ASSERT_NE(telemetry_service, nullptr);
  ASSERT_TRUE(IsTelemetryServiceEnabled(telemetry_service));
  {
    // Verify the contents of telemetry report generated.
    std::unique_ptr<TelemetryReport> telemetry_report_pb =
        GetTelemetryReport(telemetry_service);
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
         "background": { "service_worker" : "background.js" }
       })";
  static constexpr char kBackground[] =
      R"(
        chrome.test.runTests([
          async function tabOps() {
            await chrome.tabs.create({url: 'http://www.google.com'});
            const second_tab = await chrome.tabs.create(
                {url: 'http://www.google.com'});
            await chrome.tabs.update({url:'http://www.example.com'});
            await chrome.tabs.remove(second_tab.id);
            chrome.test.succeed();
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

  // Retrieve extension telemetry service instance.
  auto* telemetry_service = ExtensionTelemetryService::Get(profile());
  ASSERT_NE(telemetry_service, nullptr);
  ASSERT_TRUE(IsTelemetryServiceEnabled(telemetry_service));

  // Verify the contents of telemetry report generated.
  std::unique_ptr<TelemetryReport> telemetry_report_pb =
      GetTelemetryReport(telemetry_service);
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
  EXPECT_TRUE(IsExtensionStoreEmpty(telemetry_service));

  // Verify signal proto from the reports.
  const ExtensionTelemetryReportRequest_SignalInfo& signal =
      extension_report.signals()[0];

  // Verify the number of unique call details.
  using TabsApiInfo = ExtensionTelemetryReportRequest_SignalInfo_TabsApiInfo;
  const TabsApiInfo& tabs_api_info = signal.tabs_api_info();
  ASSERT_EQ(tabs_api_info.call_details_size(), 3);

  // Verify the contents of each call details.
  {
    const TabsApiInfo::CallDetails& call_details =
        tabs_api_info.call_details(0);
    EXPECT_EQ(call_details.count(), 2u);
    EXPECT_EQ(call_details.method(), TabsApiInfo::CREATE);
    EXPECT_EQ(call_details.current_url(), "");
    EXPECT_EQ(call_details.new_url(), "http://www.google.com/");
  }
  {
    const TabsApiInfo::CallDetails& call_details =
        tabs_api_info.call_details(1);
    EXPECT_EQ(call_details.count(), 1u);
    EXPECT_EQ(call_details.method(), TabsApiInfo::UPDATE);
    EXPECT_EQ(call_details.current_url(), "http://www.google.com/");
    EXPECT_EQ(call_details.new_url(), "http://www.example.com/");
  }
  {
    const TabsApiInfo::CallDetails& call_details =
        tabs_api_info.call_details(2);
    EXPECT_EQ(call_details.count(), 1u);
    EXPECT_EQ(call_details.method(), TabsApiInfo::REMOVE);
    EXPECT_EQ(call_details.current_url(), "http://www.example.com/");
    EXPECT_EQ(call_details.new_url(), "");
  }
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
        {kExtensionTelemetry,
         kExtensionTelemetryInterceptRemoteHostsContactedInRenderer,
         kExtensionTelemetryReportContactedHosts,
         kExtensionTelemetryReportHostsContactedViaWebSocket},
        /*disabled_features=*/
        {});
  }
};

IN_PROC_BROWSER_TEST_F(
    ExtensionTelemetryServiceBrowserTestWithInterceptRemoteHostsContactedInRendererEnabled,
    InterceptsRemoteHostContactedSignalInRenderer) {
  SetSafeBrowsingState(browser()->profile()->GetPrefs(),
                       SafeBrowsingState::ENHANCED_PROTECTION);
  ASSERT_TRUE(StartEmbeddedTestServer());
  extensions::ResultCatcher result_catcher;
  // Load extension from the test extension directory.
  const auto* extension =
      LoadExtension(test_extension_dir_.AppendASCII("basic_crx"));
  ASSERT_TRUE(extension);
  ASSERT_TRUE(result_catcher.GetNextResult());
  // Retrieve extension telemetry service instance.
  auto* telemetry_service =
      ExtensionTelemetryServiceFactory::GetForProfile(profile());
  // Successfully retrieve the extension telemetry instance.
  ASSERT_NE(telemetry_service, nullptr);
  ASSERT_TRUE(IsTelemetryServiceEnabled(telemetry_service));
  // Process signal.
  {
    // Verify that the registered extension information is saved in the
    // telemetry service's extension store.
    const ExtensionInfo* info =
        GetExtensionInfoFromExtensionStore(telemetry_service, extension->id());
    EXPECT_EQ(extension->name(), kExtensionName);
    EXPECT_EQ(extension->id(), extension->id());
    EXPECT_EQ(info->version(), kExtensionVersion);
  }
  // Generate telemetry report and verify.
  {
    // Verify the contents of telemetry report generated.
    std::unique_ptr<TelemetryReport> telemetry_report_pb =
        GetTelemetryReport(telemetry_service);
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
    EXPECT_TRUE(IsExtensionStoreEmpty(telemetry_service));

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
    const RemoteHostInfo& remote_host_contacted_info_websocket =
        remote_host_contacted_info.remote_host(1);
    EXPECT_EQ(remote_host_contacted_info_websocket.contact_count(), 1u);
    EXPECT_EQ(remote_host_contacted_info_websocket.url(),
              kExtensionContactedHost);
    EXPECT_EQ(remote_host_contacted_info_websocket.connection_protocol(),
              RemoteHostInfo::WEBSOCKET);
  }
}

}  // namespace safe_browsing
