// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "build/build_config.h"
#include "build/chromeos_buildflags.h"
#include "chrome/browser/enterprise/reporting/cloud_profile_reporting_service.h"
#include "chrome/browser/enterprise/reporting/cloud_profile_reporting_service_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/test/base/chrome_test_utils.h"
#include "components/enterprise/browser/reporting/common_pref_names.h"
#include "components/policy/core/common/cloud/cloud_policy_store.h"
#include "components/policy/core/common/cloud/user_cloud_policy_manager.h"
#include "components/policy/proto/device_management_backend.pb.h"
#include "components/prefs/pref_service.h"
#include "content/public/test/browser_test.h"

#if BUILDFLAG(IS_CHROMEOS_LACROS)
#include "components/policy/core/common/policy_loader_lacros.h"
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
    EnableReportingPolicy(profile);
  }

  void EnableProfileManagement(Profile* profile) {
    em::PolicyData policy_data;
    policy_data.set_request_token("dm-token");
    policy_data.set_device_id("device-id");
#if BUILDFLAG(IS_CHROMEOS_LACROS)
    ASSERT_TRUE(profile->IsMainProfile());
    policy::PolicyLoaderLacros::set_main_user_policy_data_for_testing(
        policy_data);
#else
    profile->GetUserCloudPolicyManager()
        ->core()
        ->store()
        ->set_policy_data_for_testing(
            std::make_unique<em::PolicyData>(policy_data));
#endif
  }

  void EnableReportingPolicy(Profile* profile) {
    profile->GetPrefs()->SetBoolean(kCloudProfileReportingEnabled, true);
  }
};

IN_PROC_BROWSER_TEST_F(CloudProfileReportingServiceTest, LaunchTest) {
  ReportScheduler* report_scheduler =
      CloudProfileReportingServiceFactory::GetForProfile(
          chrome_test_utils::GetProfile(this))
          ->report_scheduler();
  ASSERT_TRUE(report_scheduler);
  EXPECT_TRUE(report_scheduler->IsNextReportScheduledForTesting() ||
              report_scheduler->GetActiveTriggerForTesting() ==
                  ReportScheduler::kTriggerTimer);
}

}  // namespace enterprise_reporting
