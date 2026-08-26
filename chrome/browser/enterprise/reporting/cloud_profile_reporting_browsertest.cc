// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <tuple>

#include "base/test/run_until.h"
#include "base/test/scoped_feature_list.h"
#include "build/build_config.h"
#include "chrome/browser/enterprise/reporting/cloud_profile_reporting_service.h"
#include "chrome/browser/enterprise/reporting/cloud_profile_reporting_service_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/test/base/chrome_test_utils.h"
#include "components/enterprise/browser/reporting/common_pref_names.h"
#include "components/enterprise/browser/reporting/reporting_features.h"
#include "components/policy/core/common/cloud/cloud_policy_service.h"
#include "components/policy/core/common/cloud/cloud_policy_store.h"
#include "components/policy/core/common/cloud/mock_cloud_policy_client.h"
#include "components/policy/core/common/cloud/user_cloud_policy_manager.h"
#include "components/policy/proto/device_management_backend.pb.h"
#include "components/prefs/pref_service.h"
#include "content/public/test/browser_test.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"

#if BUILDFLAG(IS_ANDROID)
#include "chrome/test/base/android/android_browser_test.h"
#else
#include "chrome/test/base/in_process_browser_test.h"
#endif

namespace enterprise_reporting {

namespace em = enterprise_management;

namespace {

void EnableProfileManagement(Profile* profile) {
  em::PolicyData policy_data;
  policy_data.set_request_token("dm-token");
  policy_data.set_device_id("device-id");
  profile->GetCloudPolicyManager()
      ->core()
      ->store()
      ->set_policy_data_for_testing(
          std::make_unique<em::PolicyData>(policy_data));
  auto client = std::make_unique<policy::MockCloudPolicyClient>();
  client->SetDMToken("dm-token");
  client->SetClientId("device-id");
  profile->GetCloudPolicyManager()->core()->ConnectForTesting(
      /*service=*/nullptr, std::move(client));
}

void SetReportingPolicy(Profile* profile, bool enabled) {
  profile->GetPrefs()->SetBoolean(kCloudProfileReportingEnabled, enabled);
}

}  // namespace

class CloudProfileReportingServiceTest
    : public PlatformBrowserTest,
      public testing::WithParamInterface<
          // Two boolean variables represents whether profile reporting and
          // signals reporting is enabled
          std::tuple<bool, bool>> {
 public:
  CloudProfileReportingServiceTest() {
    scoped_feature_list_.InitAndDisableFeature(kUploadReportOnProfileOpen);
  }
  ~CloudProfileReportingServiceTest() override = default;

  void SetUpOnMainThread() override {
    Profile* profile = chrome_test_utils::GetProfile(this);
    ASSERT_TRUE(CloudProfileReportingServiceFactory::GetForProfile(profile));
    EnableProfileManagement(profile);
    SetReportingPolicy(profile, profile_reporting_enabled());
    profile->GetPrefs()->SetBoolean(kUserSecuritySignalsReporting,
                                    signals_reporting_enabled());
  }

  bool profile_reporting_enabled() const { return std::get<0>(GetParam()); }
  bool signals_reporting_enabled() const { return std::get<1>(GetParam()); }

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
};

IN_PROC_BROWSER_TEST_P(CloudProfileReportingServiceTest,
                       VerifyReportingConfig) {
  ReportScheduler* report_scheduler =
      CloudProfileReportingServiceFactory::GetForProfile(
          chrome_test_utils::GetProfile(this))
          ->report_scheduler();
  ASSERT_TRUE(report_scheduler);

  bool expect_report =
      profile_reporting_enabled() || signals_reporting_enabled();
  if (expect_report) {
    EXPECT_TRUE(base::test::RunUntil([&]() {
      return report_scheduler->GetActiveTriggerForTesting() !=
             ReportTrigger::kTriggerNone;
    }));
  }

  auto active_trigger = report_scheduler->GetActiveTriggerForTesting();
  auto active_config = report_scheduler->GetActiveGenerationConfigForTesting();

  if (signals_reporting_enabled() && profile_reporting_enabled()) {
    EXPECT_EQ(active_trigger, ReportTrigger::kTriggerTimer);
    EXPECT_EQ(active_config.security_signals_mode,
              SecuritySignalsMode::kSignalsAttached);
  } else if (profile_reporting_enabled()) {
    EXPECT_EQ(active_trigger, ReportTrigger::kTriggerTimer);
    EXPECT_EQ(active_config.security_signals_mode,
              SecuritySignalsMode::kNoSignals);
  } else if (signals_reporting_enabled()) {
    EXPECT_EQ(active_trigger, ReportTrigger::kTriggerSecurity);
    EXPECT_EQ(active_config.security_signals_mode,
              SecuritySignalsMode::kSignalsOnly);
  } else {
    EXPECT_EQ(active_trigger, ReportTrigger::kTriggerNone);
  }
}

INSTANTIATE_TEST_SUITE_P(All,
                         CloudProfileReportingServiceTest,
                         testing::Combine(
                             /*profile_reporting_enabled=*/testing::Bool(),
                             /*signals_reporting_enabled=*/testing::Bool()));

class UploadReportOnProfileOpenBrowserTest
    : public PlatformBrowserTest,
      public testing::WithParamInterface<std::tuple<bool, bool, bool>> {
 public:
  UploadReportOnProfileOpenBrowserTest() {
    scoped_feature_list_.InitWithFeatureState(
        kUploadReportOnProfileOpen, upload_report_on_profile_open_enabled());
  }
  ~UploadReportOnProfileOpenBrowserTest() override = default;

  void SetUpOnMainThread() override {
    Profile* profile = chrome_test_utils::GetProfile(this);
    ASSERT_TRUE(CloudProfileReportingServiceFactory::GetForProfile(profile));
    SetReportingPolicy(profile, profile_reporting_enabled());
    profile->GetPrefs()->SetBoolean(kUserSecuritySignalsReporting,
                                    signals_reporting_enabled());
    // Set a recent upload timestamp so that the 24h periodic timer does not
    // expire on startup.
    profile->GetPrefs()->SetTime(kLastUploadTimestamp, base::Time::Now());
    EnableProfileManagement(profile);
  }

  bool upload_report_on_profile_open_enabled() const {
    return std::get<0>(GetParam());
  }
  bool profile_reporting_enabled() const { return std::get<1>(GetParam()); }
  bool signals_reporting_enabled() const { return std::get<2>(GetParam()); }

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
};

IN_PROC_BROWSER_TEST_P(UploadReportOnProfileOpenBrowserTest,
                       VerifyProfileOpenTrigger) {
  ReportScheduler* report_scheduler =
      CloudProfileReportingServiceFactory::GetForProfile(
          chrome_test_utils::GetProfile(this))
          ->report_scheduler();
  ASSERT_TRUE(report_scheduler);

  bool expect_report =
      upload_report_on_profile_open_enabled()
          ? (profile_reporting_enabled() || signals_reporting_enabled())
          : signals_reporting_enabled();

  if (expect_report) {
    EXPECT_TRUE(base::test::RunUntil([&]() {
      return report_scheduler->GetActiveTriggerForTesting() !=
             ReportTrigger::kTriggerNone;
    }));
  }

  auto active_trigger = report_scheduler->GetActiveTriggerForTesting();
  auto active_config = report_scheduler->GetActiveGenerationConfigForTesting();

  if (upload_report_on_profile_open_enabled()) {
    // When kUploadReportOnProfileOpen is enabled, a report is uploaded upon
    // profile open even if a report was uploaded recently.
    if (profile_reporting_enabled() && signals_reporting_enabled()) {
      EXPECT_EQ(active_trigger, ReportTrigger::kTriggerProfileOpened);
      EXPECT_EQ(active_config.security_signals_mode,
                SecuritySignalsMode::kSignalsAttached);
    } else if (profile_reporting_enabled()) {
      EXPECT_EQ(active_trigger, ReportTrigger::kTriggerProfileOpened);
      EXPECT_EQ(active_config.security_signals_mode,
                SecuritySignalsMode::kNoSignals);
    } else if (signals_reporting_enabled()) {
      EXPECT_EQ(active_trigger, ReportTrigger::kTriggerProfileOpened);
      EXPECT_EQ(active_config.security_signals_mode,
                SecuritySignalsMode::kSignalsOnly);
    } else {
      EXPECT_EQ(active_trigger, ReportTrigger::kTriggerNone);
    }
  } else {
    // When kUploadReportOnProfileOpen is disabled:
    // Periodic profile reporting is not triggered since the last upload is
    // recent, but legacy security signal reporting triggers its own report on
    // startup if enabled.
    if (signals_reporting_enabled()) {
      EXPECT_EQ(active_trigger, ReportTrigger::kTriggerSecurity);
      EXPECT_EQ(active_config.security_signals_mode,
                SecuritySignalsMode::kSignalsOnly);
    } else {
      EXPECT_EQ(active_trigger, ReportTrigger::kTriggerNone);
    }
  }
}

INSTANTIATE_TEST_SUITE_P(
    All,
    UploadReportOnProfileOpenBrowserTest,
    testing::Combine(
        /*upload_report_on_profile_open_enabled=*/testing::Bool(),
        /*profile_reporting_enabled=*/testing::Bool(),
        /*signals_reporting_enabled=*/testing::Bool()));

}  // namespace enterprise_reporting
