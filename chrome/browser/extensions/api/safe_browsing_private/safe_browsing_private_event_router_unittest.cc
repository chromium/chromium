// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
#include "chrome/browser/extensions/api/safe_browsing_private/safe_browsing_private_event_router.h"

#include <memory>
#include <set>
#include <string>
#include <utility>

#include "base/bind.h"
#include "base/json/json_reader.h"
#include "base/strings/utf_string_conversions.h"
#include "base/test/mock_callback.h"
#include "base/test/scoped_feature_list.h"
#include "base/values.h"
#include "build/branding_buildflags.h"
#include "build/build_config.h"
#include "build/chromeos_buildflags.h"
#include "chrome/browser/enterprise/connectors/common.h"
#include "chrome/browser/enterprise/connectors/connectors_prefs.h"
#include "chrome/browser/enterprise/connectors/connectors_service.h"
#include "chrome/browser/extensions/api/safe_browsing_private/safe_browsing_private_event_router_factory.h"
#include "chrome/browser/policy/dm_token_utils.h"
#include "chrome/browser/safe_browsing/cloud_content_scanning/deep_scanning_test_utils.h"
#include "chrome/browser/safe_browsing/cloud_content_scanning/deep_scanning_utils.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/common/extensions/api/safe_browsing_private.h"
#include "chrome/test/base/testing_browser_process.h"
#include "chrome/test/base/testing_profile.h"
#include "chrome/test/base/testing_profile_manager.h"
#include "components/download/public/common/download_danger_type.h"
#include "components/enterprise/browser/enterprise_switches.h"
#include "components/enterprise/common/proto/connectors.pb.h"
#include "components/policy/core/common/cloud/mock_cloud_policy_client.h"
#include "components/policy/core/common/cloud/realtime_reporting_job_configuration.h"
#include "components/safe_browsing/core/common/safe_browsing_prefs.h"
#include "components/safe_browsing/core/features.h"
#include "components/signin/public/identity_manager/identity_test_environment.h"
#include "content/public/test/browser_task_environment.h"
#include "extensions/browser/test_event_router.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

#if BUILDFLAG(IS_CHROMEOS_ASH)
#include "chrome/browser/ash/login/users/fake_chrome_user_manager.h"
#include "chrome/browser/ash/profiles/profile_helper.h"
#include "chrome/browser/ash/settings/scoped_cros_settings_test_helper.h"
#include "chromeos/tpm/stub_install_attributes.h"
#include "components/account_id/account_id.h"
#include "components/user_manager/scoped_user_manager.h"
#include "components/user_manager/user.h"
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
  *wrapper = arg2.Clone();
}

constexpr char kConnectorsPrefValue[] = R"([
  {
    "service_provider": "google"
  }
])";

}  // namespace

class SafeBrowsingEventObserver : public TestEventRouter::EventObserver {
 public:
  // The observer will only listen to events with the |event_name|.
  explicit SafeBrowsingEventObserver(const std::string& event_name)
      : event_name_(event_name) {}

  ~SafeBrowsingEventObserver() override = default;

  // Removes |event_args_| from |*this| and returns them.
  base::Value PassEventArgs() { return std::move(event_args_); }

  // extensions::TestEventRouter::EventObserver:
  void OnBroadcastEvent(const extensions::Event& event) override {
    if (event.event_name == event_name_) {
      event_args_ = event.event_args->Clone();
    }
  }

 private:
  // The name of the observed event.
  const std::string event_name_;

  // The arguments passed for the last observed event.
  base::Value event_args_;

  DISALLOW_COPY_AND_ASSIGN(SafeBrowsingEventObserver);
};

std::unique_ptr<KeyedService> BuildSafeBrowsingPrivateEventRouter(
    content::BrowserContext* context) {
  return std::unique_ptr<KeyedService>(
      new SafeBrowsingPrivateEventRouter(context));
}

class SafeBrowsingPrivateEventRouterTest : public testing::Test {
 public:
  SafeBrowsingPrivateEventRouterTest()
      : profile_manager_(TestingBrowserProcess::GetGlobal()) {
    EXPECT_TRUE(profile_manager_.SetUp());
    profile_ = profile_manager_.CreateTestingProfile("test-user");
    policy::SetDMTokenForTesting(
        policy::DMToken::CreateValidTokenForTesting("fake-token"));
  }

  ~SafeBrowsingPrivateEventRouterTest() override = default;

  void TriggerOnPolicySpecifiedPasswordReuseDetectedEvent() {
    SafeBrowsingPrivateEventRouterFactory::GetForProfile(profile_)
        ->OnPolicySpecifiedPasswordReuseDetected(GURL("https://phishing.com/"),
                                                 "user_name_1",
                                                 /*is_phishing_url*/ true);
  }

  void TriggerOnPolicySpecifiedPasswordChangedEvent() {
    SafeBrowsingPrivateEventRouterFactory::GetForProfile(profile_)
        ->OnPolicySpecifiedPasswordChanged("user_name_2");
  }

  void TriggerOnDangerousDownloadOpenedEvent() {
    SafeBrowsingPrivateEventRouterFactory::GetForProfile(profile_)
        ->OnDangerousDownloadOpened(
            GURL("https://evil.com/malware.exe"), "/path/to/malware.exe",
            "sha256_of_malware_exe", "exe",
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
            GURL("https://maybevil.com/warning.exe"), "/path/to/warning.exe",
            "sha256_of_warning_exe", "POTENTIALLY_UNWANTED", "exe", 567,
            safe_browsing::EventResult::WARNED);
  }

  void TriggerOnDangerousDownloadEventBypass() {
    SafeBrowsingPrivateEventRouterFactory::GetForProfile(profile_)
        ->OnDangerousDownloadWarningBypassed(
            GURL("https://bypassevil.com/bypass.exe"), "/path/to/bypass.exe",
            "sha256_of_bypass_exe", "BYPASSED_WARNING", "exe", 890);
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

    SafeBrowsingPrivateEventRouterFactory::GetForProfile(profile_)
        ->OnAnalysisConnectorResult(
            GURL("https://evil.com/sensitive_data.txt"), "sensitive_data.txt",
            "sha256_of_data", "text/plain",
            SafeBrowsingPrivateEventRouter::kTriggerFileUpload,
            safe_browsing::DeepScanAccessPoint::UPLOAD, result, 12345,
            event_result);
  }

  void TriggerOnUnscannedFileEvent(safe_browsing::EventResult result) {
    SafeBrowsingPrivateEventRouterFactory::GetForProfile(profile_)
        ->OnUnscannedFileEvent(
            GURL("https://evil.com/sensitive_data.txt"), "sensitive_data.txt",
            "sha256_of_data", "text/plain",
            SafeBrowsingPrivateEventRouter::kTriggerFileDownload,
            safe_browsing::DeepScanAccessPoint::DOWNLOAD,
            "filePasswordProtected", 12345, result);
  }

  void SetReportingPolicy(bool enabled,
                          bool authorized = true,
                          const std::set<std::string>& enabled_event_names =
                              std::set<std::string>()) {
    safe_browsing::SetOnSecurityEventReporting(profile_->GetPrefs(), enabled,
                                               enabled_event_names);

    // If we are not enabling reporting, or if the client has already been
    // set for testing, just return.
    if (!enabled)
      return;

    if (client_ == nullptr) {
      // Set a mock cloud policy client in the router.
      client_ = std::make_unique<policy::MockCloudPolicyClient>();
      client_->SetDMToken("fake-token");
      SafeBrowsingPrivateEventRouterFactory::GetForProfile(profile_)
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
                        std::set<std::string>()) {
    event_router_ = extensions::CreateAndUseTestEventRouter(profile_);
    SafeBrowsingPrivateEventRouterFactory::GetInstance()->SetTestingFactory(
        profile_, base::BindRepeating(&BuildSafeBrowsingPrivateEventRouter));

    SetReportingPolicy(realtime_reporting_enable, authorized,
                       enabled_event_names);
  }

 protected:
  content::BrowserTaskEnvironment task_environment_;
  std::unique_ptr<policy::MockCloudPolicyClient> client_;
  TestingProfileManager profile_manager_;
  TestingProfile* profile_;
  extensions::TestEventRouter* event_router_ = nullptr;

 private:
#if !BUILDFLAG(IS_CHROMEOS_ASH)
  policy::FakeBrowserDMTokenStorage dm_token_storage_;
#endif  // !BUILDFLAG(IS_CHROMEOS_ASH)

  DISALLOW_COPY_AND_ASSIGN(SafeBrowsingPrivateEventRouterTest);
};

TEST_F(SafeBrowsingPrivateEventRouterTest, TestOnReuseDetected) {
  SetUpRouters();
  SafeBrowsingEventObserver event_observer(
      api::safe_browsing_private::OnPolicySpecifiedPasswordReuseDetected::
          kEventName);
  event_router_->AddEventObserver(&event_observer);

  base::Value report;
  EXPECT_CALL(*client_, UploadSecurityEventReport_(_, _, _, _))
      .WillOnce(CaptureArg(&report));

  TriggerOnPolicySpecifiedPasswordReuseDetectedEvent();
  base::RunLoop().RunUntilIdle();

  auto captured_args = event_observer.PassEventArgs().GetList()[0].Clone();
  EXPECT_EQ("https://phishing.com/", captured_args.FindKey("url")->GetString());
  EXPECT_EQ("user_name_1", captured_args.FindKey("userName")->GetString());

  Mock::VerifyAndClearExpectations(client_.get());
  EXPECT_EQ(base::Value::Type::DICTIONARY, report.type());
  base::Value* event_list =
      report.FindKey(policy::RealtimeReportingJobConfiguration::kEventListKey);
  ASSERT_NE(nullptr, event_list);
  EXPECT_EQ(base::Value::Type::LIST, event_list->type());
  base::Value::ListView mutable_list = event_list->GetList();
  ASSERT_EQ(1, (int)mutable_list.size());
  base::Value wrapper = std::move(mutable_list[0]);
  EXPECT_EQ(base::Value::Type::DICTIONARY, wrapper.type());
  base::Value* event =
      wrapper.FindKey(SafeBrowsingPrivateEventRouter::kKeyPasswordReuseEvent);
  EXPECT_NE(nullptr, event);
  EXPECT_EQ("https://phishing.com/",
            *event->FindStringKey(SafeBrowsingPrivateEventRouter::kKeyUrl));
  EXPECT_EQ("user_name_1", *event->FindStringKey(
                               SafeBrowsingPrivateEventRouter::kKeyUserName));
}

TEST_F(SafeBrowsingPrivateEventRouterTest, TestOnPasswordChanged) {
  SetUpRouters();
  SafeBrowsingEventObserver event_observer(
      api::safe_browsing_private::OnPolicySpecifiedPasswordChanged::kEventName);
  event_router_->AddEventObserver(&event_observer);

  base::Value report;
  EXPECT_CALL(*client_, UploadSecurityEventReport_(_, _, _, _))
      .WillOnce(CaptureArg(&report));

  TriggerOnPolicySpecifiedPasswordChangedEvent();
  base::RunLoop().RunUntilIdle();

  auto captured_args = event_observer.PassEventArgs().GetList()[0].Clone();
  EXPECT_EQ("user_name_2", captured_args.GetString());

  Mock::VerifyAndClearExpectations(client_.get());
  EXPECT_EQ(base::Value::Type::DICTIONARY, report.type());
  base::Value* event_list =
      report.FindKey(policy::RealtimeReportingJobConfiguration::kEventListKey);
  ASSERT_NE(nullptr, event_list);
  EXPECT_EQ(base::Value::Type::LIST, event_list->type());
  base::Value::ListView mutable_list = event_list->GetList();
  ASSERT_EQ(1, (int)mutable_list.size());
  base::Value wrapper = std::move(mutable_list[0]);
  EXPECT_EQ(base::Value::Type::DICTIONARY, wrapper.type());
  base::Value* event =
      wrapper.FindKey(SafeBrowsingPrivateEventRouter::kKeyPasswordChangedEvent);
  EXPECT_NE(nullptr, event);
  EXPECT_EQ("user_name_2", *event->FindStringKey(
                               SafeBrowsingPrivateEventRouter::kKeyUserName));
}

TEST_F(SafeBrowsingPrivateEventRouterTest, TestOnDangerousDownloadOpened) {
  SetUpRouters();
  SafeBrowsingEventObserver event_observer(
      api::safe_browsing_private::OnDangerousDownloadOpened::kEventName);
  event_router_->AddEventObserver(&event_observer);

  base::Value report;
  EXPECT_CALL(*client_, UploadSecurityEventReport_(_, _, _, _))
      .WillOnce(CaptureArg(&report));

  TriggerOnDangerousDownloadOpenedEvent();
  base::RunLoop().RunUntilIdle();

  auto captured_args = event_observer.PassEventArgs().GetList()[0].Clone();
  EXPECT_EQ("https://evil.com/malware.exe",
            captured_args.FindKey("url")->GetString());
  EXPECT_EQ("/path/to/malware.exe",
            captured_args.FindKey("fileName")->GetString());
  EXPECT_EQ("", captured_args.FindKey("userName")->GetString());
  EXPECT_EQ("sha256_of_malware_exe",
            captured_args.FindKey("downloadDigestSha256")->GetString());

  Mock::VerifyAndClearExpectations(client_.get());
  EXPECT_EQ(base::Value::Type::DICTIONARY, report.type());
  base::Value* event_list =
      report.FindKey(policy::RealtimeReportingJobConfiguration::kEventListKey);
  ASSERT_NE(nullptr, event_list);
  EXPECT_EQ(base::Value::Type::LIST, event_list->type());
  base::Value::ListView mutable_list = event_list->GetList();
  ASSERT_EQ(1, (int)mutable_list.size());
  base::Value wrapper = std::move(mutable_list[0]);
  EXPECT_EQ(base::Value::Type::DICTIONARY, wrapper.type());
  base::Value* event = wrapper.FindKey(
      SafeBrowsingPrivateEventRouter::kKeyDangerousDownloadEvent);
  EXPECT_NE(nullptr, event);
  EXPECT_EQ(
      "/path/to/malware.exe",
      *event->FindStringKey(SafeBrowsingPrivateEventRouter::kKeyFileName));
  EXPECT_EQ("exe", *event->FindStringKey(
                       SafeBrowsingPrivateEventRouter::kKeyContentType));
  EXPECT_EQ(1234, *event->FindIntKey(
                      SafeBrowsingPrivateEventRouter::kKeyContentSize));
  EXPECT_EQ(SafeBrowsingPrivateEventRouter::kTriggerFileDownload,
            *event->FindStringKey(SafeBrowsingPrivateEventRouter::kKeyTrigger));
  EXPECT_EQ(
      safe_browsing::EventResultToString(safe_browsing::EventResult::BYPASSED),
      *event->FindStringKey(SafeBrowsingPrivateEventRouter::kKeyEventResult));
}

TEST_F(SafeBrowsingPrivateEventRouterTest,
       TestOnSecurityInterstitialProceeded) {
  SetUpRouters();
  SafeBrowsingEventObserver event_observer(
      api::safe_browsing_private::OnSecurityInterstitialProceeded::kEventName);
  event_router_->AddEventObserver(&event_observer);

  base::Value report;
  EXPECT_CALL(*client_, UploadSecurityEventReport_(_, _, _, _))
      .WillOnce(CaptureArg(&report));

  TriggerOnSecurityInterstitialProceededEvent();
  base::RunLoop().RunUntilIdle();

  auto captured_args = event_observer.PassEventArgs().GetList()[0].Clone();
  EXPECT_EQ("https://phishing.com/", captured_args.FindKey("url")->GetString());
  EXPECT_EQ("PHISHING", captured_args.FindKey("reason")->GetString());
  EXPECT_EQ("-201", captured_args.FindKey("netErrorCode")->GetString());
  EXPECT_EQ("", captured_args.FindKey("userName")->GetString());

  Mock::VerifyAndClearExpectations(client_.get());
  EXPECT_EQ(base::Value::Type::DICTIONARY, report.type());
  base::Value* event_list =
      report.FindKey(policy::RealtimeReportingJobConfiguration::kEventListKey);
  ASSERT_NE(nullptr, event_list);
  EXPECT_EQ(base::Value::Type::LIST, event_list->type());
  base::Value::ListView mutable_list = event_list->GetList();
  ASSERT_EQ(1, (int)mutable_list.size());
  base::Value wrapper = std::move(mutable_list[0]);
  EXPECT_EQ(base::Value::Type::DICTIONARY, wrapper.type());
  base::Value* event =
      wrapper.FindKey(SafeBrowsingPrivateEventRouter::kKeyInterstitialEvent);
  EXPECT_NE(nullptr, event);
  EXPECT_EQ("PHISHING",
            *event->FindStringKey(SafeBrowsingPrivateEventRouter::kKeyReason));
  EXPECT_EQ(-201, *event->FindIntKey(
                      SafeBrowsingPrivateEventRouter::kKeyNetErrorCode));
  EXPECT_TRUE(
      *event->FindBoolKey(SafeBrowsingPrivateEventRouter::kKeyClickedThrough));
}

TEST_F(SafeBrowsingPrivateEventRouterTest, TestOnSecurityInterstitialShown) {
  SetUpRouters();
  SafeBrowsingEventObserver event_observer(
      api::safe_browsing_private::OnSecurityInterstitialShown::kEventName);
  event_router_->AddEventObserver(&event_observer);

  base::Value report;
  EXPECT_CALL(*client_, UploadSecurityEventReport_(_, _, _, _))
      .WillOnce(CaptureArg(&report));

  TriggerOnSecurityInterstitialShownEvent();
  base::RunLoop().RunUntilIdle();

  auto captured_args = event_observer.PassEventArgs().GetList()[0].Clone();
  EXPECT_EQ("https://phishing.com/", captured_args.FindKey("url")->GetString());
  EXPECT_EQ("PHISHING", captured_args.FindKey("reason")->GetString());
  EXPECT_FALSE(captured_args.FindKey("netErrorCode"));
  EXPECT_EQ("", captured_args.FindKey("userName")->GetString());

  Mock::VerifyAndClearExpectations(client_.get());
  EXPECT_EQ(base::Value::Type::DICTIONARY, report.type());
  base::Value* event_list =
      report.FindKey(policy::RealtimeReportingJobConfiguration::kEventListKey);
  ASSERT_NE(nullptr, event_list);
  EXPECT_EQ(base::Value::Type::LIST, event_list->type());
  base::Value::ListView mutable_list = event_list->GetList();
  ASSERT_EQ(1, (int)mutable_list.size());
  base::Value wrapper = std::move(mutable_list[0]);
  EXPECT_EQ(base::Value::Type::DICTIONARY, wrapper.type());
  base::Value* event =
      wrapper.FindKey(SafeBrowsingPrivateEventRouter::kKeyInterstitialEvent);
  EXPECT_NE(nullptr, event);
  EXPECT_EQ("PHISHING",
            *event->FindStringKey(SafeBrowsingPrivateEventRouter::kKeyReason));
  EXPECT_EQ(
      0, *event->FindIntKey(SafeBrowsingPrivateEventRouter::kKeyNetErrorCode));
  EXPECT_FALSE(
      *event->FindBoolKey(SafeBrowsingPrivateEventRouter::kKeyClickedThrough));
}

TEST_F(SafeBrowsingPrivateEventRouterTest, TestOnDangerousDownloadWarning) {
  SetUpRouters();
  SafeBrowsingEventObserver event_observer(
      api::safe_browsing_private::OnDangerousDownloadOpened::kEventName);
  event_router_->AddEventObserver(&event_observer);

  base::Value report;
  EXPECT_CALL(*client_, UploadSecurityEventReport_(_, _, _, _))
      .WillOnce(CaptureArg(&report));

  TriggerOnDangerousDownloadEvent();
  base::RunLoop().RunUntilIdle();

  Mock::VerifyAndClearExpectations(client_.get());
  EXPECT_EQ(base::Value::Type::DICTIONARY, report.type());
  base::Value* event_list =
      report.FindKey(policy::RealtimeReportingJobConfiguration::kEventListKey);
  ASSERT_NE(nullptr, event_list);
  EXPECT_EQ(base::Value::Type::LIST, event_list->type());
  base::Value::ListView mutable_list = event_list->GetList();
  ASSERT_EQ(1, (int)mutable_list.size());
  base::Value wrapper = std::move(mutable_list[0]);
  EXPECT_EQ(base::Value::Type::DICTIONARY, wrapper.type());
  base::Value* event = wrapper.FindKey(
      SafeBrowsingPrivateEventRouter::kKeyDangerousDownloadEvent);
  EXPECT_NE(nullptr, event);
  EXPECT_EQ(
      "/path/to/warning.exe",
      *event->FindStringKey(SafeBrowsingPrivateEventRouter::kKeyFileName));
  EXPECT_EQ("exe", *event->FindStringKey(
                       SafeBrowsingPrivateEventRouter::kKeyContentType));
  EXPECT_EQ(
      567, *event->FindIntKey(SafeBrowsingPrivateEventRouter::kKeyContentSize));
  EXPECT_EQ(
      "POTENTIALLY_UNWANTED",
      *event->FindStringKey(SafeBrowsingPrivateEventRouter::kKeyThreatType));
  EXPECT_EQ(
      safe_browsing::EventResultToString(safe_browsing::EventResult::WARNED),
      *event->FindStringKey(SafeBrowsingPrivateEventRouter::kKeyEventResult));
}

TEST_F(SafeBrowsingPrivateEventRouterTest,
       TestOnDangerousDownloadWarningBypass) {
  SetUpRouters();
  SafeBrowsingEventObserver event_observer(
      api::safe_browsing_private::OnDangerousDownloadOpened::kEventName);
  event_router_->AddEventObserver(&event_observer);

  base::Value report;
  EXPECT_CALL(*client_, UploadSecurityEventReport_(_, _, _, _))
      .WillOnce(CaptureArg(&report));

  TriggerOnDangerousDownloadEventBypass();
  base::RunLoop().RunUntilIdle();

  Mock::VerifyAndClearExpectations(client_.get());
  EXPECT_EQ(base::Value::Type::DICTIONARY, report.type());
  base::Value* event_list =
      report.FindKey(policy::RealtimeReportingJobConfiguration::kEventListKey);
  ASSERT_NE(nullptr, event_list);
  EXPECT_EQ(base::Value::Type::LIST, event_list->type());
  base::Value::ListView mutable_list = event_list->GetList();
  ASSERT_EQ(1, (int)mutable_list.size());
  base::Value wrapper = std::move(mutable_list[0]);
  EXPECT_EQ(base::Value::Type::DICTIONARY, wrapper.type());
  base::Value* event = wrapper.FindKey(
      SafeBrowsingPrivateEventRouter::kKeyDangerousDownloadEvent);
  EXPECT_NE(nullptr, event);
  EXPECT_EQ(
      "/path/to/bypass.exe",
      *event->FindStringKey(SafeBrowsingPrivateEventRouter::kKeyFileName));
  EXPECT_EQ("exe", *event->FindStringKey(
                       SafeBrowsingPrivateEventRouter::kKeyContentType));
  EXPECT_EQ(
      890, *event->FindIntKey(SafeBrowsingPrivateEventRouter::kKeyContentSize));
  EXPECT_EQ(
      "BYPASSED_WARNING",
      *event->FindStringKey(SafeBrowsingPrivateEventRouter::kKeyThreatType));
  EXPECT_EQ(
      safe_browsing::EventResultToString(safe_browsing::EventResult::BYPASSED),
      *event->FindStringKey(SafeBrowsingPrivateEventRouter::kKeyEventResult));
}

TEST_F(SafeBrowsingPrivateEventRouterTest, PolicyControlOnToOffIsDynamic) {
  SetUpRouters();
  SafeBrowsingEventObserver event_observer(
      api::safe_browsing_private::OnSecurityInterstitialShown::kEventName);
  event_router_->AddEventObserver(&event_observer);

  EXPECT_CALL(*client_, UploadSecurityEventReport_(_, _, _, _)).Times(1);
  TriggerOnSecurityInterstitialShownEvent();
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(1u, event_observer.PassEventArgs().GetList().size());
  Mock::VerifyAndClearExpectations(client_.get());

  // Now turn off policy.  This time no report should be generated.
  SetReportingPolicy(false);
  EXPECT_CALL(*client_, UploadSecurityEventReport_(_, _, _, _)).Times(0);
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
  EXPECT_CALL(*client_, UploadSecurityEventReport_(_, _, _, _)).Times(1);
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
  EXPECT_CALL(*client_, UploadSecurityEventReport_(_, _, _, _)).Times(0);

  TriggerOnPolicySpecifiedPasswordReuseDetectedEvent();
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
  EXPECT_CALL(*client_, UploadSecurityEventReport_(_, _, _, _)).Times(0);

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
  EXPECT_CALL(*client_, UploadSecurityEventReport_(_, _, _, _)).Times(0);

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
  EXPECT_CALL(*client_, UploadSecurityEventReport_(_, _, _, _)).Times(0);

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
  EXPECT_CALL(*client_, UploadSecurityEventReport_(_, _, _, _)).Times(0);

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
  EXPECT_CALL(*client_, UploadSecurityEventReport_(_, _, _, _)).Times(0);

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
  EXPECT_CALL(*client_, UploadSecurityEventReport_(_, _, _, _)).Times(0);

  TriggerOnDangerousDownloadEventBypass();
  base::RunLoop().RunUntilIdle();

  Mock::VerifyAndClearExpectations(client_.get());
  EXPECT_EQ(base::Value::Type::NONE, report.type());
}

TEST_F(SafeBrowsingPrivateEventRouterTest, TestOnSensitiveDataEvent_Allowed) {
  SetUpRouters(/*authorized=*/true);

  base::Value report;
  EXPECT_CALL(*client_, UploadSecurityEventReport_(_, _, _, _))
      .WillOnce(CaptureArg(&report));

  TriggerOnSensitiveDataEvent(safe_browsing::EventResult::ALLOWED);
  base::RunLoop().RunUntilIdle();

  Mock::VerifyAndClearExpectations(client_.get());
  EXPECT_EQ(base::Value::Type::DICTIONARY, report.type());
  base::Value* event_list =
      report.FindKey(policy::RealtimeReportingJobConfiguration::kEventListKey);
  ASSERT_NE(nullptr, event_list);
  EXPECT_EQ(base::Value::Type::LIST, event_list->type());
  base::Value::ListView mutable_list = event_list->GetList();
  ASSERT_EQ(1, (int)mutable_list.size());
  base::Value wrapper = std::move(mutable_list[0]);
  EXPECT_EQ(base::Value::Type::DICTIONARY, wrapper.type());
  base::Value* event =
      wrapper.FindKey(SafeBrowsingPrivateEventRouter::kKeySensitiveDataEvent);
  ASSERT_NE(nullptr, event);

  EXPECT_EQ(12345, *event->FindIntKey(
                       SafeBrowsingPrivateEventRouter::kKeyContentSize));
  EXPECT_EQ("text/plain", *event->FindStringKey(
                              SafeBrowsingPrivateEventRouter::kKeyContentType));
  EXPECT_EQ("sha256_of_data",
            *event->FindStringKey(
                SafeBrowsingPrivateEventRouter::kKeyDownloadDigestSha256));
  EXPECT_EQ(
      "sensitive_data.txt",
      *event->FindStringKey(SafeBrowsingPrivateEventRouter::kKeyFileName));
  EXPECT_EQ(SafeBrowsingPrivateEventRouter::kTriggerFileUpload,
            *event->FindStringKey(SafeBrowsingPrivateEventRouter::kKeyTrigger));

  base::Value* triggered_rule_info =
      event->FindKey(SafeBrowsingPrivateEventRouter::kKeyTriggeredRuleInfo);
  ASSERT_NE(nullptr, triggered_rule_info);
  ASSERT_EQ(1u, triggered_rule_info->GetList().size());
  base::Value triggered_rule = std::move(triggered_rule_info->GetList()[0]);
  EXPECT_EQ(
      safe_browsing::EventResultToString(safe_browsing::EventResult::ALLOWED),
      *event->FindStringKey(SafeBrowsingPrivateEventRouter::kKeyEventResult));
  EXPECT_EQ("fake rule",
            *triggered_rule.FindStringKey(
                SafeBrowsingPrivateEventRouter::kKeyTriggeredRuleName));
}

TEST_F(SafeBrowsingPrivateEventRouterTest, TestOnSensitiveDataEvent_Blocked) {
  SetUpRouters();

  base::Value report;
  EXPECT_CALL(*client_, UploadSecurityEventReport_(_, _, _, _))
      .WillOnce(CaptureArg(&report));

  TriggerOnSensitiveDataEvent(safe_browsing::EventResult::BLOCKED);
  base::RunLoop().RunUntilIdle();

  Mock::VerifyAndClearExpectations(client_.get());
  EXPECT_EQ(base::Value::Type::DICTIONARY, report.type());
  base::Value* event_list =
      report.FindKey(policy::RealtimeReportingJobConfiguration::kEventListKey);
  ASSERT_NE(nullptr, event_list);
  EXPECT_EQ(base::Value::Type::LIST, event_list->type());
  base::Value::ListView mutable_list = event_list->GetList();
  ASSERT_EQ(1, (int)mutable_list.size());
  base::Value wrapper = std::move(mutable_list[0]);
  EXPECT_EQ(base::Value::Type::DICTIONARY, wrapper.type());
  base::Value* event =
      wrapper.FindKey(SafeBrowsingPrivateEventRouter::kKeySensitiveDataEvent);
  ASSERT_NE(nullptr, event);

  EXPECT_EQ(12345, *event->FindIntKey(
                       SafeBrowsingPrivateEventRouter::kKeyContentSize));
  EXPECT_EQ("text/plain", *event->FindStringKey(
                              SafeBrowsingPrivateEventRouter::kKeyContentType));
  EXPECT_EQ("sha256_of_data",
            *event->FindStringKey(
                SafeBrowsingPrivateEventRouter::kKeyDownloadDigestSha256));
  EXPECT_EQ(
      "sensitive_data.txt",
      *event->FindStringKey(SafeBrowsingPrivateEventRouter::kKeyFileName));
  EXPECT_EQ(SafeBrowsingPrivateEventRouter::kTriggerFileUpload,
            *event->FindStringKey(SafeBrowsingPrivateEventRouter::kKeyTrigger));

  base::Value* triggered_rule_info =
      event->FindKey(SafeBrowsingPrivateEventRouter::kKeyTriggeredRuleInfo);
  ASSERT_NE(nullptr, triggered_rule_info);
  ASSERT_EQ(1u, triggered_rule_info->GetList().size());
  base::Value triggered_rule = std::move(triggered_rule_info->GetList()[0]);
  EXPECT_EQ(
      safe_browsing::EventResultToString(safe_browsing::EventResult::BLOCKED),
      *event->FindStringKey(SafeBrowsingPrivateEventRouter::kKeyEventResult));
  EXPECT_EQ("fake rule",
            *triggered_rule.FindStringKey(
                SafeBrowsingPrivateEventRouter::kKeyTriggeredRuleName));
}

TEST_F(SafeBrowsingPrivateEventRouterTest, TestOnUnscannedFileEvent_Allowed) {
  SetUpRouters();

  base::Value report;
  EXPECT_CALL(*client_, UploadSecurityEventReport_(_, _, _, _))
      .WillOnce(CaptureArg(&report));

  TriggerOnUnscannedFileEvent(safe_browsing::EventResult::ALLOWED);
  base::RunLoop().RunUntilIdle();

  Mock::VerifyAndClearExpectations(client_.get());
  EXPECT_EQ(base::Value::Type::DICTIONARY, report.type());
  base::Value* event_list =
      report.FindKey(policy::RealtimeReportingJobConfiguration::kEventListKey);
  ASSERT_NE(nullptr, event_list);
  EXPECT_EQ(base::Value::Type::LIST, event_list->type());
  base::Value::ListView mutable_list = event_list->GetList();
  ASSERT_EQ(1, (int)mutable_list.size());
  base::Value wrapper = std::move(mutable_list[0]);
  EXPECT_EQ(base::Value::Type::DICTIONARY, wrapper.type());
  base::Value* event =
      wrapper.FindKey(SafeBrowsingPrivateEventRouter::kKeyUnscannedFileEvent);
  ASSERT_NE(nullptr, event);

  EXPECT_EQ(12345, *event->FindIntKey(
                       SafeBrowsingPrivateEventRouter::kKeyContentSize));
  EXPECT_EQ("text/plain", *event->FindStringKey(
                              SafeBrowsingPrivateEventRouter::kKeyContentType));
  EXPECT_EQ("sha256_of_data",
            *event->FindStringKey(
                SafeBrowsingPrivateEventRouter::kKeyDownloadDigestSha256));
  EXPECT_EQ(
      "sensitive_data.txt",
      *event->FindStringKey(SafeBrowsingPrivateEventRouter::kKeyFileName));
  EXPECT_EQ(SafeBrowsingPrivateEventRouter::kTriggerFileDownload,
            *event->FindStringKey(SafeBrowsingPrivateEventRouter::kKeyTrigger));
  EXPECT_EQ("filePasswordProtected",
            *event->FindStringKey(
                SafeBrowsingPrivateEventRouter::kKeyUnscannedReason));
  EXPECT_EQ(
      EventResultToString(safe_browsing::EventResult::ALLOWED),
      *event->FindStringKey(SafeBrowsingPrivateEventRouter::kKeyEventResult));
}

TEST_F(SafeBrowsingPrivateEventRouterTest, TestOnUnscannedFileEvent_Blocked) {
  SetUpRouters();

  base::Value report;
  EXPECT_CALL(*client_, UploadSecurityEventReport_(_, _, _, _))
      .WillOnce(CaptureArg(&report));

  TriggerOnUnscannedFileEvent(safe_browsing::EventResult::BLOCKED);
  base::RunLoop().RunUntilIdle();

  Mock::VerifyAndClearExpectations(client_.get());
  EXPECT_EQ(base::Value::Type::DICTIONARY, report.type());
  base::Value* event_list =
      report.FindKey(policy::RealtimeReportingJobConfiguration::kEventListKey);
  ASSERT_NE(nullptr, event_list);
  EXPECT_EQ(base::Value::Type::LIST, event_list->type());
  base::Value::ListView mutable_list = event_list->GetList();
  ASSERT_EQ(1, (int)mutable_list.size());
  base::Value wrapper = std::move(mutable_list[0]);
  EXPECT_EQ(base::Value::Type::DICTIONARY, wrapper.type());
  base::Value* event =
      wrapper.FindKey(SafeBrowsingPrivateEventRouter::kKeyUnscannedFileEvent);
  ASSERT_NE(nullptr, event);

  EXPECT_EQ(12345, *event->FindIntKey(
                       SafeBrowsingPrivateEventRouter::kKeyContentSize));
  EXPECT_EQ("text/plain", *event->FindStringKey(
                              SafeBrowsingPrivateEventRouter::kKeyContentType));
  EXPECT_EQ("sha256_of_data",
            *event->FindStringKey(
                SafeBrowsingPrivateEventRouter::kKeyDownloadDigestSha256));
  EXPECT_EQ(
      "sensitive_data.txt",
      *event->FindStringKey(SafeBrowsingPrivateEventRouter::kKeyFileName));
  EXPECT_EQ(SafeBrowsingPrivateEventRouter::kTriggerFileDownload,
            *event->FindStringKey(SafeBrowsingPrivateEventRouter::kKeyTrigger));
  EXPECT_EQ("filePasswordProtected",
            *event->FindStringKey(
                SafeBrowsingPrivateEventRouter::kKeyUnscannedReason));
  EXPECT_EQ(
      EventResultToString(safe_browsing::EventResult::BLOCKED),
      *event->FindStringKey(SafeBrowsingPrivateEventRouter::kKeyEventResult));
}

TEST_F(SafeBrowsingPrivateEventRouterTest, TestProfileUsername) {
  SetUpRouters();
  SafeBrowsingEventObserver event_observer(
      api::safe_browsing_private::OnSecurityInterstitialShown::kEventName);
  event_router_->AddEventObserver(&event_observer);

  signin::IdentityTestEnvironment identity_test_environment;
  SafeBrowsingPrivateEventRouterFactory::GetForProfile(profile_)
      ->SetIdentityManagerForTesting(
          identity_test_environment.identity_manager());

  EXPECT_CALL(*client_, UploadSecurityEventReport_(_, _, _, _))
      .WillRepeatedly(Return());

  // With no primary account, we should not set the username.
  TriggerOnSecurityInterstitialShownEvent();
  base::RunLoop().RunUntilIdle();
  auto captured_args = event_observer.PassEventArgs().GetList()[0].Clone();
  EXPECT_EQ("", captured_args.FindKey("userName")->GetString());

  // With an unconsented primary account, we should set the username.
  identity_test_environment.MakeUnconsentedPrimaryAccountAvailable(
      "profile@example.com");
  TriggerOnSecurityInterstitialShownEvent();
  base::RunLoop().RunUntilIdle();
  captured_args = event_observer.PassEventArgs().GetList()[0].Clone();
  EXPECT_EQ("profile@example.com",
            captured_args.FindKey("userName")->GetString());

  // With a consented primary account, we should set the username.
  identity_test_environment.MakePrimaryAccountAvailable("profile@example.com");
  TriggerOnSecurityInterstitialShownEvent();
  base::RunLoop().RunUntilIdle();
  captured_args = event_observer.PassEventArgs().GetList()[0].Clone();
  EXPECT_EQ("profile@example.com",
            captured_args.FindKey("userName")->GetString());
}

// This next series of tests validate that we get the expected number of events
// reported when a given event name is enabled and we only trigger the related
// events (some events like interstiaial and dangerous downloads have multiple
// triggers for the same event name).
TEST_F(SafeBrowsingPrivateEventRouterTest, TestPasswordChangedEnabled) {
  std::set<std::string> enabled_event_names;
  enabled_event_names.insert(
      SafeBrowsingPrivateEventRouter::kKeyPasswordChangedEvent);
  SetUpRouters(/*authorized=*/true, /*realtime_reporting_enable=*/true,
               enabled_event_names);

  SafeBrowsingEventObserver event_observer(
      api::safe_browsing_private::OnPolicySpecifiedPasswordChanged::kEventName);
  event_router_->AddEventObserver(&event_observer);

  EXPECT_CALL(*client_, UploadSecurityEventReport_(_, _, _, _)).Times(1);
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
  enabled_event_names.insert(
      SafeBrowsingPrivateEventRouter::kKeyPasswordReuseEvent);
  SetUpRouters(/*authorized=*/true, /*realtime_reporting_enable=*/true,
               enabled_event_names);

  SafeBrowsingEventObserver event_observer(
      api::safe_browsing_private::OnPolicySpecifiedPasswordReuseDetected::
          kEventName);
  event_router_->AddEventObserver(&event_observer);

  EXPECT_CALL(*client_, UploadSecurityEventReport_(_, _, _, _)).Times(1);
  TriggerOnPolicySpecifiedPasswordReuseDetectedEvent();
  base::RunLoop().RunUntilIdle();

  // Assert the event actually did fire.
  ASSERT_EQ(1u, event_observer.PassEventArgs().GetList().size());

  // Make sure UploadSecurityEventReport was called the expected number of
  // times.
  Mock::VerifyAndClearExpectations(client_.get());
}

TEST_F(SafeBrowsingPrivateEventRouterTest, TestDangerousDownloadEnabled) {
  std::set<std::string> enabled_event_names;
  enabled_event_names.insert(
      SafeBrowsingPrivateEventRouter::kKeyDangerousDownloadEvent);
  SetUpRouters(/*authorized=*/true, /*realtime_reporting_enable=*/true,
               enabled_event_names);

  SafeBrowsingEventObserver event_observer(
      api::safe_browsing_private::OnDangerousDownloadOpened::kEventName);
  event_router_->AddEventObserver(&event_observer);

  EXPECT_CALL(*client_, UploadSecurityEventReport_(_, _, _, _)).Times(3);
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
  enabled_event_names.insert(
      SafeBrowsingPrivateEventRouter::kKeyInterstitialEvent);
  SetUpRouters(/*authorized=*/true, /*realtime_reporting_enable=*/true,
               enabled_event_names);

  SafeBrowsingEventObserver event_observer1(
      api::safe_browsing_private::OnSecurityInterstitialShown::kEventName);
  SafeBrowsingEventObserver event_observer2(
      api::safe_browsing_private::OnSecurityInterstitialProceeded::kEventName);
  event_router_->AddEventObserver(&event_observer1);
  event_router_->AddEventObserver(&event_observer2);

  EXPECT_CALL(*client_, UploadSecurityEventReport_(_, _, _, _)).Times(2);
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
  enabled_event_names.insert(
      SafeBrowsingPrivateEventRouter::kKeySensitiveDataEvent);
  SetUpRouters(/*authorized=*/true, /*realtime_reporting_enable=*/true,
               enabled_event_names);

  EXPECT_CALL(*client_, UploadSecurityEventReport_(_, _, _, _)).Times(1);
  TriggerOnSensitiveDataEvent(safe_browsing::EventResult::BLOCKED);
  base::RunLoop().RunUntilIdle();

  // Make sure UploadSecurityEventReport was called the expected number of
  // times.
  Mock::VerifyAndClearExpectations(client_.get());
}

TEST_F(SafeBrowsingPrivateEventRouterTest, TestUnscannedFileEnabled) {
  std::set<std::string> enabled_event_names;
  enabled_event_names.insert(
      SafeBrowsingPrivateEventRouter::kKeyUnscannedFileEvent);
  SetUpRouters(/*authorized=*/true, /*realtime_reporting_enable=*/true,
               enabled_event_names);

  EXPECT_CALL(*client_, UploadSecurityEventReport_(_, _, _, _)).Times(1);
  TriggerOnUnscannedFileEvent(safe_browsing::EventResult::ALLOWED);
  base::RunLoop().RunUntilIdle();

  // Make sure UploadSecurityEventReport was called the expected number of
  // times.
  Mock::VerifyAndClearExpectations(client_.get());
}

// Tests to make sure the feature flag and policy control real-time reporting
// as expected.  The parameter for these tests is a tuple of bools:
//
//   bool: whether the feature flag is enabled.
//   bool: whether the browser is manageable.
//   bool: whether the policy is enabled.
//   bool: whether the server has authorized this browser instance.
class SafeBrowsingIsRealtimeReportingEnabledTest
    : public SafeBrowsingPrivateEventRouterTest,
      public testing::WithParamInterface<
          testing::tuple<bool, bool, bool, bool>> {
 public:
  SafeBrowsingIsRealtimeReportingEnabledTest()
      : is_feature_flag_enabled_(testing::get<0>(GetParam())),
        is_manageable_(testing::get<1>(GetParam())),
        is_policy_enabled_(testing::get<2>(GetParam())),
        is_authorized_(testing::get<3>(GetParam())) {
    if (is_feature_flag_enabled_) {
      scoped_feature_list_.InitWithFeatures(
          {enterprise_connectors::kEnterpriseConnectorsEnabled},
          {extensions::SafeBrowsingPrivateEventRouter::
               kRealtimeReportingFeature});
    } else {
      scoped_feature_list_.InitWithFeatures(
          {}, {enterprise_connectors::kEnterpriseConnectorsEnabled,
               extensions::SafeBrowsingPrivateEventRouter::
                   kRealtimeReportingFeature});
    }

    // In chrome branded desktop builds, the browser is always manageable.
#if !BUILDFLAG(GOOGLE_CHROME_BRANDING) && !BUILDFLAG(IS_CHROMEOS_ASH)
    if (is_manageable_) {
      base::CommandLine::ForCurrentProcess()->AppendSwitch(
          switches::kEnableChromeBrowserCloudManagement);
    }
#endif

    if (is_policy_enabled_) {
      profile_->GetPrefs()->Set(enterprise_connectors::kOnSecurityEventPref,
                                *base::JSONReader::Read(kConnectorsPrefValue));
    }

#if BUILDFLAG(IS_CHROMEOS_ASH)
    auto user_manager = std::make_unique<ash::FakeChromeUserManager>();
    const AccountId account_id(
        AccountId::FromUserEmail(profile_->GetProfileUserName()));
    const user_manager::User* user = user_manager->AddUserWithAffiliation(
        account_id, /*is_affiliated=*/is_manageable_);
    chromeos::ProfileHelper::Get()->SetUserToProfileMappingForTesting(user,
                                                                      profile_);
    user_manager->UserLoggedIn(account_id, user->username_hash(),
                               /*browser_restart=*/false,
                               /*is_child=*/false);
    scoped_user_manager_ = std::make_unique<user_manager::ScopedUserManager>(
        std::move(user_manager));
    profile_->ScopedCrosSettingsTestHelper()
        ->InstallAttributes()
        ->SetCloudManaged("domain.com", "device_id");
#endif
  }

  bool should_init() {
#if !BUILDFLAG(IS_CHROMEOS_ASH)
    return is_feature_flag_enabled_;
#else
    return is_feature_flag_enabled_ && is_manageable_;
#endif
  }

 protected:
  base::test::ScopedFeatureList scoped_feature_list_;
  const bool is_feature_flag_enabled_;
  const bool is_manageable_;
  const bool is_policy_enabled_;
  const bool is_authorized_;

#if BUILDFLAG(IS_CHROMEOS_ASH)
 private:
  std::unique_ptr<user_manager::ScopedUserManager> scoped_user_manager_;
#endif
};

TEST_P(SafeBrowsingIsRealtimeReportingEnabledTest,
       ShouldInitRealtimeReportingClient) {
  EXPECT_EQ(
      should_init(),
      SafeBrowsingPrivateEventRouter::ShouldInitRealtimeReportingClient());
}

TEST_P(SafeBrowsingIsRealtimeReportingEnabledTest, CheckRealtimeReport) {
  // In production, the router won't actually be authorized unless it was
  // initialized.  The second argument to SetUpRouters() takes this into
  // account.
  SetUpRouters(should_init() && is_authorized_, is_policy_enabled_);
  SafeBrowsingEventObserver event_observer(
      api::safe_browsing_private::OnPolicySpecifiedPasswordChanged::kEventName);
  event_router_->AddEventObserver(&event_observer);

  bool should_report =
      is_feature_flag_enabled_ && is_policy_enabled_ && is_authorized_;
#if BUILDFLAG(IS_CHROMEOS_ASH)
  should_report &= is_manageable_;
#endif

  if (should_report) {
    EXPECT_CALL(*client_, UploadSecurityEventReport_(_, _, _, _)).Times(1);
  } else if (client_) {
    // Because the test will crate a |client_| object when the policy is
    // set, even if the feature flag or other conditions indicate that
    // reports should not be sent, it is possible that the pointer is not
    // null. In this case, make sure UploadSecurityEventReport() is not called.
    EXPECT_CALL(*client_, UploadSecurityEventReport_(_, _, _, _)).Times(0);
  }

  TriggerOnPolicySpecifiedPasswordChangedEvent();
  base::RunLoop().RunUntilIdle();

  // Assert the trigger actually did fire.
  EXPECT_EQ(1u, event_observer.PassEventArgs().GetList().size());

  // Make sure UploadSecurityEventReport was called the expected number of
  // times.
  if (client_)
    Mock::VerifyAndClearExpectations(client_.get());
}

INSTANTIATE_TEST_SUITE_P(All,
                         SafeBrowsingIsRealtimeReportingEnabledTest,
                         testing::Combine(testing::Bool(),
                                          testing::Bool(),
                                          testing::Bool(),
                                          testing::Bool()));

// Tests to make sure only the enabled events are reported.
//
//   std::string: the name of the event to enable.
//   int: How many triggers use this event name.
class SafeBrowsingIsRealtimeReportingEventDisabledTest
    : public SafeBrowsingPrivateEventRouterTest,
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
  EXPECT_CALL(*client_, UploadSecurityEventReport_(_, _, _, _))
      .Times(num_triggers_);
  TriggerOnPolicySpecifiedPasswordChangedEvent();
  TriggerOnPolicySpecifiedPasswordReuseDetectedEvent();
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
        testing::make_tuple(
            SafeBrowsingPrivateEventRouter::kKeyPasswordChangedEvent,
            1),
        testing::make_tuple(
            SafeBrowsingPrivateEventRouter::kKeyPasswordReuseEvent,
            1),
        testing::make_tuple(
            SafeBrowsingPrivateEventRouter::kKeyDangerousDownloadEvent,
            3),
        testing::make_tuple(
            SafeBrowsingPrivateEventRouter::kKeyInterstitialEvent,
            2),
        testing::make_tuple(
            SafeBrowsingPrivateEventRouter::kKeySensitiveDataEvent,
            1),
        testing::make_tuple(
            SafeBrowsingPrivateEventRouter::kKeyUnscannedFileEvent,
            1)));

}  // namespace extensions
