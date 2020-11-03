// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/enterprise/reporting/extension_request/extension_request_report_throttler.h"

#include "base/test/mock_callback.h"
#include "chrome/browser/enterprise/reporting/report_scheduler_desktop.h"
#include "chrome/test/base/testing_browser_process.h"
#include "chrome/test/base/testing_profile_manager.h"
#include "content/public/test/browser_task_environment.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace enterprise_reporting {
namespace {

constexpr char kProfileName1[] = "profile-name-1";
constexpr char kProfileName2[] = "profile-name-2";
constexpr base::TimeDelta kThrottleTime = base::TimeDelta::FromSeconds(2);

}  // namespace

class ExtensionRequestReportThrottlerTest : public ::testing::Test {
 public:
  ExtensionRequestReportThrottlerTest()
      : task_environment_(base::test::TaskEnvironment::TimeSource::MOCK_TIME),
        profile_manager_(TestingBrowserProcess::GetGlobal()) {}
  ~ExtensionRequestReportThrottlerTest() override = default;

  void SetUp() override {
    ASSERT_TRUE(profile_manager_.SetUp());
    throttler_.Enable(kThrottleTime, report_trigger_.Get());
  }

  void TearDown() override { throttler_.Disable(); }

  void VerifyBatchedProfiles(base::flat_set<base::FilePath> expected_paths) {
    EXPECT_EQ(expected_paths, throttler_.GetProfiles());
  }

  base::MockRepeatingClosure report_trigger_;
  ExtensionRequestReportThrottler throttler_;

  content::BrowserTaskEnvironment task_environment_;
  TestingProfileManager profile_manager_;
};

TEST_F(ExtensionRequestReportThrottlerTest, OneRequest) {
  TestingProfile* profile =
      profile_manager_.CreateTestingProfile(kProfileName1);

  EXPECT_CALL(report_trigger_, Run()).Times(1);

  throttler_.AddProfile(profile->GetPath());

  VerifyBatchedProfiles({profile->GetPath()});

  throttler_.ResetProfiles();

  VerifyBatchedProfiles({});
}

TEST_F(ExtensionRequestReportThrottlerTest,
       MultipleRequestsBetweenTriggerAndGenerator) {
  TestingProfile* profile1 =
      profile_manager_.CreateTestingProfile(kProfileName1);
  TestingProfile* profile2 =
      profile_manager_.CreateTestingProfile(kProfileName2);

  EXPECT_CALL(report_trigger_, Run()).Times(1);

  throttler_.AddProfile(profile1->GetPath());
  VerifyBatchedProfiles({profile1->GetPath()});

  // No ResetProfiles() called, means no report has been generated and uploaded.
  throttler_.AddProfile(profile1->GetPath());
  throttler_.AddProfile(profile1->GetPath());
  VerifyBatchedProfiles({profile1->GetPath()});

  throttler_.AddProfile(profile2->GetPath());
  VerifyBatchedProfiles({profile1->GetPath(), profile2->GetPath()});

  throttler_.ResetProfiles();

  VerifyBatchedProfiles({});
  ::testing::Mock::VerifyAndClearExpectations(&report_trigger_);
}

TEST_F(ExtensionRequestReportThrottlerTest, RequestThrottleByTimer) {
  TestingProfile* profile1 =
      profile_manager_.CreateTestingProfile(kProfileName1);
  TestingProfile* profile2 =
      profile_manager_.CreateTestingProfile(kProfileName2);

  EXPECT_CALL(report_trigger_, Run()).Times(1);

  throttler_.AddProfile(profile1->GetPath());

  ::testing::Mock::VerifyAndClearExpectations(&report_trigger_);
  VerifyBatchedProfiles({profile1->GetPath()});

  EXPECT_CALL(report_trigger_, Run()).Times(0);

  // The report is uploaded and a new request is added right after it.
  throttler_.ResetProfiles();
  throttler_.OnExtensionRequestUploaded();
  throttler_.AddProfile(profile2->GetPath());

  base::TimeDelta wait_time = base::TimeDelta::FromSeconds(1);
  task_environment_.FastForwardBy(wait_time);

  ::testing::Mock::VerifyAndClearExpectations(&report_trigger_);

  EXPECT_CALL(report_trigger_, Run()).Times(1);

  task_environment_.FastForwardBy(kThrottleTime - wait_time);

  VerifyBatchedProfiles({profile2->GetPath()});
  ::testing::Mock::VerifyAndClearExpectations(&report_trigger_);
}

TEST_F(ExtensionRequestReportThrottlerTest, RequestThrottleByOngoingRequest) {
  TestingProfile* profile1 =
      profile_manager_.CreateTestingProfile(kProfileName1);
  TestingProfile* profile2 =
      profile_manager_.CreateTestingProfile(kProfileName2);

  EXPECT_CALL(report_trigger_, Run()).Times(1);

  throttler_.AddProfile(profile1->GetPath());

  ::testing::Mock::VerifyAndClearExpectations(&report_trigger_);
  VerifyBatchedProfiles({profile1->GetPath()});

  EXPECT_CALL(report_trigger_, Run()).Times(0);

  // The report is generated but not uploaded. A new request is added after
  // throttle time.
  throttler_.ResetProfiles();
  task_environment_.FastForwardBy(kThrottleTime);
  throttler_.AddProfile(profile2->GetPath());

  ::testing::Mock::VerifyAndClearExpectations(&report_trigger_);

  EXPECT_CALL(report_trigger_, Run()).Times(1);

  // The previous report is finally finished.
  throttler_.OnExtensionRequestUploaded();

  VerifyBatchedProfiles({profile2->GetPath()});
  ::testing::Mock::VerifyAndClearExpectations(&report_trigger_);
}

}  // namespace enterprise_reporting
