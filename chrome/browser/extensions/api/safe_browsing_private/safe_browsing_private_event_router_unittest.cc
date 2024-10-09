// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
#include "chrome/browser/extensions/api/safe_browsing_private/safe_browsing_private_event_router.h"

#include <memory>
#include <set>
#include <string>
#include <utility>

#include "base/check_deref.h"
#include "base/command_line.h"
#include "base/functional/bind.h"
#include "base/json/json_reader.h"
#include "base/memory/raw_ptr.h"
#include "base/strings/utf_string_conversions.h"
#include "base/test/mock_callback.h"
#include "base/test/scoped_feature_list.h"
#include "base/values.h"
#include "build/branding_buildflags.h"
#include "build/build_config.h"
#include "build/chromeos_buildflags.h"
#include "chrome/browser/enterprise/connectors/common.h"
#include "chrome/browser/enterprise/connectors/connectors_service.h"
#include "chrome/browser/enterprise/connectors/reporting/realtime_reporting_client.h"
#include "chrome/browser/enterprise/connectors/reporting/realtime_reporting_client_factory.h"
#include "chrome/browser/enterprise/connectors/test/deep_scanning_test_utils.h"
#include "chrome/browser/extensions/api/safe_browsing_private/safe_browsing_private_event_router_factory.h"
#include "chrome/browser/policy/dm_token_utils.h"
#include "chrome/browser/safe_browsing/cloud_content_scanning/deep_scanning_utils.h"
#include "chrome/browser/safe_browsing/test_extension_event_observer.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/common/extensions/api/safe_browsing_private.h"
#include "chrome/test/base/testing_browser_process.h"
#include "chrome/test/base/testing_profile.h"
#include "chrome/test/base/testing_profile_manager.h"
#include "components/download/public/common/download_danger_type.h"
#include "components/enterprise/browser/enterprise_switches.h"
#include "components/enterprise/common/proto/connectors.pb.h"
#include "components/enterprise/connectors/core/connectors_prefs.h"
#include "components/policy/core/common/cloud/mock_cloud_policy_client.h"
#include "components/policy/core/common/cloud/realtime_reporting_job_configuration.h"
#include "components/safe_browsing/core/common/features.h"
#include "components/safe_browsing/core/common/safe_browsing_prefs.h"
#include "components/signin/public/identity_manager/identity_test_environment.h"
#include "content/public/test/browser_task_environment.h"
#include "extensions/browser/test_event_router.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

#if BUILDFLAG(IS_CHROMEOS_ASH)
#include "chrome/browser/ash/login/users/fake_chrome_user_manager.h"
#include "chrome/browser/ash/login/users/user_manager_delegate_impl.h"
#include "chrome/browser/ash/profiles/profile_helper.h"
#include "chrome/browser/ash/settings/scoped_cros_settings_test_helper.h"
#include "chrome/browser/browser_process.h"
#include "chromeos/ash/components/install_attributes/stub_install_attributes.h"
#include "chromeos/ash/components/settings/cros_settings.h"
#include "components/account_id/account_id.h"
#include "components/user_manager/scoped_user_manager.h"
#include "components/user_manager/user.h"
#include "components/user_manager/user_manager_impl.h"
#else
#include "components/enterprise/browser/controller/fake_browser_dm_token_storage.h"
#endif

using ::testing::_;
using ::testing::Mock;
using ::testing::Return;
using ::testing::SaveArg;

namespace extensions {

namespace {

ACTION_P(CaptureArg, wrapper) {
  *wrapper = arg1.Clone();
}

constexpr char kConnectorsPrefValue[] = R"([
  {
    "service_provider": "google"
  }
])";

constexpr char kUrl[] = "https://evil.com/sensitive_data.txt";
constexpr char kTabUrl[] = "https://evil.site.com/";
constexpr char kSource[] = "exampleSource";
constexpr char kDestination[] = "exampleDestination";

}  // namespace

class SafeBrowsingEventObserver : public TestEventRouter::EventObserver {
 public:
  // The observer will only listen to events with the |event_name|.
  explicit SafeBrowsingEventObserver(const std::string& event_name)
      : event_name_(event_name) {}

  SafeBrowsingEventObserver(const SafeBrowsingEventObserver&) = delete;
  SafeBrowsingEventObserver& operator=(const SafeBrowsingEventObserver&) =
      delete;

  ~SafeBrowsingEventObserver() override = default;

  // Removes |event_args_| from |*this| and returns them.
  base::Value PassEventArgs() { return std::move(event_args_); }

  // extensions::TestEventRouter::EventObserver:
  void OnBroadcastEvent(const extensions::Event& event) override {
    if (event.event_name == event_name_) {
      event_args_ = base::Value(event.event_args.Clone());
    }
  }

 private:
  // The name of the observed event.
  const std::string event_name_;

  // The arguments passed for the last observed event.
  base::Value event_args_;
};

class SafeBrowsingPrivateEventRouterTestBase : public testing::Test {
 public:
  SafeBrowsingPrivateEventRouterTestBase()
      : profile_manager_(TestingBrowserProcess::GetGlobal()) {
    EXPECT_TRUE(profile_manager_.SetUp());
  }

  SafeBrowsingPrivateEventRouterTestBase(
      const SafeBrowsingPrivateEventRouterTestBase&) = delete;
  SafeBrowsingPrivateEventRouterTestBase& operator=(
      const SafeBrowsingPrivateEventRouterTestBase&) = delete;

  ~SafeBrowsingPrivateEventRouterTestBase() override = default;

  void SetUp() override {
    profile_ = profile_manager_.CreateTestingProfile("test-user");
    policy::SetDMTokenForTesting(
        policy::DMToken::CreateValidToken("fake-token"));
  }

  void TriggerOnPolicySpecifiedPasswordReuseDetectedEvent(bool warning_shown) {
    SafeBrowsingPrivateEventRouterFactory::GetForProfile(profile_)
        ->OnPolicySpecifiedPasswordReuseDetected(
            GURL("https://phishing.com/"), "user_name_1",
            /*is_phishing_url*/ true, warning_shown);
  }

  void TriggerOnPolicySpecifiedPasswordChangedEvent() {
    SafeBrowsingPrivateEventRouterFactory::GetForProfile(profile_)
        ->OnPolicySpecifiedPasswordChanged("user_name_2");
  }

  void TriggerOnDangerousDownloadOpenedEvent() {
    SafeBrowsingPrivateEventRouterFactory::GetForProfile(profile_)
        ->OnDangerousDownloadOpened(
            GURL("https://evil.com/malware.exe"), GURL("https://evil.site.com"),
            "/path/to/malware.exe", "sha256_of_malware_exe", "exe", "scan_id",
            download::DownloadDangerType::DOWNLOAD_DANGER_TYPE_DANGEROUS_FILE,
            1234);
  }

  void TriggerOnSecurityInterstitialShownEvent() {
    SafeBrowsingPrivateEventRouterFactory::GetForProfile(profile_)
        ->OnSecurityInterstitialShown(GURL("https://phishing.com/"), "PHISHING",
                                      0);
  }

  void TriggerOnSecurityInterstitialProceededEvent() {
    SafeBrowsingPrivateEventRouterFactory::GetForProfile(profile_)
        ->OnSecurityInterstitialProceeded(GURL("https://phishing.com/"),
                                          "PHISHING", -201);
  }

  void TriggerOnDangerousDownloadEvent() {
    SafeBrowsingPrivateEventRouterFactory::GetForProfile(profile_)
        ->OnDangerousDownloadEvent(
            GURL("https://maybevil.com/warning.exe"),
            GURL("https://maybe.evil/"), "/path/to/warning.exe",
            "sha256_of_warning_exe", "POTENTIALLY_UNWANTED", "exe", "scan_id",
            567, safe_browsing::EventResult::WARNED);
  }

  void TriggerOnDangerousDownloadEventBypass() {
    SafeBrowsingPrivateEventRouterFactory::GetForProfile(profile_)
        ->OnDangerousDownloadWarningBypassed(
            GURL("https://bypassevil.com/bypass.exe"),
            GURL("https://bypass.evil"), "/path/to/bypass.exe",
            "sha256_of_bypass_exe", "BYPASSED_WARNING", "exe", "scan_id", 890);
  }

  void TriggerOnSensitiveDataEvent(safe_browsing::EventResult event_result) {
    enterprise_connectors::ContentAnalysisResponse::Result result;
    result.set_tag("dlp");
    result.set_status(
        enterprise_connectors::ContentAnalysisResponse::Result::SUCCESS);
    auto* rule = result.add_triggered_rules();
    rule->set_action(enterprise_connectors::TriggeredRule::BLOCK);
    rule->set_rule_name("fake rule");
    rule->set_rule_id("12345");
    rule->set_url_category("test rule category");

    SafeBrowsingPrivateEventRouterFactory::GetForProfile(profile_)
        ->OnAnalysisConnectorResult(
            GURL(kUrl), GURL(kTabUrl), kSource, kDestination,
            "sensitive_data.txt", "sha256_of_data", "text/plain",
            SafeBrowsingPrivateEventRouter::kTriggerFileUpload, "scan_id",
            "content_transfer_method",
            safe_browsing::DeepScanAccessPoint::UPLOAD, result, 12345,
            event_result);
  }

  void TriggerOnUrlFilteringInterstitial(const std::string& threat_type,
                                         const std::string& watermark_message) {
    safe_browsing::RTLookupResponse response;
    auto* threat_info = response.add_threat_info();
    if (threat_type == "ENTERPRISE_WARNED_SEEN" ||
        threat_type == "ENTERPRISE_WARNED_BYPASS") {
      threat_info->set_verdict_type(
          safe_browsing::RTLookupResponse::ThreatInfo::WARN);
    } else if (threat_type == "ENTERPRISE_BLOCKED_SEEN") {
      threat_info->set_verdict_type(
          safe_browsing::RTLookupResponse::ThreatInfo::DANGEROUS);
    }
    auto* matched_url_navigation_rule =
        threat_info->mutable_matched_url_navigation_rule();
    matched_url_navigation_rule->set_rule_id("test rule id");
    matched_url_navigation_rule->set_rule_name("test rule name");
    matched_url_navigation_rule->set_matched_url_category("test rule category");
    if (!watermark_message.empty()) {
      matched_url_navigation_rule->mutable_watermark_message()
          ->set_watermark_message(watermark_message);
    }

    SafeBrowsingPrivateEventRouterFactory::GetForProfile(profile_)
        ->OnUrlFilteringInterstitial(GURL("https://filteredurl.com"),
                                     threat_type, response);
  }

  void TriggerOnUnscannedFileEvent(safe_browsing::EventResult result) {
    SafeBrowsingPrivateEventRouterFactory::GetForProfile(profile_)
        ->OnUnscannedFileEvent(
            GURL(kUrl), GURL(kTabUrl), kSource, kDestination,
            "sensitive_data.txt", "sha256_of_data", "text/plain",
            SafeBrowsingPrivateEventRouter::kTriggerFileDownload,
            safe_browsing::DeepScanAccessPoint::DOWNLOAD,
            "filePasswordProtected", "content_transfer_method", 12345, result);
  }

  void TriggerOnLoginEvent(
      const GURL& url,
      const std::u16string& login_user_name,
      url::SchemeHostPort federated_origin = url::SchemeHostPort()) {
    SafeBrowsingPrivateEventRouterFactory::GetForProfile(profile_)
        ->OnLoginEvent(url, federated_origin.IsValid(), federated_origin,
                       login_user_name);
  }

  void TriggerOnPasswordBreachEvent(
      const std::string& trigger,
      const std::vector<std::pair<GURL, std::u16string>>& identities) {
    SafeBrowsingPrivateEventRouterFactory::GetForProfile(profile_)
        ->OnPasswordBreach(trigger, identities);
  }

#if BUILDFLAG(ENTERPRISE_DATA_CONTROLS)
  void TriggerOnDataControlsSensitiveDataEvent(
      const data_controls::Verdict::TriggeredRules& triggered_rules) {
    SafeBrowsingPrivateEventRouterFactory::GetForProfile(profile_)
        ->OnDataControlsSensitiveDataEvent(
            GURL(kUrl), GURL(kTabUrl), kSource, kDestination, "text/plain",
            SafeBrowsingPrivateEventRouter::kTriggerWebContentUpload,
            triggered_rules, safe_browsing::EventResult::BLOCKED, 12345);
  }
#endif  // BUILDFLAG(ENTERPRISE_DATA_CONTROLS)

  void SetReportingPolicy(
      bool enabled,
      bool authorized = true,
      const std::set<std::string>& enabled_event_names =
          std::set<std::string>(),
      const std::map<std::string, std::vector<std::string>>&
          enabled_opt_in_events =
              std::map<std::string, std::vector<std::string>>()) {
    enterprise_connectors::test::SetOnSecurityEventReporting(
        profile_->GetPrefs(), enabled, enabled_event_names,
        enabled_opt_in_events);

    // If we are not enabling reporting, or if the client has already been
    // set for testing, just return.
    if (!enabled) {
      return;
    }

    if (client_ == nullptr) {
      // Set a mock cloud policy client in the router.
      client_ = std::make_unique<policy::MockCloudPolicyClient>();
      client_->SetDMToken("fake-token");
      enterprise_connectors::RealtimeReportingClientFactory::GetForProfile(
          profile_)
          ->SetBrowserCloudPolicyClientForTesting(client_.get());
    }

    if (!authorized) {
      // This causes the DM Token to be rejected, and unauthorized for 24 hours.
      client_->SetStatus(policy::DM_STATUS_SERVICE_MANAGEMENT_NOT_SUPPORTED);
      client_->NotifyClientError();
    }
  }

  void SetUpRouters(bool authorized = true,
                    bool realtime_reporting_enable = true,
                    const std::set<std::string>& enabled_event_names =
                        std::set<std::string>(),
                    const std::map<std::string, std::vector<std::string>>&
                        enabled_opt_in_events =
                            std::map<std::string, std::vector<std::string>>()) {
    event_router_ = extensions::CreateAndUseTestEventRouter(profile_);
    SafeBrowsingPrivateEventRouterFactory::GetInstance()->SetTestingFactory(
        profile_, base::BindRepeating(
                      &safe_browsing::BuildSafeBrowsingPrivateEventRouter));

    enterprise_connectors::RealtimeReportingClientFactory::GetInstance()
        ->SetTestingFactory(
            profile_,
            base::BindRepeating(&safe_browsing::BuildRealtimeReportingClient));

    SetReportingPolicy(realtime_reporting_enable, authorized,
                       enabled_event_names, enabled_opt_in_events);
  }

  std::string GetProfileIdentifier() const {
    return profile_->GetPath().AsUTF8Unsafe();
  }

 protected:
  content::BrowserTaskEnvironment task_environment_;
  std::unique_ptr<policy::MockCloudPolicyClient> client_;
  TestingProfileManager profile_manager_;
  raw_ptr<TestingProfile> profile_ = nullptr;
  raw_ptr<extensions::TestEventRouter> event_router_ = nullptr;

 private:
#if !BUILDFLAG(IS_CHROMEOS_ASH)
  policy::FakeBrowserDMTokenStorage dm_token_storage_;
#endif  // !BUILDFLAG(IS_CHROMEOS_ASH)
};

class SafeBrowsingPrivateEventRouterTest
    : public SafeBrowsingPrivateEventRouterTestBase {
#if BUILDFLAG(IS_CHROMEOS_ASH)
 public:
  SafeBrowsingPrivateEventRouterTest() = default;

 protected:
  ash::ScopedCrosSettingsTestHelper cros_settings_test_helper_;
  user_manager::ScopedUserManager user_manager_{
      std::make_unique<user_manager::UserManagerImpl>(
          std::make_unique<ash::UserManagerDelegateImpl>(),
          g_browser_process->local_state(),
          ash::CrosSettings::Get())};
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)
};

TEST_F(SafeBrowsingPrivateEventRouterTest, TestOnReuseDetected_Warned) {
  SetUpRouters();
  SafeBrowsingEventObserver event_observer(
      api::safe_browsing_private::OnPolicySpecifiedPasswordReuseDetected::
          kEventName);
  event_router_->AddEventObserver(&event_observer);

  base::Value::Dict report;
  EXPECT_CALL(*client_, UploadSecurityEventReport)
      .WillOnce(CaptureArg(&report));

  TriggerOnPolicySpecifiedPasswordReuseDetectedEvent(/*warning_shown*/ true);
  base::RunLoop().RunUntilIdle();

  base::Value::Dict captured_args =
      event_observer.PassEventArgs().GetList()[0].Clone().TakeDict();
  EXPECT_EQ("https://phishing.com/",
            CHECK_DEREF(captured_args.FindString("url")));
  EXPECT_EQ("user_name_1", CHECK_DEREF(captured_args.FindString("userName")));

  Mock::VerifyAndClearExpectations(client_.get());
  const base::Value::List* event_list =
      report.FindList(policy::RealtimeReportingJobConfiguration::kEventListKey);
  ASSERT_NE(nullptr, event_list);
  ASSERT_EQ(1u, event_list->size());
  const base::Value::Dict& wrapper = (*event_list)[0].GetDict();
  const base::Value::Dict* event =
      wrapper.FindDict(enterprise_connectors::kKeyPasswordReuseEvent);
  EXPECT_NE(nullptr, event);
  EXPECT_EQ("https://phishing.com/",
            *event->FindString(SafeBrowsingPrivateEventRouter::kKeyUrl));
  EXPECT_EQ("user_name_1",
            *event->FindString(SafeBrowsingPrivateEventRouter::kKeyUserName));
  EXPECT_EQ(
      safe_browsing::EventResultToString(safe_browsing::EventResult::WARNED),
      *event->FindString(SafeBrowsingPrivateEventRouter::kKeyEventResult));
}

TEST_F(SafeBrowsingPrivateEventRouterTest, TestOnReuseDetected_Allowed) {
  SetUpRouters();
  SafeBrowsingEventObserver event_observer(
      api::safe_browsing_private::OnPolicySpecifiedPasswordReuseDetected::
          kEventName);
  event_router_->AddEventObserver(&event_observer);

  base::Value::Dict report;
  EXPECT_CALL(*client_, UploadSecurityEventReport)
      .WillOnce(CaptureArg(&report));

  TriggerOnPolicySpecifiedPasswordReuseDetectedEvent(/*warning_shown*/ false);
  base::RunLoop().RunUntilIdle();

  base::Value::Dict captured_args =
      event_observer.PassEventArgs().GetList()[0].Clone().TakeDict();
  EXPECT_EQ("https://phishing.com/",
            CHECK_DEREF(captured_args.FindString("url")));
  EXPECT_EQ("user_name_1", CHECK_DEREF(captured_args.FindString("userName")));

  Mock::VerifyAndClearExpectations(client_.get());
  const base::Value::List* event_list =
      report.FindList(policy::RealtimeReportingJobConfiguration::kEventListKey);
  ASSERT_NE(nullptr, event_list);
  ASSERT_EQ(1u, event_list->size());
  const base::Value::Dict& wrapper = (*event_list)[0].GetDict();
  const base::Value::Dict* event =
      wrapper.FindDict(enterprise_connectors::kKeyPasswordReuseEvent);
  EXPECT_NE(nullptr, event);
  EXPECT_EQ("https://phishing.com/",
            *event->FindString(SafeBrowsingPrivateEventRouter::kKeyUrl));
  EXPECT_EQ("user_name_1",
            *event->FindString(SafeBrowsingPrivateEventRouter::kKeyUserName));
  EXPECT_EQ(
      safe_browsing::EventResultToString(safe_browsing::EventResult::ALLOWED),
      *event->FindString(SafeBrowsingPrivateEventRouter::kKeyEventResult));
}

TEST_F(SafeBrowsingPrivateEventRouterTest, TestOnPasswordChanged) {
  SetUpRouters();
  SafeBrowsingEventObserver event_observer(
      api::safe_browsing_private::OnPolicySpecifiedPasswordChanged::kEventName);
  event_router_->AddEventObserver(&event_observer);

  base::Value::Dict report;
  EXPECT_CALL(*client_, UploadSecurityEventReport)
      .WillOnce(CaptureArg(&report));

  TriggerOnPolicySpecifiedPasswordChangedEvent();
  base::RunLoop().RunUntilIdle();

  auto captured_args = event_observer.PassEventArgs().GetList()[0].Clone();
  EXPECT_EQ("user_name_2", captured_args.GetString());

  Mock::VerifyAndClearExpectations(client_.get());
  const base::Value::List* event_list =
      report.FindList(policy::RealtimeReportingJobConfiguration::kEventListKey);
  ASSERT_NE(nullptr, event_list);
  ASSERT_EQ(1u, event_list->size());
  const base::Value::Dict& wrapper = (*event_list)[0].GetDict();
  const base::Value::Dict* event =
      wrapper.FindDict(enterprise_connectors::kKeyPasswordChangedEvent);
  EXPECT_NE(nullptr, event);
  EXPECT_EQ("user_name_2",
            *event->FindString(SafeBrowsingPrivateEventRouter::kKeyUserName));
}

TEST_F(SafeBrowsingPrivateEventRouterTest, TestOnDangerousDownloadOpened) {
  SetUpRouters();
  SafeBrowsingEventObserver event_observer(
      api::safe_browsing_private::OnDangerousDownloadOpened::kEventName);
  event_router_->AddEventObserver(&event_observer);

  base::Value::Dict report;
  EXPECT_CALL(*client_, UploadSecurityEventReport)
      .WillOnce(CaptureArg(&report));

  TriggerOnDangerousDownloadOpenedEvent();
  base::RunLoop().RunUntilIdle();

  base::Value::Dict captured_args =
      event_observer.PassEventArgs().GetList()[0].Clone().TakeDict();
  EXPECT_EQ("https://evil.com/malware.exe",
            CHECK_DEREF(captured_args.FindString("url")));
  EXPECT_EQ("/path/to/malware.exe",
            CHECK_DEREF(captured_args.FindString("fileName")));
  EXPECT_EQ("", CHECK_DEREF(captured_args.FindString("userName")));
  EXPECT_EQ("sha256_of_malware_exe",
            CHECK_DEREF(captured_args.FindString("downloadDigestSha256")));

  Mock::VerifyAndClearExpectations(client_.get());
  const base::Value::List* event_list =
      report.FindList(policy::RealtimeReportingJobConfiguration::kEventListKey);
  ASSERT_NE(nullptr, event_list);
  ASSERT_EQ(1u, event_list->size());
  const base::Value::Dict& wrapper = (*event_list)[0].GetDict();
  const base::Value::Dict* event =
      wrapper.FindDict(enterprise_connectors::kKeyDangerousDownloadEvent);
  EXPECT_NE(nullptr, event);

#if BUILDFLAG(IS_CHROMEOS)
  // TODO(crbug.com/40941444): To fix the tests for ChromeOS.
  EXPECT_EQ("malware.exe",
#else
  EXPECT_EQ("/path/to/malware.exe",
#endif  // BUILDFLAG(IS_CHROMEOS)
            *event->FindString(SafeBrowsingPrivateEventRouter::kKeyFileName));
  EXPECT_EQ("exe", *event->FindString(
                       SafeBrowsingPrivateEventRouter::kKeyContentType));
  EXPECT_EQ("1234", *event->FindString(
                        SafeBrowsingPrivateEventRouter::kKeyContentSize));
  EXPECT_EQ(SafeBrowsingPrivateEventRouter::kTriggerFileDownload,
            *event->FindString(SafeBrowsingPrivateEventRouter::kKeyTrigger));
  EXPECT_EQ(
      safe_browsing::EventResultToString(safe_browsing::EventResult::BYPASSED),
      *event->FindString(SafeBrowsingPrivateEventRouter::kKeyEventResult));
  EXPECT_EQ("scan_id",
            *event->FindString(SafeBrowsingPrivateEventRouter::kKeyScanId));
}

TEST_F(SafeBrowsingPrivateEventRouterTest,
       TestOnSecurityInterstitialProceeded) {
  SetUpRouters();
  SafeBrowsingEventObserver event_observer(
      api::safe_browsing_private::OnSecurityInterstitialProceeded::kEventName);
  event_router_->AddEventObserver(&event_observer);

  base::Value::Dict report;
  EXPECT_CALL(*client_, UploadSecurityEventReport)
      .WillOnce(CaptureArg(&report));

  TriggerOnSecurityInterstitialProceededEvent();
  base::RunLoop().RunUntilIdle();

  base::Value::Dict captured_args =
      event_observer.PassEventArgs().GetList()[0].Clone().TakeDict();
  EXPECT_EQ("https://phishing.com/",
            CHECK_DEREF(captured_args.FindString("url")));
  EXPECT_EQ("PHISHING", CHECK_DEREF(captured_args.FindString("reason")));
  EXPECT_EQ("-201", CHECK_DEREF(captured_args.FindString("netErrorCode")));
  EXPECT_EQ("", CHECK_DEREF(captured_args.FindString("userName")));

  Mock::VerifyAndClearExpectations(client_.get());
  const base::Value::List* event_list =
      report.FindList(policy::RealtimeReportingJobConfiguration::kEventListKey);
  ASSERT_NE(nullptr, event_list);
  ASSERT_EQ(1u, event_list->size());
  const base::Value::Dict& wrapper = (*event_list)[0].GetDict();
  const base::Value::Dict* event =
      wrapper.FindDict(enterprise_connectors::kKeyInterstitialEvent);
  EXPECT_NE(nullptr, event);
  EXPECT_EQ("PHISHING",
            *event->FindString(SafeBrowsingPrivateEventRouter::kKeyReason));
  EXPECT_EQ(-201,
            *event->FindInt(SafeBrowsingPrivateEventRouter::kKeyNetErrorCode));
  EXPECT_TRUE(
      *event->FindBool(SafeBrowsingPrivateEventRouter::kKeyClickedThrough));
}

TEST_F(SafeBrowsingPrivateEventRouterTest, TestOnSecurityInterstitialShown) {
  SetUpRouters();
  SafeBrowsingEventObserver event_observer(
      api::safe_browsing_private::OnSecurityInterstitialShown::kEventName);
  event_router_->AddEventObserver(&event_observer);

  base::Value::Dict report;
  EXPECT_CALL(*client_, UploadSecurityEventReport)
      .WillOnce(CaptureArg(&report));

  TriggerOnSecurityInterstitialShownEvent();
  base::RunLoop().RunUntilIdle();

  base::Value::Dict captured_args =
      event_observer.PassEventArgs().GetList()[0].Clone().TakeDict();
  EXPECT_EQ("https://phishing.com/",
            CHECK_DEREF(captured_args.FindString("url")));
  EXPECT_EQ("PHISHING", CHECK_DEREF(captured_args.FindString("reason")));
  EXPECT_FALSE(captured_args.contains("netErrorCode"));
  EXPECT_EQ("", CHECK_DEREF(captured_args.FindString("userName")));

  Mock::VerifyAndClearExpectations(client_.get());
  const base::Value::List* event_list =
      report.FindList(policy::RealtimeReportingJobConfiguration::kEventListKey);
  ASSERT_NE(nullptr, event_list);
  ASSERT_EQ(1u, event_list->size());
  const base::Value::Dict& wrapper = (*event_list)[0].GetDict();
  const base::Value::Dict* event =
      wrapper.FindDict(enterprise_connectors::kKeyInterstitialEvent);
  EXPECT_NE(nullptr, event);
  EXPECT_EQ("PHISHING",
            *event->FindString(SafeBrowsingPrivateEventRouter::kKeyReason));
  EXPECT_EQ(0,
            *event->FindInt(SafeBrowsingPrivateEventRouter::kKeyNetErrorCode));
  EXPECT_FALSE(
      *event->FindBool(SafeBrowsingPrivateEventRouter::kKeyClickedThrough));
}

TEST_F(SafeBrowsingPrivateEventRouterTest, TestOnDangerousDownloadWarning) {
  SetUpRouters();
  SafeBrowsingEventObserver event_observer(
      api::safe_browsing_private::OnDangerousDownloadOpened::kEventName);
  event_router_->AddEventObserver(&event_observer);

  base::Value::Dict report;
  EXPECT_CALL(*client_, UploadSecurityEventReport)
      .WillOnce(CaptureArg(&report));

  TriggerOnDangerousDownloadEvent();
  base::RunLoop().RunUntilIdle();

  Mock::VerifyAndClearExpectations(client_.get());
  const base::Value::List* event_list =
      report.FindList(policy::RealtimeReportingJobConfiguration::kEventListKey);
  ASSERT_NE(nullptr, event_list);
  ASSERT_EQ(1u, event_list->size());
  const base::Value::Dict& wrapper = (*event_list)[0].GetDict();
  const base::Value::Dict* event =
      wrapper.FindDict(enterprise_connectors::kKeyDangerousDownloadEvent);
  EXPECT_NE(nullptr, event);
#if BUILDFLAG(IS_CHROMEOS)
  // TODO(crbug.com/1163303): To fix the tests for ChromeOS.
  EXPECT_EQ("warning.exe",
#else
  EXPECT_EQ("/path/to/warning.exe",
#endif  // BUILDFLAG(IS_CHROMEOS)
            *event->FindString(SafeBrowsingPrivateEventRouter::kKeyFileName));
  EXPECT_EQ("exe", *event->FindString(
                       SafeBrowsingPrivateEventRouter::kKeyContentType));
  EXPECT_EQ("567", *event->FindString(
                       SafeBrowsingPrivateEventRouter::kKeyContentSize));
  EXPECT_EQ("POTENTIALLY_UNWANTED",
            *event->FindString(SafeBrowsingPrivateEventRouter::kKeyThreatType));
  EXPECT_EQ(
      safe_browsing::EventResultToString(safe_browsing::EventResult::WARNED),
      *event->FindString(SafeBrowsingPrivateEventRouter::kKeyEventResult));
  EXPECT_EQ("scan_id",
            *event->FindString(SafeBrowsingPrivateEventRouter::kKeyScanId));
}

TEST_F(SafeBrowsingPrivateEventRouterTest,
       TestOnDangerousDownloadWarningBypass) {
  SetUpRouters();
  SafeBrowsingEventObserver event_observer(
      api::safe_browsing_private::OnDangerousDownloadOpened::kEventName);
  event_router_->AddEventObserver(&event_observer);

  base::Value::Dict report;
  EXPECT_CALL(*client_, UploadSecurityEventReport)
      .WillOnce(CaptureArg(&report));

  TriggerOnDangerousDownloadEventBypass();
  base::RunLoop().RunUntilIdle();

  Mock::VerifyAndClearExpectations(client_.get());
  const base::Value::List* event_list =
      report.FindList(policy::RealtimeReportingJobConfiguration::kEventListKey);
  ASSERT_NE(nullptr, event_list);
  ASSERT_EQ(1u, event_list->size());
  const base::Value::Dict& wrapper = (*event_list)[0].GetDict();
  const base::Value::Dict* event =
      wrapper.FindDict(enterprise_connectors::kKeyDangerousDownloadEvent);
  EXPECT_NE(nullptr, event);

#if BUILDFLAG(IS_CHROMEOS)
  // TODO(crbug.com/1163303): To fix the tests for ChromeOS.
  EXPECT_EQ("bypass.exe",
#else
  EXPECT_EQ("/path/to/bypass.exe",
#endif  // BUILDFLAG(IS_CHROMEOS)
            *event->FindString(SafeBrowsingPrivateEventRouter::kKeyFileName));
  EXPECT_EQ("exe", *event->FindString(
                       SafeBrowsingPrivateEventRouter::kKeyContentType));
  EXPECT_EQ("890", *event->FindString(
                       SafeBrowsingPrivateEventRouter::kKeyContentSize));
  EXPECT_EQ("BYPASSED_WARNING",
            *event->FindString(SafeBrowsingPrivateEventRouter::kKeyThreatType));
  EXPECT_EQ(
      safe_browsing::EventResultToString(safe_browsing::EventResult::BYPASSED),
      *event->FindString(SafeBrowsingPrivateEventRouter::kKeyEventResult));
  EXPECT_EQ("scan_id",
            *event->FindString(SafeBrowsingPrivateEventRouter::kKeyScanId));
}

TEST_F(SafeBrowsingPrivateEventRouterTest, PolicyControlOnToOffIsDynamic) {
  SetUpRouters();
  SafeBrowsingEventObserver event_observer(
      api::safe_browsing_private::OnSecurityInterstitialShown::kEventName);
  event_router_->AddEventObserver(&event_observer);

  EXPECT_CALL(*client_, UploadSecurityEventReport).Times(1);
  TriggerOnSecurityInterstitialShownEvent();
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(1u, event_observer.PassEventArgs().GetList().size());
  Mock::VerifyAndClearExpectations(client_.get());

  // Now turn off policy.  This time no report should be generated.
  SetReportingPolicy(false);
  EXPECT_CALL(*client_, UploadSecurityEventReport).Times(0);
  TriggerOnSecurityInterstitialShownEvent();
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(1u, event_observer.PassEventArgs().GetList().size());
  Mock::VerifyAndClearExpectations(client_.get());
}

TEST_F(SafeBrowsingPrivateEventRouterTest, PolicyControlOffToOnIsDynamic) {
  SetUpRouters(/*authorized=*/true, /*realtime_reporting_enable=*/false);
  SafeBrowsingEventObserver event_observer(
      api::safe_browsing_private::OnSecurityInterstitialShown::kEventName);
  event_router_->AddEventObserver(&event_observer);

  TriggerOnSecurityInterstitialShownEvent();
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(1u, event_observer.PassEventArgs().GetList().size());

  // Now turn on policy.
  SetReportingPolicy(true);
  EXPECT_CALL(*client_, UploadSecurityEventReport).Times(1);
  TriggerOnSecurityInterstitialShownEvent();
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(1u, event_observer.PassEventArgs().GetList().size());
  Mock::VerifyAndClearExpectations(client_.get());
}

TEST_F(SafeBrowsingPrivateEventRouterTest, TestUnauthorizedOnReuseDetected) {
  SetUpRouters(/*authorized=*/false);
  SafeBrowsingEventObserver event_observer(
      api::safe_browsing_private::OnPolicySpecifiedPasswordReuseDetected::
          kEventName);
  event_router_->AddEventObserver(&event_observer);

  base::Value report;
  EXPECT_CALL(*client_, UploadSecurityEventReport).Times(0);

  TriggerOnPolicySpecifiedPasswordReuseDetectedEvent(/*warning_shown*/ true);
  base::RunLoop().RunUntilIdle();

  Mock::VerifyAndClearExpectations(client_.get());
  EXPECT_EQ(base::Value::Type::NONE, report.type());
}

TEST_F(SafeBrowsingPrivateEventRouterTest, TestUnauthorizedOnPasswordChanged) {
  SetUpRouters(/*authorized=*/false);
  SafeBrowsingEventObserver event_observer(
      api::safe_browsing_private::OnPolicySpecifiedPasswordChanged::kEventName);
  event_router_->AddEventObserver(&event_observer);

  base::Value report;
  EXPECT_CALL(*client_, UploadSecurityEventReport).Times(0);

  TriggerOnPolicySpecifiedPasswordChangedEvent();
  base::RunLoop().RunUntilIdle();

  Mock::VerifyAndClearExpectations(client_.get());
  EXPECT_EQ(base::Value::Type::NONE, report.type());
}

TEST_F(SafeBrowsingPrivateEventRouterTest,
       TestUnauthorizedOnDangerousDownloadOpened) {
  SetUpRouters(/*authorized=*/false);
  SafeBrowsingEventObserver event_observer(
      api::safe_browsing_private::OnDangerousDownloadOpened::kEventName);
  event_router_->AddEventObserver(&event_observer);

  base::Value report;
  EXPECT_CALL(*client_, UploadSecurityEventReport).Times(0);

  TriggerOnDangerousDownloadOpenedEvent();
  base::RunLoop().RunUntilIdle();

  Mock::VerifyAndClearExpectations(client_.get());
  EXPECT_EQ(base::Value::Type::NONE, report.type());
}

TEST_F(SafeBrowsingPrivateEventRouterTest,
       TestUnauthorizedOnSecurityInterstitialProceeded) {
  SetUpRouters(/*authorized=*/false);
  SafeBrowsingEventObserver event_observer(
      api::safe_browsing_private::OnSecurityInterstitialProceeded::kEventName);
  event_router_->AddEventObserver(&event_observer);

  base::Value report;
  EXPECT_CALL(*client_, UploadSecurityEventReport).Times(0);

  TriggerOnSecurityInterstitialProceededEvent();
  base::RunLoop().RunUntilIdle();

  Mock::VerifyAndClearExpectations(client_.get());
  EXPECT_EQ(base::Value::Type::NONE, report.type());
}

TEST_F(SafeBrowsingPrivateEventRouterTest,
       TestUnauthorizedOnSecurityInterstitialShown) {
  SetUpRouters(/*authorized=*/false);
  SafeBrowsingEventObserver event_observer(
      api::safe_browsing_private::OnSecurityInterstitialShown::kEventName);
  event_router_->AddEventObserver(&event_observer);

  base::Value report;
  EXPECT_CALL(*client_, UploadSecurityEventReport).Times(0);

  TriggerOnSecurityInterstitialShownEvent();
  base::RunLoop().RunUntilIdle();

  Mock::VerifyAndClearExpectations(client_.get());
  EXPECT_EQ(base::Value::Type::NONE, report.type());
}

TEST_F(SafeBrowsingPrivateEventRouterTest,
       TestUnauthorizedOnDangerousDownloadWarning) {
  SetUpRouters(/*authorized=*/false);
  SafeBrowsingEventObserver event_observer(
      api::safe_browsing_private::OnDangerousDownloadOpened::kEventName);
  event_router_->AddEventObserver(&event_observer);

  base::Value report;
  EXPECT_CALL(*client_, UploadSecurityEventReport).Times(0);

  TriggerOnDangerousDownloadEvent();
  base::RunLoop().RunUntilIdle();

  Mock::VerifyAndClearExpectations(client_.get());
  EXPECT_EQ(base::Value::Type::NONE, report.type());
}

TEST_F(SafeBrowsingPrivateEventRouterTest,
       TestUnauthorizedOnDangerousDownloadWarningBypass) {
  SetUpRouters(/*authorized=*/false);
  SafeBrowsingEventObserver event_observer(
      api::safe_browsing_private::OnDangerousDownloadOpened::kEventName);
  event_router_->AddEventObserver(&event_observer);

  base::Value report;
  EXPECT_CALL(*client_, UploadSecurityEventReport).Times(0);

  TriggerOnDangerousDownloadEventBypass();
  base::RunLoop().RunUntilIdle();

  Mock::VerifyAndClearExpectations(client_.get());
  EXPECT_EQ(base::Value::Type::NONE, report.type());
}

TEST_F(SafeBrowsingPrivateEventRouterTest, TestOnLoginEvent) {
  SetUpRouters(
      /*authorized=*/true,
      /*realtime_reporting_enable=*/true,
      /*enabled_event_names=*/{},
      /*enabled_opt_in_events=*/{{"loginEvent", {"*"}}});

  signin::IdentityTestEnvironment identity_test_environment;
  enterprise_connectors::RealtimeReportingClientFactory::GetForProfile(profile_)
      ->SetIdentityManagerForTesting(
          identity_test_environment.identity_manager());
  identity_test_environment.MakePrimaryAccountAvailable(
      profile_->GetProfileUserName(), signin::ConsentLevel::kSignin);

  enterprise_connectors::test::EventReportValidator validator(client_.get());
  validator.ExpectLoginEvent("https://www.example.com/", false, "",
                             profile_->GetProfileUserName(),
                             GetProfileIdentifier(), u"*****");

  TriggerOnLoginEvent(GURL("https://www.example.com/"), u"login-username");
}

TEST_F(SafeBrowsingPrivateEventRouterTest,
       TestOnLoginEventNoMatchingUrlPattern) {
  SetUpRouters(
      /*authorized=*/true,
      /*realtime_reporting_enable=*/true,
      /*enabled_event_names=*/{},
      /*enabled_opt_in_events=*/{{"loginEvent", {"notexample.com"}}});

  signin::IdentityTestEnvironment identity_test_environment;
  enterprise_connectors::RealtimeReportingClientFactory::GetForProfile(profile_)
      ->SetIdentityManagerForTesting(
          identity_test_environment.identity_manager());
  identity_test_environment.MakePrimaryAccountAvailable(
      profile_->GetProfileUserName(), signin::ConsentLevel::kSignin);

  enterprise_connectors::test::EventReportValidator validator(client_.get());
  validator.ExpectNoReport();

  TriggerOnLoginEvent(GURL("https://www.example.com/"), u"login-username");
}

TEST_F(SafeBrowsingPrivateEventRouterTest,
       TestOnLoginEventWithEmailAsLoginUsername) {
  SetUpRouters(
      /*authorized=*/true,
      /*realtime_reporting_enable=*/true,
      /*enabled_event_names=*/{},
      /*enabled_opt_in_events=*/{{"loginEvent", {"*"}}});

  signin::IdentityTestEnvironment identity_test_environment;
  enterprise_connectors::RealtimeReportingClientFactory::GetForProfile(profile_)
      ->SetIdentityManagerForTesting(
          identity_test_environment.identity_manager());
  identity_test_environment.MakePrimaryAccountAvailable(
      profile_->GetProfileUserName(), signin::ConsentLevel::kSignin);

  enterprise_connectors::test::EventReportValidator validator(client_.get());
  validator.ExpectLoginEvent("https://www.example.com/", false, "",
                             profile_->GetProfileUserName(),
                             GetProfileIdentifier(), u"*****@example.com");

  TriggerOnLoginEvent(GURL("https://www.example.com/"),
                      u"login-username@example.com");
}

TEST_F(SafeBrowsingPrivateEventRouterTest, TestOnLoginEventFederated) {
  SetUpRouters(
      /*authorized=*/true,
      /*realtime_reporting_enable=*/true,
      /*enabled_event_names=*/{},
      /*enabled_opt_in_events=*/{{"loginEvent", {"*"}}});

  signin::IdentityTestEnvironment identity_test_environment;
  enterprise_connectors::RealtimeReportingClientFactory::GetForProfile(profile_)
      ->SetIdentityManagerForTesting(
          identity_test_environment.identity_manager());
  identity_test_environment.MakePrimaryAccountAvailable(
      profile_->GetProfileUserName(), signin::ConsentLevel::kSignin);

  enterprise_connectors::test::EventReportValidator validator(client_.get());
  validator.ExpectLoginEvent(
      "https://www.example.com/", true, "https://www.google.com",
      profile_->GetProfileUserName(), GetProfileIdentifier(), u"*****");

  TriggerOnLoginEvent(GURL("https://www.example.com/"), u"login-username",
                      url::SchemeHostPort(GURL("https://www.google.com")));
}

TEST_F(SafeBrowsingPrivateEventRouterTest, TestOnPasswordBreach) {
  SetUpRouters(
      /*authorized=*/true,
      /*realtime_reporting_enable=*/true,
      /*enabled_event_names=*/{},
      /*enabled_opt_in_events=*/{{"passwordBreachEvent", {"*"}}});

  signin::IdentityTestEnvironment identity_test_environment;
  enterprise_connectors::RealtimeReportingClientFactory::GetForProfile(profile_)
      ->SetIdentityManagerForTesting(
          identity_test_environment.identity_manager());
  identity_test_environment.MakePrimaryAccountAvailable(
      profile_->GetProfileUserName(), signin::ConsentLevel::kSignin);

  enterprise_connectors::test::EventReportValidator validator(client_.get());
  validator.ExpectPasswordBreachEvent(
      "SAFETY_CHECK",
      {
          {"https://first.example.com/", u"*****"},
          {"https://second.example.com/", u"*****@gmail.com"},
      },
      profile_->GetProfileUserName(), GetProfileIdentifier());

  TriggerOnPasswordBreachEvent(
      "SAFETY_CHECK",
      {
          {GURL("https://first.example.com"), u"first_user_name"},
          {GURL("https://second.example.com"), u"second_user_name@gmail.com"},
      });
}

TEST_F(SafeBrowsingPrivateEventRouterTest,
       TestOnPasswordBreachNoMatchingUrlPattern) {
  SetUpRouters(
      /*authorized=*/true,
      /*realtime_reporting_enable=*/true,
      /*enabled_event_names=*/{},
      /*enabled_opt_in_events=*/{{"passwordBreachEvent", {"notexample.com"}}});

  signin::IdentityTestEnvironment identity_test_environment;
  enterprise_connectors::RealtimeReportingClientFactory::GetForProfile(profile_)
      ->SetIdentityManagerForTesting(
          identity_test_environment.identity_manager());
  identity_test_environment.MakePrimaryAccountAvailable(
      profile_->GetProfileUserName(), signin::ConsentLevel::kSignin);

  enterprise_connectors::test::EventReportValidator validator(client_.get());
  validator.ExpectNoReport();

  TriggerOnPasswordBreachEvent(
      "SAFETY_CHECK",
      {
          {GURL("https://first.example.com"), u"first_user_name"},
          {GURL("https://second.example.com"), u"second_user_name"},
      });
}

TEST_F(SafeBrowsingPrivateEventRouterTest,
       TestOnPasswordBreachPartiallyMatchingUrlPatterns) {
  SetUpRouters(
      /*authorized=*/true,
      /*realtime_reporting_enable=*/true,
      /*enabled_event_names=*/{},
      /*enabled_opt_in_events=*/
      {{"passwordBreachEvent", {"secondexample.com"}}});

  signin::IdentityTestEnvironment identity_test_environment;
  enterprise_connectors::RealtimeReportingClientFactory::GetForProfile(profile_)
      ->SetIdentityManagerForTesting(
          identity_test_environment.identity_manager());
  identity_test_environment.MakePrimaryAccountAvailable(
      profile_->GetProfileUserName(), signin::ConsentLevel::kSignin);

  // The event is only enabled on secondexample.com, so expect only the
  // information related to that origin to be reported.
  enterprise_connectors::test::EventReportValidator validator(client_.get());
  validator.ExpectPasswordBreachEvent(
      "SAFETY_CHECK",
      {
          {"https://secondexample.com/", u"*****"},
      },
      profile_->GetProfileUserName(), GetProfileIdentifier());

  TriggerOnPasswordBreachEvent(
      "SAFETY_CHECK",
      {
          {GURL("https://firstexample.com"), u"first_user_name"},
          {GURL("https://secondexample.com"), u"second_user_name"},
      });
}

TEST_F(SafeBrowsingPrivateEventRouterTest, TestOnSensitiveDataEvent_Allowed) {
  SetUpRouters(/*authorized=*/true);

  base::Value::Dict report;
  EXPECT_CALL(*client_, UploadSecurityEventReport)
      .WillOnce(CaptureArg(&report));

  TriggerOnSensitiveDataEvent(safe_browsing::EventResult::ALLOWED);
  base::RunLoop().RunUntilIdle();

  Mock::VerifyAndClearExpectations(client_.get());
  const base::Value::List* event_list =
      report.FindList(policy::RealtimeReportingJobConfiguration::kEventListKey);
  ASSERT_NE(nullptr, event_list);
  ASSERT_EQ(1u, event_list->size());
  const base::Value::Dict& wrapper = (*event_list)[0].GetDict();
  const base::Value::Dict* event =
      wrapper.FindDict(enterprise_connectors::kKeySensitiveDataEvent);
  ASSERT_NE(nullptr, event);

  EXPECT_EQ(kUrl, *event->FindString(SafeBrowsingPrivateEventRouter::kKeyUrl));
  EXPECT_EQ(kTabUrl,
            *event->FindString(SafeBrowsingPrivateEventRouter::kKeyTabUrl));
  EXPECT_EQ(kSource,
            *event->FindString(SafeBrowsingPrivateEventRouter::kKeySource));
  EXPECT_EQ(kDestination, *event->FindString(
                              SafeBrowsingPrivateEventRouter::kKeyDestination));
  EXPECT_EQ("12345", *event->FindString(
                         SafeBrowsingPrivateEventRouter::kKeyContentSize));
  EXPECT_EQ("text/plain", *event->FindString(
                              SafeBrowsingPrivateEventRouter::kKeyContentType));
  EXPECT_EQ("sha256_of_data",
            *event->FindString(
                SafeBrowsingPrivateEventRouter::kKeyDownloadDigestSha256));
  EXPECT_EQ("sensitive_data.txt",
            *event->FindString(SafeBrowsingPrivateEventRouter::kKeyFileName));
  EXPECT_EQ(SafeBrowsingPrivateEventRouter::kTriggerFileUpload,
            *event->FindString(SafeBrowsingPrivateEventRouter::kKeyTrigger));

  const base::Value::List* triggered_rule_info =
      event->FindList(SafeBrowsingPrivateEventRouter::kKeyTriggeredRuleInfo);
  ASSERT_NE(nullptr, triggered_rule_info);
  ASSERT_EQ(1u, triggered_rule_info->size());
  const base::Value::Dict& triggered_rule = (*triggered_rule_info)[0].GetDict();
  EXPECT_EQ(
      safe_browsing::EventResultToString(safe_browsing::EventResult::ALLOWED),
      *event->FindString(SafeBrowsingPrivateEventRouter::kKeyEventResult));
  EXPECT_EQ("fake rule",
            *triggered_rule.FindString(
                SafeBrowsingPrivateEventRouter::kKeyTriggeredRuleName));
  EXPECT_EQ("test rule category",
            *triggered_rule.FindString(
                SafeBrowsingPrivateEventRouter::kKeyUrlCategory));
  EXPECT_EQ("scan_id",
            *event->FindString(SafeBrowsingPrivateEventRouter::kKeyScanId));
}

TEST_F(SafeBrowsingPrivateEventRouterTest, TestOnSensitiveDataEvent_Blocked) {
  SetUpRouters();

  base::Value::Dict report;
  EXPECT_CALL(*client_, UploadSecurityEventReport)
      .WillOnce(CaptureArg(&report));

  TriggerOnSensitiveDataEvent(safe_browsing::EventResult::BLOCKED);
  base::RunLoop().RunUntilIdle();

  Mock::VerifyAndClearExpectations(client_.get());
  const base::Value::List* event_list =
      report.FindList(policy::RealtimeReportingJobConfiguration::kEventListKey);
  ASSERT_NE(nullptr, event_list);
  ASSERT_EQ(1u, event_list->size());
  const base::Value::Dict& wrapper = (*event_list)[0].GetDict();
  const base::Value::Dict* event =
      wrapper.FindDict(enterprise_connectors::kKeySensitiveDataEvent);
  ASSERT_NE(nullptr, event);

  EXPECT_EQ(kUrl, *event->FindString(SafeBrowsingPrivateEventRouter::kKeyUrl));
  EXPECT_EQ(kTabUrl,
            *event->FindString(SafeBrowsingPrivateEventRouter::kKeyTabUrl));
  EXPECT_EQ(kSource,
            *event->FindString(SafeBrowsingPrivateEventRouter::kKeySource));
  EXPECT_EQ(kDestination, *event->FindString(
                              SafeBrowsingPrivateEventRouter::kKeyDestination));
  EXPECT_EQ("12345", *event->FindString(
                         SafeBrowsingPrivateEventRouter::kKeyContentSize));
  EXPECT_EQ("text/plain", *event->FindString(
                              SafeBrowsingPrivateEventRouter::kKeyContentType));
  EXPECT_EQ("sha256_of_data",
            *event->FindString(
                SafeBrowsingPrivateEventRouter::kKeyDownloadDigestSha256));
  EXPECT_EQ("sensitive_data.txt",
            *event->FindString(SafeBrowsingPrivateEventRouter::kKeyFileName));
  EXPECT_EQ(SafeBrowsingPrivateEventRouter::kTriggerFileUpload,
            *event->FindString(SafeBrowsingPrivateEventRouter::kKeyTrigger));

  const base::Value::List* triggered_rule_info =
      event->FindList(SafeBrowsingPrivateEventRouter::kKeyTriggeredRuleInfo);
  ASSERT_NE(nullptr, triggered_rule_info);
  ASSERT_EQ(1u, triggered_rule_info->size());
  const base::Value::Dict& triggered_rule = (*triggered_rule_info)[0].GetDict();
  EXPECT_EQ(
      safe_browsing::EventResultToString(safe_browsing::EventResult::BLOCKED),
      *event->FindString(SafeBrowsingPrivateEventRouter::kKeyEventResult));
  EXPECT_EQ("fake rule",
            *triggered_rule.FindString(
                SafeBrowsingPrivateEventRouter::kKeyTriggeredRuleName));
  EXPECT_EQ("test rule category",
            *triggered_rule.FindString(
                SafeBrowsingPrivateEventRouter::kKeyUrlCategory));
  EXPECT_EQ("scan_id",
            *event->FindString(SafeBrowsingPrivateEventRouter::kKeyScanId));
}

TEST_F(SafeBrowsingPrivateEventRouterTest,
       TestOnUrlFilteringInterstitial_Blocked) {
  SetUpRouters();

  base::Value::Dict report;
  EXPECT_CALL(*client_, UploadSecurityEventReport)
      .WillOnce(CaptureArg(&report));

  TriggerOnUrlFilteringInterstitial("ENTERPRISE_BLOCKED_SEEN", "");
  base::RunLoop().RunUntilIdle();

  Mock::VerifyAndClearExpectations(client_.get());
  const base::Value::List* event_list =
      report.FindList(policy::RealtimeReportingJobConfiguration::kEventListKey);
  ASSERT_NE(nullptr, event_list);
  ASSERT_EQ(1u, event_list->size());
  const base::Value::Dict& wrapper = (*event_list)[0].GetDict();
  const base::Value::Dict* event = wrapper.FindDict(
      enterprise_connectors::kKeyUrlFilteringInterstitialEvent);
  ASSERT_NE(nullptr, event);

  EXPECT_FALSE(
      *event->FindBool(SafeBrowsingPrivateEventRouter::kKeyClickedThrough));
  EXPECT_EQ("ENTERPRISE_BLOCKED_SEEN",
            *event->FindString(SafeBrowsingPrivateEventRouter::kKeyThreatType));

  const base::Value::List* triggered_rule_info =
      event->FindList(SafeBrowsingPrivateEventRouter::kKeyTriggeredRuleInfo);
  ASSERT_NE(nullptr, triggered_rule_info);
  ASSERT_EQ(1u, triggered_rule_info->size());
  const base::Value::Dict& triggered_rule = (*triggered_rule_info)[0].GetDict();
  EXPECT_EQ(
      safe_browsing::EventResultToString(safe_browsing::EventResult::BLOCKED),
      *event->FindString(SafeBrowsingPrivateEventRouter::kKeyEventResult));
  EXPECT_EQ("test rule name",
            *triggered_rule.FindString(
                SafeBrowsingPrivateEventRouter::kKeyTriggeredRuleName));
  EXPECT_FALSE(triggered_rule.FindBool(
      SafeBrowsingPrivateEventRouter::kKeyHasWatermarking));
}

TEST_F(SafeBrowsingPrivateEventRouterTest,
       TestOnUrlFilteringInterstitial_Warned) {
  SetUpRouters();

  base::Value::Dict report;
  EXPECT_CALL(*client_, UploadSecurityEventReport)
      .WillOnce(CaptureArg(&report));

  TriggerOnUrlFilteringInterstitial("ENTERPRISE_WARNED_SEEN",
                                    "watermark message");
  base::RunLoop().RunUntilIdle();

  Mock::VerifyAndClearExpectations(client_.get());
  const base::Value::List* event_list =
      report.FindList(policy::RealtimeReportingJobConfiguration::kEventListKey);
  ASSERT_NE(nullptr, event_list);
  ASSERT_EQ(1u, event_list->size());
  const base::Value::Dict& wrapper = (*event_list)[0].GetDict();
  const base::Value::Dict* event = wrapper.FindDict(
      enterprise_connectors::kKeyUrlFilteringInterstitialEvent);
  ASSERT_NE(nullptr, event);

  EXPECT_FALSE(
      *event->FindBool(SafeBrowsingPrivateEventRouter::kKeyClickedThrough));
  EXPECT_EQ("ENTERPRISE_WARNED_SEEN",
            *event->FindString(SafeBrowsingPrivateEventRouter::kKeyThreatType));

  const base::Value::List* triggered_rule_info =
      event->FindList(SafeBrowsingPrivateEventRouter::kKeyTriggeredRuleInfo);
  ASSERT_NE(nullptr, triggered_rule_info);
  ASSERT_EQ(1u, triggered_rule_info->size());
  const base::Value::Dict& triggered_rule = (*triggered_rule_info)[0].GetDict();
  EXPECT_EQ(
      safe_browsing::EventResultToString(safe_browsing::EventResult::WARNED),
      *event->FindString(SafeBrowsingPrivateEventRouter::kKeyEventResult));
  EXPECT_EQ("test rule name",
            *triggered_rule.FindString(
                SafeBrowsingPrivateEventRouter::kKeyTriggeredRuleName));
  EXPECT_TRUE(triggered_rule.FindBool(
      SafeBrowsingPrivateEventRouter::kKeyHasWatermarking));
}

TEST_F(SafeBrowsingPrivateEventRouterTest,
       TestOnUrlFilteringInterstitial_Bypassed) {
  SetUpRouters();

  base::Value::Dict report;
  EXPECT_CALL(*client_, UploadSecurityEventReport)
      .WillOnce(CaptureArg(&report));

  TriggerOnUrlFilteringInterstitial("ENTERPRISE_WARNED_BYPASS", "confidential");
  base::RunLoop().RunUntilIdle();

  Mock::VerifyAndClearExpectations(client_.get());
  const base::Value::List* event_list =
      report.FindList(policy::RealtimeReportingJobConfiguration::kEventListKey);
  ASSERT_NE(nullptr, event_list);
  ASSERT_EQ(1u, event_list->size());
  const base::Value::Dict& wrapper = (*event_list)[0].GetDict();
  const base::Value::Dict* event = wrapper.FindDict(
      enterprise_connectors::kKeyUrlFilteringInterstitialEvent);
  ASSERT_NE(nullptr, event);

  EXPECT_TRUE(
      *event->FindBool(SafeBrowsingPrivateEventRouter::kKeyClickedThrough));
  EXPECT_EQ("ENTERPRISE_WARNED_BYPASS",
            *event->FindString(SafeBrowsingPrivateEventRouter::kKeyThreatType));

  const base::Value::List* triggered_rule_info =
      event->FindList(SafeBrowsingPrivateEventRouter::kKeyTriggeredRuleInfo);
  ASSERT_NE(nullptr, triggered_rule_info);
  ASSERT_EQ(1u, triggered_rule_info->size());
  const base::Value::Dict& triggered_rule = (*triggered_rule_info)[0].GetDict();
  EXPECT_EQ(
      safe_browsing::EventResultToString(safe_browsing::EventResult::BYPASSED),
      *event->FindString(SafeBrowsingPrivateEventRouter::kKeyEventResult));
  EXPECT_EQ("test rule name",
            *triggered_rule.FindString(
                SafeBrowsingPrivateEventRouter::kKeyTriggeredRuleName));
  EXPECT_TRUE(triggered_rule.FindBool(
      SafeBrowsingPrivateEventRouter::kKeyHasWatermarking));
}

TEST_F(SafeBrowsingPrivateEventRouterTest,
       TestOnUrlFilteringInterstitial_WatermarkAudit) {
  SetUpRouters();

  base::Value::Dict report;
  EXPECT_CALL(*client_, UploadSecurityEventReport)
      .WillOnce(CaptureArg(&report));

  TriggerOnUrlFilteringInterstitial("", "watermark message");
  base::RunLoop().RunUntilIdle();

  Mock::VerifyAndClearExpectations(client_.get());
  const base::Value::List* event_list =
      report.FindList(policy::RealtimeReportingJobConfiguration::kEventListKey);
  ASSERT_NE(nullptr, event_list);
  ASSERT_EQ(1u, event_list->size());
  const base::Value::Dict& wrapper = (*event_list)[0].GetDict();
  const base::Value::Dict* event = wrapper.FindDict(
      enterprise_connectors::kKeyUrlFilteringInterstitialEvent);
  ASSERT_NE(nullptr, event);

  EXPECT_FALSE(
      *event->FindBool(SafeBrowsingPrivateEventRouter::kKeyClickedThrough));
  EXPECT_FALSE(
      event->FindString(SafeBrowsingPrivateEventRouter::kKeyThreatType));

  const base::Value::List* triggered_rule_info =
      event->FindList(SafeBrowsingPrivateEventRouter::kKeyTriggeredRuleInfo);
  ASSERT_NE(nullptr, triggered_rule_info);
  ASSERT_EQ(1u, triggered_rule_info->size());
  const base::Value::Dict& triggered_rule = (*triggered_rule_info)[0].GetDict();
  EXPECT_EQ(
      safe_browsing::EventResultToString(safe_browsing::EventResult::ALLOWED),
      *event->FindString(SafeBrowsingPrivateEventRouter::kKeyEventResult));
  EXPECT_EQ("test rule name",
            *triggered_rule.FindString(
                SafeBrowsingPrivateEventRouter::kKeyTriggeredRuleName));
  EXPECT_TRUE(triggered_rule.FindBool(
      SafeBrowsingPrivateEventRouter::kKeyHasWatermarking));
}

TEST_F(SafeBrowsingPrivateEventRouterTest, TestOnUnscannedFileEvent_Allowed) {
  SetUpRouters();

  base::Value::Dict report;
  EXPECT_CALL(*client_, UploadSecurityEventReport)
      .WillOnce(CaptureArg(&report));

  TriggerOnUnscannedFileEvent(safe_browsing::EventResult::ALLOWED);
  base::RunLoop().RunUntilIdle();

  Mock::VerifyAndClearExpectations(client_.get());
  const base::Value::List* event_list =
      report.FindList(policy::RealtimeReportingJobConfiguration::kEventListKey);
  ASSERT_NE(nullptr, event_list);
  ASSERT_EQ(1u, event_list->size());
  const base::Value::Dict& wrapper = (*event_list)[0].GetDict();
  const base::Value::Dict* event =
      wrapper.FindDict(enterprise_connectors::kKeyUnscannedFileEvent);
  ASSERT_NE(nullptr, event);

  EXPECT_EQ(kUrl, *event->FindString(SafeBrowsingPrivateEventRouter::kKeyUrl));
  EXPECT_EQ(kTabUrl,
            *event->FindString(SafeBrowsingPrivateEventRouter::kKeyTabUrl));
  EXPECT_EQ(kSource,
            *event->FindString(SafeBrowsingPrivateEventRouter::kKeySource));
  EXPECT_EQ(kDestination, *event->FindString(
                              SafeBrowsingPrivateEventRouter::kKeyDestination));
  EXPECT_EQ("12345", *event->FindString(
                         SafeBrowsingPrivateEventRouter::kKeyContentSize));
  EXPECT_EQ("text/plain", *event->FindString(
                              SafeBrowsingPrivateEventRouter::kKeyContentType));
  EXPECT_EQ("sha256_of_data",
            *event->FindString(
                SafeBrowsingPrivateEventRouter::kKeyDownloadDigestSha256));
  EXPECT_EQ("sensitive_data.txt",
            *event->FindString(SafeBrowsingPrivateEventRouter::kKeyFileName));
  EXPECT_EQ(SafeBrowsingPrivateEventRouter::kTriggerFileDownload,
            *event->FindString(SafeBrowsingPrivateEventRouter::kKeyTrigger));
  EXPECT_EQ(
      "filePasswordProtected",
      *event->FindString(SafeBrowsingPrivateEventRouter::kKeyUnscannedReason));
  EXPECT_EQ(
      EventResultToString(safe_browsing::EventResult::ALLOWED),
      *event->FindString(SafeBrowsingPrivateEventRouter::kKeyEventResult));
}

TEST_F(SafeBrowsingPrivateEventRouterTest, TestOnUnscannedFileEvent_Blocked) {
  SetUpRouters();

  base::Value::Dict report;
  EXPECT_CALL(*client_, UploadSecurityEventReport)
      .WillOnce(CaptureArg(&report));

  TriggerOnUnscannedFileEvent(safe_browsing::EventResult::BLOCKED);
  base::RunLoop().RunUntilIdle();

  Mock::VerifyAndClearExpectations(client_.get());
  const base::Value::List* event_list =
      report.FindList(policy::RealtimeReportingJobConfiguration::kEventListKey);
  ASSERT_NE(nullptr, event_list);
  ASSERT_EQ(1u, event_list->size());
  const base::Value::Dict& wrapper = (*event_list)[0].GetDict();
  const base::Value::Dict* event =
      wrapper.FindDict(enterprise_connectors::kKeyUnscannedFileEvent);
  ASSERT_NE(nullptr, event);

  EXPECT_EQ(kUrl, *event->FindString(SafeBrowsingPrivateEventRouter::kKeyUrl));
  EXPECT_EQ(kTabUrl,
            *event->FindString(SafeBrowsingPrivateEventRouter::kKeyTabUrl));
  EXPECT_EQ(kSource,
            *event->FindString(SafeBrowsingPrivateEventRouter::kKeySource));
  EXPECT_EQ(kDestination, *event->FindString(
                              SafeBrowsingPrivateEventRouter::kKeyDestination));
  EXPECT_EQ("12345", *event->FindString(
                         SafeBrowsingPrivateEventRouter::kKeyContentSize));
  EXPECT_EQ("text/plain", *event->FindString(
                              SafeBrowsingPrivateEventRouter::kKeyContentType));
  EXPECT_EQ("sha256_of_data",
            *event->FindString(
                SafeBrowsingPrivateEventRouter::kKeyDownloadDigestSha256));
  EXPECT_EQ("sensitive_data.txt",
            *event->FindString(SafeBrowsingPrivateEventRouter::kKeyFileName));
  EXPECT_EQ(SafeBrowsingPrivateEventRouter::kTriggerFileDownload,
            *event->FindString(SafeBrowsingPrivateEventRouter::kKeyTrigger));
  EXPECT_EQ(
      "filePasswordProtected",
      *event->FindString(SafeBrowsingPrivateEventRouter::kKeyUnscannedReason));
  EXPECT_EQ(
      EventResultToString(safe_browsing::EventResult::BLOCKED),
      *event->FindString(SafeBrowsingPrivateEventRouter::kKeyEventResult));
}

TEST_F(SafeBrowsingPrivateEventRouterTest, TestProfileUsername) {
  SetUpRouters();
  SafeBrowsingEventObserver event_observer(
      api::safe_browsing_private::OnSecurityInterstitialShown::kEventName);
  event_router_->AddEventObserver(&event_observer);

  signin::IdentityTestEnvironment identity_test_environment;
  enterprise_connectors::RealtimeReportingClientFactory::GetForProfile(profile_)
      ->SetIdentityManagerForTesting(
          identity_test_environment.identity_manager());

  EXPECT_CALL(*client_, UploadSecurityEventReport).WillRepeatedly(Return());

  // With no primary account, we should not set the username.
  TriggerOnSecurityInterstitialShownEvent();
  base::RunLoop().RunUntilIdle();
  base::Value::Dict captured_args =
      event_observer.PassEventArgs().GetList()[0].Clone().TakeDict();
  EXPECT_EQ("", CHECK_DEREF(captured_args.FindString("userName")));

  // With an unconsented primary account, we should set the username.
  identity_test_environment.MakePrimaryAccountAvailable(
      "profile@example.com", signin::ConsentLevel::kSignin);
  TriggerOnSecurityInterstitialShownEvent();
  base::RunLoop().RunUntilIdle();
  captured_args =
      event_observer.PassEventArgs().GetList()[0].Clone().TakeDict();
  EXPECT_EQ("profile@example.com",
            CHECK_DEREF(captured_args.FindString("userName")));

  // With a consented primary account, we should set the username.
  identity_test_environment.MakePrimaryAccountAvailable(
      "profile@example.com", signin::ConsentLevel::kSync);
  TriggerOnSecurityInterstitialShownEvent();
  base::RunLoop().RunUntilIdle();
  captured_args =
      event_observer.PassEventArgs().GetList()[0].Clone().TakeDict();
  EXPECT_EQ("profile@example.com",
            CHECK_DEREF(captured_args.FindString("userName")));
}

// This next series of tests validate that we get the expected number of events
// reported when a given event name is enabled and we only trigger the related
// events (some events like interstitial and dangerous downloads have multiple
// triggers for the same event name).
TEST_F(SafeBrowsingPrivateEventRouterTest, TestPasswordChangedEnabled) {
  std::set<std::string> enabled_event_names;
  enabled_event_names.insert(enterprise_connectors::kKeyPasswordChangedEvent);
  SetUpRouters(/*authorized=*/true, /*realtime_reporting_enable=*/true,
               enabled_event_names);

  SafeBrowsingEventObserver event_observer(
      api::safe_browsing_private::OnPolicySpecifiedPasswordChanged::kEventName);
  event_router_->AddEventObserver(&event_observer);

  EXPECT_CALL(*client_, UploadSecurityEventReport).Times(1);
  TriggerOnPolicySpecifiedPasswordChangedEvent();
  base::RunLoop().RunUntilIdle();

  // Assert the event actually did fire.
  ASSERT_EQ(1u, event_observer.PassEventArgs().GetList().size());

  // Make sure UploadSecurityEventReport was called the expected number of
  // times.
  Mock::VerifyAndClearExpectations(client_.get());
}

TEST_F(SafeBrowsingPrivateEventRouterTest, TestPasswordReuseEnabled) {
  std::set<std::string> enabled_event_names;
  enabled_event_names.insert(enterprise_connectors::kKeyPasswordReuseEvent);
  SetUpRouters(/*authorized=*/true, /*realtime_reporting_enable=*/true,
               enabled_event_names);

  SafeBrowsingEventObserver event_observer(
      api::safe_browsing_private::OnPolicySpecifiedPasswordReuseDetected::
          kEventName);
  event_router_->AddEventObserver(&event_observer);

  EXPECT_CALL(*client_, UploadSecurityEventReport).Times(1);
  TriggerOnPolicySpecifiedPasswordReuseDetectedEvent(/*warning_shown*/ true);
  base::RunLoop().RunUntilIdle();

  // Assert the event actually did fire.
  ASSERT_EQ(1u, event_observer.PassEventArgs().GetList().size());

  // Make sure UploadSecurityEventReport was called the expected number of
  // times.
  Mock::VerifyAndClearExpectations(client_.get());
}

TEST_F(SafeBrowsingPrivateEventRouterTest, TestDangerousDownloadEnabled) {
  std::set<std::string> enabled_event_names;
  enabled_event_names.insert(enterprise_connectors::kKeyDangerousDownloadEvent);
  SetUpRouters(/*authorized=*/true, /*realtime_reporting_enable=*/true,
               enabled_event_names);

  SafeBrowsingEventObserver event_observer(
      api::safe_browsing_private::OnDangerousDownloadOpened::kEventName);
  event_router_->AddEventObserver(&event_observer);

  EXPECT_CALL(*client_, UploadSecurityEventReport).Times(3);
  TriggerOnDangerousDownloadEvent();
  TriggerOnDangerousDownloadEventBypass();
  TriggerOnDangerousDownloadOpenedEvent();
  base::RunLoop().RunUntilIdle();

  // Assert the event actually did fire.
  ASSERT_EQ(1u, event_observer.PassEventArgs().GetList().size());

  // Make sure UploadSecurityEventReport was called the expected number of
  // times.
  Mock::VerifyAndClearExpectations(client_.get());
}

TEST_F(SafeBrowsingPrivateEventRouterTest, TestInterstitialEnabled) {
  std::set<std::string> enabled_event_names;
  enabled_event_names.insert(enterprise_connectors::kKeyInterstitialEvent);
  SetUpRouters(/*authorized=*/true, /*realtime_reporting_enable=*/true,
               enabled_event_names);

  SafeBrowsingEventObserver event_observer1(
      api::safe_browsing_private::OnSecurityInterstitialShown::kEventName);
  SafeBrowsingEventObserver event_observer2(
      api::safe_browsing_private::OnSecurityInterstitialProceeded::kEventName);
  event_router_->AddEventObserver(&event_observer1);
  event_router_->AddEventObserver(&event_observer2);

  EXPECT_CALL(*client_, UploadSecurityEventReport).Times(2);
  TriggerOnSecurityInterstitialShownEvent();
  TriggerOnSecurityInterstitialProceededEvent();
  base::RunLoop().RunUntilIdle();

  // Assert the event actually did fire.
  ASSERT_EQ(1u, event_observer1.PassEventArgs().GetList().size());
  ASSERT_EQ(1u, event_observer2.PassEventArgs().GetList().size());

  // Make sure UploadSecurityEventReport was called the expected number of
  // times.
  Mock::VerifyAndClearExpectations(client_.get());
}

TEST_F(SafeBrowsingPrivateEventRouterTest, TestSensitiveDataEnabled) {
  std::set<std::string> enabled_event_names;
  enabled_event_names.insert(enterprise_connectors::kKeySensitiveDataEvent);
  SetUpRouters(/*authorized=*/true, /*realtime_reporting_enable=*/true,
               enabled_event_names);

  EXPECT_CALL(*client_, UploadSecurityEventReport).Times(1);
  TriggerOnSensitiveDataEvent(safe_browsing::EventResult::BLOCKED);
  base::RunLoop().RunUntilIdle();

  // Make sure UploadSecurityEventReport was called the expected number of
  // times.
  Mock::VerifyAndClearExpectations(client_.get());
}

TEST_F(SafeBrowsingPrivateEventRouterTest, TestUnscannedFileEnabled) {
  std::set<std::string> enabled_event_names;
  enabled_event_names.insert(enterprise_connectors::kKeyUnscannedFileEvent);
  SetUpRouters(/*authorized=*/true, /*realtime_reporting_enable=*/true,
               enabled_event_names);

  EXPECT_CALL(*client_, UploadSecurityEventReport).Times(1);
  TriggerOnUnscannedFileEvent(safe_browsing::EventResult::ALLOWED);
  base::RunLoop().RunUntilIdle();

  // Make sure UploadSecurityEventReport was called the expected number of
  // times.
  Mock::VerifyAndClearExpectations(client_.get());
}

#if BUILDFLAG(ENTERPRISE_DATA_CONTROLS)
TEST_F(SafeBrowsingPrivateEventRouterTest, TestDataControlsSensitiveDataEvent) {
  SetUpRouters();

  base::Value::Dict report;
  EXPECT_CALL(*client_, UploadSecurityEventReport)
      .WillOnce(CaptureArg(&report));

  TriggerOnDataControlsSensitiveDataEvent({
      {0, {"rule_id_1", "rule_name_1"}},
      {1, {"rule_id_2", "rule_name_2"}},
  });
  base::RunLoop().RunUntilIdle();

  Mock::VerifyAndClearExpectations(client_.get());
  const base::Value::List* event_list =
      report.FindList(policy::RealtimeReportingJobConfiguration::kEventListKey);
  ASSERT_NE(event_list, nullptr);
  ASSERT_EQ(event_list->size(), 1u);
  const base::Value::Dict& wrapper = (*event_list)[0].GetDict();
  const base::Value::Dict* event =
      wrapper.FindDict(enterprise_connectors::kKeySensitiveDataEvent);
  ASSERT_NE(event, nullptr);

  EXPECT_EQ(*event->FindString(SafeBrowsingPrivateEventRouter::kKeyUrl), kUrl);
  EXPECT_EQ(*event->FindString(SafeBrowsingPrivateEventRouter::kKeyTabUrl),
            kTabUrl);
  EXPECT_EQ(*event->FindString(SafeBrowsingPrivateEventRouter::kKeySource),
            kSource);
  EXPECT_EQ(*event->FindString(SafeBrowsingPrivateEventRouter::kKeyDestination),
            kDestination);
  EXPECT_EQ(*event->FindString(SafeBrowsingPrivateEventRouter::kKeyContentSize),
            "12345");
  EXPECT_EQ(*event->FindString(SafeBrowsingPrivateEventRouter::kKeyContentType),
            "text/plain");
  EXPECT_EQ(*event->FindString(SafeBrowsingPrivateEventRouter::kKeyTrigger),
            SafeBrowsingPrivateEventRouter::kTriggerWebContentUpload);
  EXPECT_EQ(
      *event->FindString(SafeBrowsingPrivateEventRouter::kKeyEventResult),
      safe_browsing::EventResultToString(safe_browsing::EventResult::BLOCKED));

  const base::Value::List* triggered_rule_info =
      event->FindList(SafeBrowsingPrivateEventRouter::kKeyTriggeredRuleInfo);
  ASSERT_NE(triggered_rule_info, nullptr);
  ASSERT_EQ(triggered_rule_info->size(), 2u);
  const base::Value::Dict& rule_1 = (*triggered_rule_info)[0].GetDict();
  EXPECT_EQ(
      *rule_1.FindString(SafeBrowsingPrivateEventRouter::kKeyTriggeredRuleName),
      "rule_name_1");
  EXPECT_EQ(
      *rule_1.FindString(SafeBrowsingPrivateEventRouter::kKeyTriggeredRuleId),
      "rule_id_1");
  const base::Value::Dict& rule_2 = (*triggered_rule_info)[1].GetDict();
  EXPECT_EQ(
      *rule_2.FindString(SafeBrowsingPrivateEventRouter::kKeyTriggeredRuleName),
      "rule_name_2");
  EXPECT_EQ(
      *rule_2.FindString(SafeBrowsingPrivateEventRouter::kKeyTriggeredRuleId),
      "rule_id_2");
}
#endif  // BUILDFLAG(ENTERPRISE_DATA_CONTROLS)

// Tests to make sure the feature flag and policy control real-time reporting
// as expected.  The parameter for these tests is a tuple of bools:
//
//   bool: whether the feature flag is enabled.
//   bool: whether the browser is manageable.
//   bool: whether the policy is enabled.
//   bool: whether the server has authorized this browser instance.
class SafeBrowsingIsRealtimeReportingEnabledTest
    : public SafeBrowsingPrivateEventRouterTestBase,
      public testing::WithParamInterface<testing::tuple<bool, bool, bool>> {
 public:
  SafeBrowsingIsRealtimeReportingEnabledTest()
      : is_manageable_(testing::get<0>(GetParam())),
        is_policy_enabled_(testing::get<1>(GetParam())),
        is_authorized_(testing::get<2>(GetParam())) {
    // In chrome branded desktop builds, the browser is always manageable.
#if !BUILDFLAG(GOOGLE_CHROME_BRANDING) && !BUILDFLAG(IS_CHROMEOS_ASH)
    if (is_manageable_) {
      base::CommandLine::ForCurrentProcess()->AppendSwitch(
          switches::kEnableChromeBrowserCloudManagement);
    }
#endif
  }

  void SetUp() override {
    SafeBrowsingPrivateEventRouterTestBase::SetUp();
    if (is_policy_enabled_) {
      profile_->GetPrefs()->Set(enterprise_connectors::kOnSecurityEventPref,
                                *base::JSONReader::Read(kConnectorsPrefValue));
    }

#if BUILDFLAG(IS_CHROMEOS_ASH)
    user_manager_.Reset(std::make_unique<ash::FakeChromeUserManager>());
    const AccountId account_id(
        AccountId::FromUserEmail(profile_->GetProfileUserName()));
    const user_manager::User* user = user_manager_->AddUserWithAffiliation(
        account_id, /*is_affiliated=*/is_manageable_);
    ash::ProfileHelper::Get()->SetUserToProfileMappingForTesting(user,
                                                                 profile_);
    user_manager_->UserLoggedIn(account_id, user->username_hash(),
                                /*browser_restart=*/false,
                                /*is_child=*/false);
    profile_->ScopedCrosSettingsTestHelper()
        ->InstallAttributes()
        ->SetCloudManaged("domain.com", "device_id");
#endif
  }

 protected:
  base::test::ScopedFeatureList scoped_feature_list_;
  const bool is_manageable_;
  const bool is_policy_enabled_;
  const bool is_authorized_;

#if BUILDFLAG(IS_CHROMEOS_ASH)
 private:
  user_manager::TypedScopedUserManager<ash::FakeChromeUserManager>
      user_manager_;
#endif
};

TEST_P(SafeBrowsingIsRealtimeReportingEnabledTest, CheckRealtimeReport) {
  // In production, the router won't actually be authorized unless it was
  // initialized.  The second argument to SetUpRouters() takes this into
  // account.
  SetUpRouters(is_authorized_, is_policy_enabled_);
  SafeBrowsingEventObserver event_observer(
      api::safe_browsing_private::OnPolicySpecifiedPasswordChanged::kEventName);
  event_router_->AddEventObserver(&event_observer);

  bool should_report = is_policy_enabled_ && is_authorized_;

  if (should_report) {
    EXPECT_CALL(*client_, UploadSecurityEventReport).Times(1);
  } else if (client_) {
    // Because the test will crate a |client_| object when the policy is
    // set, even if the feature flag or other conditions indicate that
    // reports should not be sent, it is possible that the pointer is not
    // null. In this case, make sure UploadSecurityEventReport() is not called.
    EXPECT_CALL(*client_, UploadSecurityEventReport).Times(0);
  }

  TriggerOnPolicySpecifiedPasswordChangedEvent();
  base::RunLoop().RunUntilIdle();

  // Assert the trigger actually did fire.
  EXPECT_EQ(1u, event_observer.PassEventArgs().GetList().size());

  // Make sure UploadSecurityEventReport was called the expected number of
  // times.
  if (client_) {
    Mock::VerifyAndClearExpectations(client_.get());
  }
}

INSTANTIATE_TEST_SUITE_P(All,
                         SafeBrowsingIsRealtimeReportingEnabledTest,
                         testing::Combine(testing::Bool(),
                                          testing::Bool(),
                                          testing::Bool()));

// Tests to make sure only the enabled events are reported.
//
//   std::string: the name of the event to enable.
//   int: How many triggers use this event name.
class SafeBrowsingIsRealtimeReportingEventDisabledTest
    : public SafeBrowsingPrivateEventRouterTestBase,
      public testing::WithParamInterface<testing::tuple<std::string, int>> {
 public:
  SafeBrowsingIsRealtimeReportingEventDisabledTest()
      : event_name_(testing::get<0>(GetParam())),
        num_triggers_(testing::get<1>(GetParam())) {}

 protected:
  const std::string event_name_;
  const int num_triggers_;
};

// Tests above confirm the 1:n relation between enabled event name and expected
// triggers, when only these triggers are fired. Here we make sure none of the
// unexpected events are enabled when we trigger all of them.
TEST_P(SafeBrowsingIsRealtimeReportingEventDisabledTest,
       TryAllButOnlyTriggerExpectedNumberOfTimesForGivenEvent) {
  std::set<std::string> enabled_event_names;
  enabled_event_names.insert(event_name_);
  SetUpRouters(/*authorized=*/true, /*realtime_reporting_enable=*/true,
               enabled_event_names);

  SafeBrowsingEventObserver event_observer1(
      api::safe_browsing_private::OnPolicySpecifiedPasswordChanged::kEventName);
  SafeBrowsingEventObserver event_observer2(
      api::safe_browsing_private::OnPolicySpecifiedPasswordReuseDetected::
          kEventName);
  SafeBrowsingEventObserver event_observer3(
      api::safe_browsing_private::OnDangerousDownloadOpened::kEventName);
  SafeBrowsingEventObserver event_observer4(
      api::safe_browsing_private::OnSecurityInterstitialShown::kEventName);
  SafeBrowsingEventObserver event_observer5(
      api::safe_browsing_private::OnSecurityInterstitialProceeded::kEventName);
  event_router_->AddEventObserver(&event_observer1);
  event_router_->AddEventObserver(&event_observer2);
  event_router_->AddEventObserver(&event_observer3);
  event_router_->AddEventObserver(&event_observer4);
  event_router_->AddEventObserver(&event_observer5);

  // Only 1 of the 9 triggers should make it to an upload.
  EXPECT_CALL(*client_, UploadSecurityEventReport).Times(num_triggers_);
  TriggerOnPolicySpecifiedPasswordChangedEvent();
  TriggerOnPolicySpecifiedPasswordReuseDetectedEvent(/*warning_shown*/ true);
  TriggerOnDangerousDownloadOpenedEvent();
  TriggerOnSecurityInterstitialShownEvent();
  TriggerOnSecurityInterstitialProceededEvent();
  TriggerOnDangerousDownloadEvent();
  TriggerOnDangerousDownloadEventBypass();
  TriggerOnSensitiveDataEvent(safe_browsing::EventResult::BLOCKED);
  TriggerOnUnscannedFileEvent(safe_browsing::EventResult::ALLOWED);

  base::RunLoop().RunUntilIdle();

  // Assert the events with triggers actually did fire.
  EXPECT_EQ(1u, event_observer1.PassEventArgs().GetList().size());
  EXPECT_EQ(1u, event_observer2.PassEventArgs().GetList().size());
  EXPECT_EQ(1u, event_observer3.PassEventArgs().GetList().size());
  EXPECT_EQ(1u, event_observer4.PassEventArgs().GetList().size());
  EXPECT_EQ(1u, event_observer5.PassEventArgs().GetList().size());

  // Make sure UploadSecurityEventReport was called the expected number of
  // times.
  Mock::VerifyAndClearExpectations(client_.get());
}

INSTANTIATE_TEST_SUITE_P(
    All,
    SafeBrowsingIsRealtimeReportingEventDisabledTest,
    testing::Values(
        testing::make_tuple(enterprise_connectors::kKeyPasswordChangedEvent, 1),
        testing::make_tuple(enterprise_connectors::kKeyPasswordReuseEvent, 1),
        testing::make_tuple(enterprise_connectors::kKeyDangerousDownloadEvent,
                            3),
        testing::make_tuple(enterprise_connectors::kKeyInterstitialEvent, 2),
        testing::make_tuple(enterprise_connectors::kKeySensitiveDataEvent, 1),
        testing::make_tuple(enterprise_connectors::kKeyUnscannedFileEvent, 1)));

}  // namespace extensions
