// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/run_loop.h"
#include "build/build_config.h"
#include "chrome/browser/enterprise/reporting/cloud_profile_reporting_service.h"
#include "chrome/browser/enterprise/reporting/cloud_profile_reporting_service_factory.h"
#include "chrome/browser/policy/profile_policy_connector.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/test/base/chrome_test_utils.h"
#include "components/enterprise/browser/reporting/common_pref_names.h"
#include "components/policy/core/common/cloud/cloud_policy_service.h"
#include "components/policy/core/common/cloud/cloud_policy_store.h"
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

class CloudProfileReportingServiceTest : public PlatformBrowserTest {
 public:
  CloudProfileReportingServiceTest() = default;
  ~CloudProfileReportingServiceTest() override = default;

  void SetUpOnMainThread() override {
    Profile* profile = chrome_test_utils::GetProfile(this);
    EnableProfileManagement(profile);
    profile->GetPrefs()->SetBoolean(kCloudProfileReportingEnabled, true);
    SetReportingPolicy(profile, /*enabled=*/true);
  }

  void EnableProfileManagement(Profile* profile) {
    em::PolicyData policy_data;
    policy_data.set_request_token("dm-token");
    policy_data.set_device_id("device-id");
    profile->GetCloudPolicyManager()
        ->core()
        ->store()
        ->set_policy_data_for_testing(
            std::make_unique<em::PolicyData>(policy_data));
    auto client = std::make_unique<policy::CloudPolicyClient>(
        /*service=*/nullptr, /*url_laoder_factory=*/nullptr);
    profile->GetCloudPolicyManager()->core()->ConnectForTesting(
        /*service=*/nullptr, std::move(client));
  }

  void SetReportingPolicy(Profile* profile, bool enabled) {
    profile->GetPrefs()->SetBoolean(kCloudProfileReportingEnabled, enabled);
  }
};

IN_PROC_BROWSER_TEST_F(CloudProfileReportingServiceTest, LaunchTest) {
  base::RunLoop().RunUntilIdle();
  ReportScheduler* report_scheduler =
      CloudProfileReportingServiceFactory::GetForProfile(
          chrome_test_utils::GetProfile(this))
          ->report_scheduler();
  ASSERT_TRUE(report_scheduler);
  EXPECT_TRUE(report_scheduler->IsNextReportScheduledForTesting() ||
              report_scheduler->GetActiveTriggerForTesting() ==
                  ReportTrigger::kTriggerTimer);
}

#if !BUILDFLAG(IS_ANDROID)
class CloudProfileReportingServiceTestDesktop
    : public CloudProfileReportingServiceTest,
      public testing::WithParamInterface<
          // Two boolean variables represents whether profile reporting and
          // signals reporting is enabled
          testing::tuple<bool, bool>> {
 public:
  CloudProfileReportingServiceTestDesktop() = default;
  ~CloudProfileReportingServiceTestDesktop() override = default;

  void SetUpOnMainThread() override {
    Profile* profile = chrome_test_utils::GetProfile(this);
    EnableProfileManagement(profile);
    SetReportingPolicy(profile, profile_reporting_enabled());
    profile->GetPrefs()->SetBoolean(kUserSecuritySignalsReporting,
                                    signals_reporting_enabled());
  }

  bool profile_reporting_enabled() { return testing::get<0>(GetParam()); }
  bool signals_reporting_enabled() { return testing::get<1>(GetParam()); }
};

IN_PROC_BROWSER_TEST_P(CloudProfileReportingServiceTestDesktop,
                       VerifyReportingConfig) {
  base::RunLoop().RunUntilIdle();
  ReportScheduler* report_scheduler =
      CloudProfileReportingServiceFactory::GetForProfile(
          chrome_test_utils::GetProfile(this))
          ->report_scheduler();
  ASSERT_TRUE(report_scheduler);

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
                         CloudProfileReportingServiceTestDesktop,
                         testing::Combine(
                             /*profile_reporting_enabled=*/testing::Bool(),
                             /*signals_reporting_enabled=*/testing::Bool()));

#endif  // !BUILDFLAG(IS_ANDROID)

}  // namespace enterprise_reporting
