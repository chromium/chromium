// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/enterprise/reporting/report_scheduler.h"

#include <utility>

#include "base/bind_helpers.h"
#include "base/test/gmock_callback_support.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/scoped_feature_list.h"
#include "build/build_config.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/enterprise/reporting/prefs.h"
#include "chrome/browser/policy/fake_browser_dm_token_storage.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/upgrade_detector/build_state.h"
#include "chrome/common/chrome_constants.h"
#include "chrome/common/chrome_features.h"
#include "chrome/common/pref_names.h"
#include "chrome/test/base/scoped_testing_local_state.h"
#include "chrome/test/base/testing_browser_process.h"
#include "chrome/test/base/testing_profile_manager.h"
#include "components/policy/core/common/cloud/mock_cloud_policy_client.h"
#include "components/version_info/version_info.h"
#include "content/public/test/browser_task_environment.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using ::base::test::RunOnceCallback;
using ::testing::_;
using ::testing::ByMove;
using ::testing::Invoke;
using ::testing::Return;
using ::testing::WithArgs;

namespace em = enterprise_management;

namespace enterprise_reporting {

namespace {

constexpr char kDMToken[] = "dm_token";
constexpr char kClientId[] = "client_id";
constexpr base::TimeDelta kDefaultUploadInterval =
    base::TimeDelta::FromHours(24);

#if !defined(OS_CHROMEOS)
constexpr char kUploadTriggerMetricName[] =
    "Enterprise.CloudReportingUploadTrigger";
#endif

}  // namespace

ACTION_P(ScheduleGeneratorCallback, request_number) {
  ReportGenerator::ReportRequests requests;
  for (int i = 0; i < request_number; i++)
    requests.push(std::make_unique<ReportGenerator::ReportRequest>());
  base::ThreadTaskRunnerHandle::Get()->PostTask(
      FROM_HERE, base::BindOnce(std::move(arg0), std::move(requests)));
}

class MockReportGenerator : public ReportGenerator {
 public:
  void Generate(bool with_profiles, ReportCallback callback) override {
    OnGenerate(with_profiles, callback);
  }
  MOCK_METHOD2(OnGenerate, void(bool with_profiles, ReportCallback& callback));
  MOCK_METHOD0(GenerateBasic, ReportRequests());
};

class MockReportUploader : public ReportUploader {
 public:
  MockReportUploader() : ReportUploader(nullptr, 0) {}
  ~MockReportUploader() override = default;
  MOCK_METHOD2(SetRequestAndUpload, void(ReportRequests, ReportCallback));

 private:
  DISALLOW_COPY_AND_ASSIGN(MockReportUploader);
};

class ReportSchedulerTest : public ::testing::Test {
 public:
  ReportSchedulerTest()
      : task_environment_(base::test::TaskEnvironment::TimeSource::MOCK_TIME),
        local_state_(TestingBrowserProcess::GetGlobal()),
        profile_manager_(TestingBrowserProcess::GetGlobal(), &local_state_) {}
  ~ReportSchedulerTest() override = default;
  void SetUp() override {
    ASSERT_TRUE(profile_manager_.SetUp());
    client_ptr_ = std::make_unique<policy::MockCloudPolicyClient>();
    client_ = client_ptr_.get();
    generator_ptr_ = std::make_unique<MockReportGenerator>();
    generator_ = generator_ptr_.get();
    uploader_ptr_ = std::make_unique<MockReportUploader>();
    uploader_ = uploader_ptr_.get();
#if !defined(OS_CHROMEOS)
    SetLastUploadVersion(chrome::kChromeVersion);
#endif
    Init(true, kDMToken, kClientId);
  }

  void Init(bool policy_enabled,
            const std::string& dm_token,
            const std::string& client_id) {
    ToggleCloudReport(policy_enabled);
    storage_.SetDMToken(dm_token);
    storage_.SetClientId(client_id);
  }

  void CreateScheduler() {
    scheduler_ =
        std::make_unique<ReportScheduler>(client_, std::move(generator_ptr_));
    scheduler_->SetReportUploaderForTesting(std::move(uploader_ptr_));
  }

  void SetLastUploadInHour(base::TimeDelta gap) {
    previous_set_last_upload_timestamp_ = base::Time::Now() - gap;
    local_state_.Get()->SetTime(kLastUploadTimestamp,
                                previous_set_last_upload_timestamp_);
  }

  void ToggleCloudReport(bool enabled) {
    local_state_.Get()->SetManagedPref(prefs::kCloudReportingEnabled,
                                       std::make_unique<base::Value>(enabled));
  }

#if !defined(OS_CHROMEOS)
  void SetLastUploadVersion(const std::string& version) {
    local_state_.Get()->SetString(kLastUploadVersion, version);
  }

  void ExpectLastUploadVersion(const std::string& version) {
    EXPECT_EQ(local_state_.Get()->GetString(kLastUploadVersion), version);
  }
#endif  // !defined(OS_CHROMEOS)

  // If lastUploadTimestamp is updated recently, it should be updated as Now().
  // Otherwise, it should be same as previous set timestamp.
  void ExpectLastUploadTimestampUpdated(bool is_updated) {
    auto current_last_upload_timestamp =
        local_state_.Get()->GetTime(kLastUploadTimestamp);
    if (is_updated) {
      EXPECT_EQ(base::Time::Now(), current_last_upload_timestamp);
    } else {
      EXPECT_EQ(previous_set_last_upload_timestamp_,
                current_last_upload_timestamp);
    }
  }

  ReportGenerator::ReportRequests CreateRequests(int number) {
    ReportGenerator::ReportRequests requests;
    for (int i = 0; i < number; i++)
      requests.push(std::make_unique<ReportGenerator::ReportRequest>());
    return requests;
  }

  // Chrome OS needn't setup registration.
  void EXPECT_CALL_SetupRegistration() {
#if defined(OS_CHROMEOS)
    EXPECT_CALL(*client_, SetupRegistration(_, _, _)).Times(0);
#else
    EXPECT_CALL(*client_, SetupRegistration(kDMToken, kClientId, _));
#endif
  }

  void EXPECT_CALL_SetupRegistrationWithSetDMToken() {
#if defined(OS_CHROMEOS)
    EXPECT_CALL(*client_, SetupRegistration(_, _, _)).Times(0);
#else
    EXPECT_CALL(*client_, SetupRegistration(kDMToken, kClientId, _))
        .WillOnce(WithArgs<0>(
            Invoke(client_, &policy::MockCloudPolicyClient::SetDMToken)));
#endif
  }

  base::test::ScopedFeatureList scoped_feature_list_;
  content::BrowserTaskEnvironment task_environment_;
  ScopedTestingLocalState local_state_;
  TestingProfileManager profile_manager_;

  std::unique_ptr<ReportScheduler> scheduler_;
  policy::MockCloudPolicyClient* client_;
  MockReportGenerator* generator_;
  MockReportUploader* uploader_;
  policy::FakeBrowserDMTokenStorage storage_;
  base::Time previous_set_last_upload_timestamp_;
  base::HistogramTester histogram_tester_;

 private:
  std::unique_ptr<policy::MockCloudPolicyClient> client_ptr_;
  std::unique_ptr<MockReportGenerator> generator_ptr_;
  std::unique_ptr<MockReportUploader> uploader_ptr_;
  DISALLOW_COPY_AND_ASSIGN(ReportSchedulerTest);
};

TEST_F(ReportSchedulerTest, NoReportWithoutPolicy) {
  Init(false, kDMToken, kClientId);
  CreateScheduler();
  EXPECT_FALSE(scheduler_->IsNextReportScheduledForTesting());
}

// Chrome OS needn't set dm token and client id in the report scheduler.
#if !defined(OS_CHROMEOS)
TEST_F(ReportSchedulerTest, NoReportWithoutDMToken) {
  Init(true, "", kClientId);
  CreateScheduler();
  EXPECT_FALSE(scheduler_->IsNextReportScheduledForTesting());
}

TEST_F(ReportSchedulerTest, NoReportWithoutClientId) {
  Init(true, kDMToken, "");
  CreateScheduler();
  EXPECT_FALSE(scheduler_->IsNextReportScheduledForTesting());
}
#endif

TEST_F(ReportSchedulerTest, UploadReportSucceeded) {
  EXPECT_CALL_SetupRegistration();
  EXPECT_CALL(*generator_, OnGenerate(/*with_profiles=*/true, _))
      .WillOnce(WithArgs<1>(ScheduleGeneratorCallback(1)));
  EXPECT_CALL(*uploader_, SetRequestAndUpload(_, _))
      .WillOnce(RunOnceCallback<1>(ReportUploader::kSuccess));

  CreateScheduler();
  EXPECT_TRUE(scheduler_->IsNextReportScheduledForTesting());

  // Run pending task.
  task_environment_.FastForwardBy(base::TimeDelta());

  // Next report is scheduled.
  EXPECT_TRUE(scheduler_->IsNextReportScheduledForTesting());
  ExpectLastUploadTimestampUpdated(true);

  ::testing::Mock::VerifyAndClearExpectations(client_);
  ::testing::Mock::VerifyAndClearExpectations(generator_);
}

TEST_F(ReportSchedulerTest, UploadReportTransientError) {
  EXPECT_CALL_SetupRegistration();
  EXPECT_CALL(*generator_, OnGenerate(/*with_profiles=*/true, _))
      .WillOnce(WithArgs<1>(ScheduleGeneratorCallback(1)));
  EXPECT_CALL(*uploader_, SetRequestAndUpload(_, _))
      .WillOnce(RunOnceCallback<1>(ReportUploader::kTransientError));

  CreateScheduler();
  EXPECT_TRUE(scheduler_->IsNextReportScheduledForTesting());

  // Run pending task.
  task_environment_.FastForwardBy(base::TimeDelta());

  // Next report is scheduled.
  EXPECT_TRUE(scheduler_->IsNextReportScheduledForTesting());
  ExpectLastUploadTimestampUpdated(true);

  ::testing::Mock::VerifyAndClearExpectations(client_);
  ::testing::Mock::VerifyAndClearExpectations(generator_);
}

TEST_F(ReportSchedulerTest, UploadReportPersistentError) {
  EXPECT_CALL_SetupRegistrationWithSetDMToken();
  EXPECT_CALL(*generator_, OnGenerate(/*with_profiles=*/true, _))
      .WillOnce(WithArgs<1>(ScheduleGeneratorCallback(1)));
  EXPECT_CALL(*uploader_, SetRequestAndUpload(_, _))
      .WillOnce(RunOnceCallback<1>(ReportUploader::kPersistentError));

  CreateScheduler();
  EXPECT_TRUE(scheduler_->IsNextReportScheduledForTesting());

  // Run pending task.
  task_environment_.FastForwardBy(base::TimeDelta());

  // Next report is not scheduled.
  EXPECT_FALSE(scheduler_->IsNextReportScheduledForTesting());
  ExpectLastUploadTimestampUpdated(false);

  // Turn off and on reporting to resume.
  ToggleCloudReport(false);
  ToggleCloudReport(true);
  EXPECT_TRUE(scheduler_->IsNextReportScheduledForTesting());

  ::testing::Mock::VerifyAndClearExpectations(client_);
  ::testing::Mock::VerifyAndClearExpectations(generator_);
}

TEST_F(ReportSchedulerTest, NoReportGenerate) {
  EXPECT_CALL_SetupRegistrationWithSetDMToken();
  EXPECT_CALL(*generator_, OnGenerate(/*with_profiles=*/true, _))
      .WillOnce(WithArgs<1>(ScheduleGeneratorCallback(0)));
  EXPECT_CALL(*uploader_, SetRequestAndUpload(_, _)).Times(0);

  CreateScheduler();
  EXPECT_TRUE(scheduler_->IsNextReportScheduledForTesting());

  // Run pending task.
  task_environment_.FastForwardBy(base::TimeDelta());

  // Next report is not scheduled.
  EXPECT_FALSE(scheduler_->IsNextReportScheduledForTesting());
  ExpectLastUploadTimestampUpdated(false);

  // Turn off and on reporting to resume.
  ToggleCloudReport(false);
  ToggleCloudReport(true);
  EXPECT_TRUE(scheduler_->IsNextReportScheduledForTesting());

  ::testing::Mock::VerifyAndClearExpectations(client_);
  ::testing::Mock::VerifyAndClearExpectations(generator_);
}

TEST_F(ReportSchedulerTest, TimerDelayWithLastUploadTimestamp) {
  const base::TimeDelta gap = base::TimeDelta::FromHours(10);
  SetLastUploadInHour(gap);

  EXPECT_CALL_SetupRegistration();
  EXPECT_CALL(*generator_, OnGenerate(/*with_profiles=*/true, _))
      .WillOnce(WithArgs<1>(ScheduleGeneratorCallback(1)));
  EXPECT_CALL(*uploader_, SetRequestAndUpload(_, _))
      .WillOnce(RunOnceCallback<1>(ReportUploader::kSuccess));

  CreateScheduler();
  EXPECT_TRUE(scheduler_->IsNextReportScheduledForTesting());

  base::TimeDelta next_report_delay = kDefaultUploadInterval - gap;
  task_environment_.FastForwardBy(next_report_delay -
                                  base::TimeDelta::FromSeconds(1));
  ExpectLastUploadTimestampUpdated(false);
  task_environment_.FastForwardBy(base::TimeDelta::FromSeconds(1));
  ExpectLastUploadTimestampUpdated(true);

  ::testing::Mock::VerifyAndClearExpectations(client_);
  ::testing::Mock::VerifyAndClearExpectations(generator_);
}

TEST_F(ReportSchedulerTest, TimerDelayWithoutLastUploadTimestamp) {
  EXPECT_CALL_SetupRegistration();
  EXPECT_CALL(*generator_, OnGenerate(/*with_profiles=*/true, _))
      .WillOnce(WithArgs<1>(ScheduleGeneratorCallback(1)));
  EXPECT_CALL(*uploader_, SetRequestAndUpload(_, _))
      .WillOnce(RunOnceCallback<1>(ReportUploader::kSuccess));

  CreateScheduler();
  EXPECT_TRUE(scheduler_->IsNextReportScheduledForTesting());

  ExpectLastUploadTimestampUpdated(false);
  task_environment_.FastForwardBy(base::TimeDelta());
  ExpectLastUploadTimestampUpdated(true);

  ::testing::Mock::VerifyAndClearExpectations(client_);
}

TEST_F(ReportSchedulerTest,
       ReportingIsDisabledWhileNewReportIsScheduledButNotPosted) {
  EXPECT_CALL_SetupRegistration();

  CreateScheduler();
  EXPECT_TRUE(scheduler_->IsNextReportScheduledForTesting());

  // Run pending task.
  task_environment_.FastForwardBy(base::TimeDelta());

  ToggleCloudReport(false);

  // Next report is not scheduled.
  EXPECT_FALSE(scheduler_->IsNextReportScheduledForTesting());
  ExpectLastUploadTimestampUpdated(false);

  ::testing::Mock::VerifyAndClearExpectations(client_);
  ::testing::Mock::VerifyAndClearExpectations(generator_);
}

TEST_F(ReportSchedulerTest, ReportingIsDisabledWhileNewReportIsPosted) {
  EXPECT_CALL_SetupRegistration();
  EXPECT_CALL(*generator_, OnGenerate(/*with_profiles=*/true, _))
      .WillOnce(WithArgs<1>(ScheduleGeneratorCallback(1)));
  EXPECT_CALL(*uploader_, SetRequestAndUpload(_, _))
      .WillOnce(RunOnceCallback<1>(ReportUploader::kSuccess));

  CreateScheduler();
  EXPECT_TRUE(scheduler_->IsNextReportScheduledForTesting());

  // Run pending task.
  task_environment_.FastForwardBy(base::TimeDelta());

  ToggleCloudReport(false);

  // Run pending task.
  task_environment_.FastForwardBy(base::TimeDelta());

  ExpectLastUploadTimestampUpdated(true);
  // Next report is not scheduled.
  EXPECT_FALSE(scheduler_->IsNextReportScheduledForTesting());

  ::testing::Mock::VerifyAndClearExpectations(client_);
  ::testing::Mock::VerifyAndClearExpectations(generator_);
}

#if !defined(OS_CHROMEOS)

// Tests that a basic report is generated and uploaded when a browser update is
// detected.
TEST_F(ReportSchedulerTest, OnUpdate) {
  // Pretend that a periodic report was generated recently so that one isn't
  // kicked off during startup.
  SetLastUploadInHour(base::TimeDelta::FromHours(1));
  EXPECT_CALL_SetupRegistration();
  EXPECT_CALL(*generator_, OnGenerate(/*with_profiles=*/false, _))
      .WillOnce(WithArgs<1>(ScheduleGeneratorCallback(1)));
  EXPECT_CALL(*uploader_, SetRequestAndUpload(_, _))
      .WillOnce(RunOnceCallback<1>(ReportUploader::kSuccess));

  CreateScheduler();
  g_browser_process->GetBuildState()->SetUpdate(
      BuildState::UpdateType::kNormalUpdate,
      base::Version("1" + version_info::GetVersionNumber()), base::nullopt);
  task_environment_.RunUntilIdle();

  // The timestamp should not have been updated, since a periodic report was not
  // generated/uploaded.
  ExpectLastUploadTimestampUpdated(false);

  histogram_tester_.ExpectUniqueSample(kUploadTriggerMetricName, 2, 1);
}

// Tests that a full report is generated and uploaded following a basic report
// if the timer fires while the basic report is being uploaded.
TEST_F(ReportSchedulerTest, DeferredTimer) {
  EXPECT_CALL_SetupRegistration();
  CreateScheduler();

  // An update arrives, triggering report generation and upload (sans profiles).
  EXPECT_CALL(*generator_, OnGenerate(/*with_profiles=*/false, _))
      .WillOnce(WithArgs<1>(ScheduleGeneratorCallback(1)));

  // Hang on to the uploader's ReportCallback.
  ReportUploader::ReportCallback saved_callback;
  EXPECT_CALL(*uploader_, SetRequestAndUpload(_, _))
      .WillOnce([&saved_callback](ReportUploader::ReportRequests requests,
                                  ReportUploader::ReportCallback callback) {
        saved_callback = std::move(callback);
      });

  g_browser_process->GetBuildState()->SetUpdate(
      BuildState::UpdateType::kNormalUpdate,
      base::Version("1" + version_info::GetVersionNumber()), base::nullopt);
  task_environment_.RunUntilIdle();
  ::testing::Mock::VerifyAndClearExpectations(generator_);
  ::testing::Mock::VerifyAndClearExpectations(uploader_);

  // Now the timer fires before the upload completes. No new report should be
  // generated yet.
  task_environment_.RunUntilIdle();
  ::testing::Mock::VerifyAndClearExpectations(generator_);

  // Once the previous upload completes, a new report should be generated
  // forthwith.
  EXPECT_CALL(*generator_, OnGenerate(/*with_profiles=*/true, _))
      .WillOnce(WithArgs<1>(ScheduleGeneratorCallback(1)));
  auto new_uploader = std::make_unique<MockReportUploader>();
  EXPECT_CALL(*new_uploader, SetRequestAndUpload(_, _))
      .WillOnce(RunOnceCallback<1>(ReportUploader::kSuccess));
  std::move(saved_callback).Run(ReportUploader::kSuccess);
  ExpectLastUploadTimestampUpdated(false);
  ::testing::Mock::VerifyAndClearExpectations(generator_);

  this->uploader_ = new_uploader.get();
  this->scheduler_->SetReportUploaderForTesting(std::move(new_uploader));

  task_environment_.RunUntilIdle();
  ::testing::Mock::VerifyAndClearExpectations(uploader_);
  ExpectLastUploadTimestampUpdated(true);

  histogram_tester_.ExpectBucketCount(kUploadTriggerMetricName, 1, 1);
  histogram_tester_.ExpectBucketCount(kUploadTriggerMetricName, 2, 1);
}

// Tests that a basic report is generated and uploaded during startup when a
// new version is being run and the last periodic upload was less than a day
// ago.
TEST_F(ReportSchedulerTest, OnNewVersion) {
  // Pretend that the last upload was from a different browser version.
  SetLastUploadVersion(chrome::kChromeVersion + std::string("1"));

  // Pretend that a periodic report was generated recently.
  SetLastUploadInHour(base::TimeDelta::FromHours(1));

  EXPECT_CALL_SetupRegistration();
  EXPECT_CALL(*generator_, OnGenerate(/*with_profiles=*/false, _))
      .WillOnce(WithArgs<1>(ScheduleGeneratorCallback(1)));
  EXPECT_CALL(*uploader_, SetRequestAndUpload(_, _))
      .WillOnce(RunOnceCallback<1>(ReportUploader::kSuccess));

  CreateScheduler();

  task_environment_.RunUntilIdle();

  // The timestamp should not have been updated, since a periodic report was not
  // generated/uploaded.
  ExpectLastUploadTimestampUpdated(false);

  // The last upload is now from this version.
  ExpectLastUploadVersion(chrome::kChromeVersion);

  histogram_tester_.ExpectUniqueSample(kUploadTriggerMetricName, 3, 1);
}

// Tests that a full report is generated and uploaded during startup when a
// new version is being run and the last periodic upload was more than a day
// ago.
TEST_F(ReportSchedulerTest, OnNewVersionRegularReport) {
  // Pretend that the last upload was from a different browser version.
  SetLastUploadVersion(chrome::kChromeVersion + std::string("1"));

  // Pretend that a periodic report was last generated over a day ago.
  SetLastUploadInHour(base::TimeDelta::FromHours(25));

  EXPECT_CALL_SetupRegistration();
  EXPECT_CALL(*generator_, OnGenerate(/*with_profiles=*/true, _))
      .WillOnce(WithArgs<1>(ScheduleGeneratorCallback(1)));
  EXPECT_CALL(*uploader_, SetRequestAndUpload(_, _))
      .WillOnce(RunOnceCallback<1>(ReportUploader::kSuccess));

  CreateScheduler();

  task_environment_.RunUntilIdle();

  // The timestamp should have been updated, since a periodic report was
  // generated/uploaded.
  ExpectLastUploadTimestampUpdated(true);

  // The last upload is now from this version.
  ExpectLastUploadVersion(chrome::kChromeVersion);

  histogram_tester_.ExpectUniqueSample(kUploadTriggerMetricName, 1, 1);
}

#endif  // !defined(OS_CHROMEOS)

}  // namespace enterprise_reporting
