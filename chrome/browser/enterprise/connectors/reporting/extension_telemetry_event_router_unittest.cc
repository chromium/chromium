// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/enterprise/connectors/reporting/extension_telemetry_event_router.h"

#include "base/memory/raw_ptr.h"
#include "base/strings/stringprintf.h"
#include "base/test/values_test_util.h"
#include "chrome/browser/enterprise/connectors/reporting/realtime_reporting_client.h"
#include "chrome/browser/enterprise/connectors/reporting/realtime_reporting_client_factory.h"
#include "chrome/browser/enterprise/connectors/test/deep_scanning_test_utils.h"
#include "chrome/browser/policy/dm_token_utils.h"
#include "chrome/test/base/testing_browser_process.h"
#include "chrome/test/base/testing_profile.h"
#include "chrome/test/base/testing_profile_manager.h"
#include "components/enterprise/connectors/core/reporting_constants.h"
#include "components/enterprise/connectors/core/reporting_service_settings.h"
#include "components/policy/core/common/cloud/mock_cloud_policy_client.h"
#include "components/safe_browsing/core/common/features.h"
#include "content/public/test/browser_task_environment.h"
#include "extensions/browser/extension_registry.h"
#include "extensions/common/manifest_constants.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace enterprise_connectors {

namespace {

using ::google::protobuf::RepeatedPtrField;
using ::testing::_;
using ::testing::ByRef;
using ::testing::Eq;
using ::testing::Return;

using ExtensionInfo =
    ::safe_browsing::ExtensionTelemetryReportRequest_ExtensionInfo;
using RemoteHostInfo = safe_browsing::
    ExtensionTelemetryReportRequest_SignalInfo_RemoteHostContactedInfo_RemoteHostInfo;
using CookiesGetAllInfo =
    safe_browsing::ExtensionTelemetryReportRequest_SignalInfo_CookiesGetAllInfo;
using CookiesGetInfo =
    safe_browsing::ExtensionTelemetryReportRequest_SignalInfo_CookiesGetInfo;
using RemoteHostContactedInfo = safe_browsing::
    ExtensionTelemetryReportRequest_SignalInfo_RemoteHostContactedInfo;
using TabsApiInfo =
    safe_browsing::ExtensionTelemetryReportRequest_SignalInfo_TabsApiInfo;

constexpr const char kFakeProfileUsername[] = "fake-profile";
constexpr const char kFakeExtensionId[] = "fake-extension-id";
constexpr const char kFakeExtensionVersion[] = "1";
constexpr const char kFakeExtensionName[] = "Foo extension";

constexpr const char kCookieDomain[] = "example-domain";
constexpr const char kCookieName[] = "cookie-1";
constexpr const char kCookiePath[] = "/path1";
constexpr const char kCookieStoreId[] = "store-1";
constexpr const char kCookieURL[] = "www.example1.com/";
constexpr const char kRemoteHostContactedURL[] = "www.youtube.com/";
constexpr const char kTabsNewURL[] = "www.gogle.com/";
constexpr const char kTabsCurrentURL[] = "www.google.com/";

}  // namespace

class MockRealtimeReportingClient : public RealtimeReportingClient {
 public:
  explicit MockRealtimeReportingClient(content::BrowserContext* context)
      : RealtimeReportingClient(context) {}
  MockRealtimeReportingClient(const MockRealtimeReportingClient&) = delete;
  MockRealtimeReportingClient& operator=(const MockRealtimeReportingClient&) =
      delete;

  MOCK_METHOD3(ReportRealtimeEvent,
               void(const std::string&,
                    const ReportingSettings& settings,
                    base::Value::Dict event));

  MOCK_METHOD(std::string, GetProfileUserName, (), (const, override));
};

std::unique_ptr<KeyedService> MakeMockRealtimeReportingClient(
    content::BrowserContext* profile_) {
  return std::make_unique<MockRealtimeReportingClient>(profile_);
}

class ExtensionTelemetryEventRouterTest : public testing::Test {
 public:
  ExtensionTelemetryEventRouterTest()
      : profile_manager_(TestingBrowserProcess::GetGlobal()) {}

  void SetUp() override {
    EXPECT_TRUE(profile_manager_.SetUp());
    profile_ = profile_manager_.CreateTestingProfile(kFakeProfileUsername);
    policy::SetDMTokenForTesting(
        policy::DMToken::CreateValidToken("fake-token"));

    RealtimeReportingClientFactory::GetInstance()->SetTestingFactory(
        profile_, base::BindRepeating(&MakeMockRealtimeReportingClient));
    extension_telemetry_event_router_ =
        std::make_unique<ExtensionTelemetryEventRouter>(profile_);

    mock_realtime_reporting_client_ = static_cast<MockRealtimeReportingClient*>(
        RealtimeReportingClientFactory::GetForProfile(profile_));

    test::SetOnSecurityEventReporting(
        profile_->GetPrefs(), /*enabled=*/true,
        /*enabled_event_names=*/{},
        /*enabled_opt_in_events=*/
        {{enterprise_connectors::kExtensionTelemetryEvent, {"*"}}});
    // Set a mock cloud policy client in the router.
    client_ = std::make_unique<policy::MockCloudPolicyClient>();
    client_->SetDMToken("fake-token");
    mock_realtime_reporting_client_->SetBrowserCloudPolicyClientForTesting(
        client_.get());
  }

  void TearDown() override {
    mock_realtime_reporting_client_->SetBrowserCloudPolicyClientForTesting(
        nullptr);
  }

  std::unique_ptr<safe_browsing::ExtensionTelemetryReportRequest>
  GenerateTelemetryReportRequest(
      ExtensionInfo::InstallLocation install_location =
          ExtensionInfo::INTERNAL) {
    auto telemetry_report_request =
        std::make_unique<safe_browsing::ExtensionTelemetryReportRequest>();
    telemetry_report_request->set_creation_timestamp_msec(1718811019088);
    safe_browsing::ExtensionTelemetryReportRequest_Report* telemetry_report =
        telemetry_report_request->add_reports();

    // Extension info
    safe_browsing::ExtensionTelemetryReportRequest_ExtensionInfo*
        extension_info = telemetry_report->mutable_extension();
    extension_info->set_id(kFakeExtensionId);
    extension_info->set_name(kFakeExtensionName);
    extension_info->set_version(kFakeExtensionVersion);
    extension_info->set_install_location(install_location);
    if (install_location == ExtensionInfo::UNPACKED) {
      extension_info->add_file_infos();
    }

    // Cookies get all signal
    safe_browsing::ExtensionTelemetryReportRequest_SignalInfo*
        cookies_get_all_signal = telemetry_report->add_signals();
    CookiesGetAllInfo::GetAllArgsInfo* get_all_args_info =
        cookies_get_all_signal->mutable_cookies_get_all_info()
            ->add_get_all_args_info();
    get_all_args_info->set_domain(kCookieDomain);
    get_all_args_info->set_name(kCookieName);
    get_all_args_info->set_path(kCookiePath);
    get_all_args_info->set_secure(true);
    get_all_args_info->set_store_id(kCookieStoreId);
    get_all_args_info->set_url(kCookieURL);
    get_all_args_info->set_is_session(true);
    get_all_args_info->set_count(1);

    // Cookies get signal
    safe_browsing::ExtensionTelemetryReportRequest_SignalInfo*
        cookies_get_signal = telemetry_report->add_signals();
    CookiesGetInfo::GetArgsInfo* get_args_info =
        cookies_get_signal->mutable_cookies_get_info()->add_get_args_info();
    get_args_info->set_name(kCookieName);
    get_args_info->set_store_id(kCookieStoreId);
    get_args_info->set_url(kCookieURL);
    get_args_info->set_count(2);

    // Remote host contacted signal
    safe_browsing::ExtensionTelemetryReportRequest_SignalInfo*
        remote_host_contacted_signal = telemetry_report->add_signals();
    RemoteHostContactedInfo::RemoteHostInfo* remote_host_info =
        remote_host_contacted_signal->mutable_remote_host_contacted_info()
            ->add_remote_host();
    remote_host_info->set_url(kRemoteHostContactedURL);
    remote_host_info->set_connection_protocol(
        RemoteHostContactedInfo::RemoteHostInfo::HTTP_HTTPS);
    remote_host_info->set_contacted_by(
        RemoteHostContactedInfo::RemoteHostInfo::CONTENT_SCRIPT);
    remote_host_info->set_contact_count(3);

    // Tabs Api signal
    safe_browsing::ExtensionTelemetryReportRequest_SignalInfo* tabs_api_signal =
        telemetry_report->add_signals();
    TabsApiInfo::CallDetails* call_details =
        tabs_api_signal->mutable_tabs_api_info()->add_call_details();
    call_details->set_method(TabsApiInfo::UPDATE);
    call_details->set_new_url(kTabsNewURL);
    call_details->set_current_url(kTabsCurrentURL);
    call_details->set_count(4);

    return telemetry_report_request;
  }

 protected:
  content::BrowserTaskEnvironment task_environment_;
  TestingProfileManager profile_manager_;
  raw_ptr<TestingProfile> profile_;
  std::unique_ptr<policy::MockCloudPolicyClient> client_;
  base::test::ScopedFeatureList scoped_feature_list_;
  raw_ptr<MockRealtimeReportingClient> mock_realtime_reporting_client_;
  std::unique_ptr<ExtensionTelemetryEventRouter>
      extension_telemetry_event_router_;
};

class ExtensionTelemetryEventInstallLocationTest
    : public ExtensionTelemetryEventRouterTest,
      public testing::WithParamInterface<ExtensionInfo::InstallLocation> {
 public:
  ExtensionTelemetryEventInstallLocationTest() {
    scoped_feature_list_.InitAndEnableFeature(
        safe_browsing::kExtensionTelemetryForEnterprise);
  }

 protected:
  ExtensionInfo::InstallLocation install_location_ = GetParam();
};

TEST_P(ExtensionTelemetryEventInstallLocationTest,
       CheckTelemetryEventReported) {
  const std::string event_json = base::StringPrintf(
      R"({
    "extension_telemetry_report": {
      "creation_timestamp_msec": "1718811019088",
      "reports": [{
        "extension": {%s
          "id": "fake-extension-id",
          "name": "Foo extension",
          "install_location": "%s",
          "is_from_store": false,
          "version": "1"
        },
        "signals": {
          "cookies_get_all_info": {
            "get_all_args_info": [ {
              "count": 1,
              "secure": true,
              "is_session": true,
              "name": "cookie-1",
              "path": "/path1",
              "store_id": "store-1",
              "domain": "example-domain",
              "url": "www.example1.com/"
            }]
          },
          "cookies_get_info": {
            "get_args_info": [{
              "count": 2,
              "name": "cookie-1",
              "store_id": "store-1",
              "url": "www.example1.com/"
            }]
          },
          "remote_host_contacted_info": {
            "remote_host": [ {
                "connection_protocol": "HTTP_HTTPS",
                "contact_count": 3,
                "contacted_by": "CONTENT_SCRIPT",
                "url": "www.youtube.com/"
            } ]
          },
          "tabs_api_info": {
            "call_details": [ {
                "count": 4,
                "new_url": "www.gogle.com/",
                "current_url": "www.google.com/",
                "method": "UPDATE"
            } ]
          }
        }
      }]
    }
  })",
      install_location_ == ExtensionInfo::UNPACKED ? R"(
        "file_info": [ {
          "hash": "",
          "name": ""
        } ],
      )"
                                                   : "",
      ExtensionInfo::InstallLocation_Name(install_location_).c_str());
  base::Value::Dict expected_event = base::test::ParseJsonDict(event_json);

  EXPECT_CALL(*mock_realtime_reporting_client_, GetProfileUserName())
      .WillRepeatedly(Return(kFakeProfileUsername));
  EXPECT_CALL(*mock_realtime_reporting_client_,
              ReportRealtimeEvent(kExtensionTelemetryEvent, _,
                                  Eq(ByRef(expected_event))));
  extension_telemetry_event_router_->UploadTelemetryReport(
      GenerateTelemetryReportRequest(install_location_));
}

INSTANTIATE_TEST_SUITE_P(
    ExtensionTelemetryEventInstallLocationTest,
    ExtensionTelemetryEventInstallLocationTest,
    testing::Values(ExtensionInfo::UNKNOWN_LOCATION,
                    ExtensionInfo::INTERNAL,
                    ExtensionInfo::EXTERNAL_PREF,
                    ExtensionInfo::EXTERNAL_REGISTRY,
                    ExtensionInfo::UNPACKED,
                    ExtensionInfo::COMPONENT,
                    ExtensionInfo::EXTERNAL_PREF_DOWNLOAD,
                    ExtensionInfo::EXTERNAL_POLICY_DOWNLOAD,
                    ExtensionInfo::COMMAND_LINE,
                    ExtensionInfo::EXTERNAL_POLICY,
                    ExtensionInfo::EXTERNAL_COMPONENT));

TEST_F(ExtensionTelemetryEventRouterTest, CheckIsPolicyEnabled) {
  // Feature disabled by default, and set reportiing to false.
  test::SetOnSecurityEventReporting(profile_->GetPrefs(), /*enabled=*/false);

  // Expect policy disabled.
  EXPECT_FALSE(extension_telemetry_event_router_->IsPolicyEnabled());

  // Enable feature.
  scoped_feature_list_.InitAndEnableFeature(
      safe_browsing::kExtensionTelemetryForEnterprise);

  // Expect policy still disabled due to reporting disabled.
  EXPECT_FALSE(extension_telemetry_event_router_->IsPolicyEnabled());

  // Enable reporting.
  test::SetOnSecurityEventReporting(
      profile_->GetPrefs(), /*enabled=*/true,
      /*enabled_event_names=*/{},
      /*enabled_opt_in_events=*/
      {{enterprise_connectors::kExtensionTelemetryEvent, {"*"}}});

  EXPECT_TRUE(extension_telemetry_event_router_->IsPolicyEnabled());
}

}  // namespace enterprise_connectors
