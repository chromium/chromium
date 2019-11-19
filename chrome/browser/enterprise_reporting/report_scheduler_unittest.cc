// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/enterprise_reporting/report_scheduler.h"

#include <utility>

#include "base/test/gmock_callback_support.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/scoped_feature_list.h"
#include "chrome/browser/enterprise_reporting/prefs.h"
#include "chrome/browser/enterprise_reporting/request_timer.h"
#include "chrome/browser/policy/fake_browser_dm_token_storage.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/common/chrome_features.h"
#include "chrome/common/pref_names.h"
#include "chrome/test/base/scoped_testing_local_state.h"
#include "chrome/test/base/testing_browser_process.h"
#include "chrome/test/base/testing_profile_manager.h"
#include "components/policy/core/common/cloud/mock_cloud_policy_client.h"
#include "content/public/test/browser_task_environment.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using ::base::test::RunOnceCallback;
using ::testing::_;
using ::testing::Invoke;
using ::testing::WithArgs;

namespace em = enterprise_management;

namespace enterprise_reporting {
namespace {
constexpr char kDMToken[] = "dm_token";
constexpr char kClientId[] = "client_id";
constexpr char kStaleProfileCountMetricsName[] =
    "Enterprise.CloudReportingStaleProfileCount";
const int kDefaultUploadInterval = 24;
}  // namespace
class FakeRequestTimer : public RequestTimer {
 public:
  FakeRequestTimer() = default;
  ~FakeRequestTimer() override = default;
  void Start(const base::Location& posted_from,
             base::TimeDelta first_delay,
             base::TimeDelta repeat_delay,
             base::RepeatingClosure user_task) override {
    first_delay_ = first_delay;
    repeat_delay_ = repeat_delay;
    user_task_ = user_task;
    is_running_ = true;
  }

  void Stop() override { is_running_ = false; }

  void Reset() override { is_running_ = true; }

  void Fire() {
    EXPECT_TRUE(is_running_);
    user_task_.Run();
    is_running_ = false;
  }

  bool is_running() { return is_running_; }

  base::TimeDelta first_delay() { return first_delay_; }

  base::TimeDelta repeat_delay() { return repeat_delay_; }

 private:
  base::TimeDelta first_delay_;
  base::TimeDelta repeat_delay_;
  base::RepeatingClosure user_task_;
  bool is_running_ = false;
  DISALLOW_COPY_AND_ASSIGN(FakeRequestTimer);
};

ACTION_P(ScheduleGeneratorCallback, request_number) {
  ReportGenerator::Requests requests;
  for (int i = 0; i < request_number; i++)
    requests.push(std::make_unique<ReportGenerator::Request>());
  base::ThreadTaskRunnerHandle::Get()->PostTask(
      FROM_HERE, base::BindOnce(std::move(arg0), std::move(requests)));
}

class MockReportGenerator : public ReportGenerator {
 public:
  void Generate(ReportCallback callback) override { OnGenerate(callback); }
  MOCK_METHOD1(OnGenerate, void(ReportCallback& callback));
};

class MockReportUploader : public ReportUploader {
 public:
  MockReportUploader() : ReportUploader(nullptr, 0) {}
  ~MockReportUploader() override = default;
  void SetRequestAndUpload(Requests requests,
                           ReportCallback callback) override {
    OnSetRequestAndUpload(requests, callback);
  }
  MOCK_METHOD2(OnSetRequestAndUpload,
               void(Requests& requests, ReportCallback& callback));

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
    scoped_feature_list_.InitAndEnableFeature(
        features::kEnterpriseReportingInBrowser);
    ASSERT_TRUE(profile_manager_.SetUp());
    client_ptr_ = std::make_unique<policy::MockCloudPolicyClient>();
    client_ = client_ptr_.get();
    timer_ptr_ = std::make_unique<FakeRequestTimer>();
    timer_ = timer_ptr_.get();
    generator_ptr_ = std::make_unique<MockReportGenerator>();
    generator_ = generator_ptr_.get();
    uploader_ptr_ = std::make_unique<MockReportUploader>();
    uploader_ = uploader_ptr_.get();
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
    scheduler_ = std::make_unique<ReportScheduler>(std::move(client_ptr_),
                                                   std::move(timer_ptr_),
                                                   std::move(generator_ptr_));
    scheduler_->SetReportUploaderForTesting(std::move(uploader_ptr_));
  }

  void SetLastUploadInHour(int gap) {
    previous_set_last_upload_timestamp_ =
        base::Time::Now() - base::TimeDelta::FromHours(gap);
    local_state_.Get()->SetTime(kLastUploadTimestamp,
                                previous_set_last_upload_timestamp_);
  }

  void ToggleCloudReport(bool enabled) {
    local_state_.Get()->SetManagedPref(prefs::kCloudReportingEnabled,
                                       std::make_unique<base::Value>(enabled));
  }

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

  ReportGenerator::Requests CreateRequests(int number) {
    ReportGenerator::Requests requests;
    for (int i = 0; i < number; i++)
      requests.push(std::make_unique<ReportGenerator::Request>());
    return requests;
  }

  base::test::ScopedFeatureList scoped_feature_list_;
  content::BrowserTaskEnvironment task_environment_;
  ScopedTestingLocalState local_state_;
  TestingProfileManager profile_manager_;

  std::unique_ptr<ReportScheduler> scheduler_;
  policy::MockCloudPolicyClient* client_;
  FakeRequestTimer* timer_;
  MockReportGenerator* generator_;
  MockReportUploader* uploader_;
  policy::FakeBrowserDMTokenStorage storage_;
  base::Time previous_set_last_upload_timestamp_;
  base::HistogramTester histogram_tester_;

 private:
  std::unique_ptr<policy::MockCloudPolicyClient> client_ptr_;
  std::unique_ptr<FakeRequestTimer> timer_ptr_;
  std::unique_ptr<MockReportGenerator> generator_ptr_;
  std::unique_ptr<MockReportUploader> uploader_ptr_;
  DISALLOW_COPY_AND_ASSIGN(ReportSchedulerTest);
};

TEST_F(ReportSchedulerTest, NoReportWithoutPolicy) {
  Init(false, kDMToken, kClientId);
  CreateScheduler();
  EXPECT_FALSE(timer_->is_running());
}

TEST_F(ReportSchedulerTest, NoReportWithoutDMToken) {
  Init(true, "", kClientId);
  CreateScheduler();
  EXPECT_FALSE(timer_->is_running());
}

TEST_F(ReportSchedulerTest, NoReportWithoutClientId) {
  Init(true, kDMToken, "");
  CreateScheduler();
  EXPECT_FALSE(timer_->is_running());
}

TEST_F(ReportSchedulerTest, UploadReportSucceeded) {
  EXPECT_CALL(*client_, SetupRegistration(kDMToken, kClientId, _));
  EXPECT_CALL(*generator_, OnGenerate(_))
      .WillOnce(WithArgs<0>(ScheduleGeneratorCallback(1)));
  EXPECT_CALL(*uploader_, OnSetRequestAndUpload(_, _))
      .WillOnce(RunOnceCallback<1>(ReportUploader::kSuccess));

  CreateScheduler();
  EXPECT_TRUE(timer_->is_running());

  timer_->Fire();

  // timer is paused until the report is finished.
  EXPECT_FALSE(timer_->is_running());

  // Run pending task.
  task_environment_.FastForwardBy(base::TimeDelta());

  // Next report is scheduled.
  EXPECT_TRUE(timer_->is_running());
  ExpectLastUploadTimestampUpdated(true);

  ::testing::Mock::VerifyAndClearExpectations(client_);
  ::testing::Mock::VerifyAndClearExpectations(generator_);
}

TEST_F(ReportSchedulerTest, UploadReportTransientError) {
  EXPECT_CALL(*client_, SetupRegistration(kDMToken, kClientId, _));
  EXPECT_CALL(*generator_, OnGenerate(_))
      .WillOnce(WithArgs<0>(ScheduleGeneratorCallback(1)));
  EXPECT_CALL(*uploader_, OnSetRequestAndUpload(_, _))
      .WillOnce(RunOnceCallback<1>(ReportUploader::kTransientError));

  CreateScheduler();
  EXPECT_TRUE(timer_->is_running());

  timer_->Fire();

  // timer is paused until the report is finished.
  EXPECT_FALSE(timer_->is_running());

  // Run pending task.
  task_environment_.FastForwardBy(base::TimeDelta());

  // Next report is scheduled.
  EXPECT_TRUE(timer_->is_running());
  ExpectLastUploadTimestampUpdated(true);

  ::testing::Mock::VerifyAndClearExpectations(client_);
  ::testing::Mock::VerifyAndClearExpectations(generator_);
}

TEST_F(ReportSchedulerTest, UploadReportPersistentError) {
  EXPECT_CALL(*client_, SetupRegistration(kDMToken, kClientId, _))
      .WillOnce(WithArgs<0>(
          Invoke(client_, &policy::MockCloudPolicyClient::SetDMToken)));
  EXPECT_CALL(*generator_, OnGenerate(_))
      .WillOnce(WithArgs<0>(ScheduleGeneratorCallback(1)));
  EXPECT_CALL(*uploader_, OnSetRequestAndUpload(_, _))
      .WillOnce(RunOnceCallback<1>(ReportUploader::kPersistentError));

  CreateScheduler();
  EXPECT_TRUE(timer_->is_running());

  timer_->Fire();

  // timer is paused until the report is finished.
  EXPECT_FALSE(timer_->is_running());

  // Run pending task.
  task_environment_.FastForwardBy(base::TimeDelta());

  // Next report is not scheduled.
  EXPECT_FALSE(timer_->is_running());
  ExpectLastUploadTimestampUpdated(false);

  // Turn off and on reporting to resume.
  ToggleCloudReport(false);
  ToggleCloudReport(true);
  EXPECT_TRUE(timer_->is_running());

  ::testing::Mock::VerifyAndClearExpectations(client_);
  ::testing::Mock::VerifyAndClearExpectations(generator_);
}

TEST_F(ReportSchedulerTest, NoReportGenerate) {
  EXPECT_CALL(*client_, SetupRegistration(kDMToken, kClientId, _))
      .WillOnce(WithArgs<0>(
          Invoke(client_, &policy::MockCloudPolicyClient::SetDMToken)));
  EXPECT_CALL(*generator_, OnGenerate(_))
      .WillOnce(WithArgs<0>(ScheduleGeneratorCallback(0)));
  EXPECT_CALL(*uploader_, OnSetRequestAndUpload(_, _)).Times(0);

  CreateScheduler();
  EXPECT_TRUE(timer_->is_running());

  timer_->Fire();

  // timer is paused until the report is finished.
  EXPECT_FALSE(timer_->is_running());

  // Run pending task.
  task_environment_.FastForwardBy(base::TimeDelta());

  // Next report is not scheduled.
  EXPECT_FALSE(timer_->is_running());
  ExpectLastUploadTimestampUpdated(false);

  // Turn off and on reporting to resume.
  ToggleCloudReport(false);
  ToggleCloudReport(true);
  EXPECT_TRUE(timer_->is_running());

  ::testing::Mock::VerifyAndClearExpectations(client_);
  ::testing::Mock::VerifyAndClearExpectations(generator_);
}

TEST_F(ReportSchedulerTest, TimerDelayWithLastUploadTimestamp) {
  int gap = 10;
  SetLastUploadInHour(gap);

  EXPECT_CALL(*client_, SetupRegistration(kDMToken, kClientId, _));

  CreateScheduler();
  EXPECT_TRUE(timer_->is_running());

  EXPECT_EQ(base::TimeDelta::FromHours(kDefaultUploadInterval - gap),
            timer_->first_delay());
  EXPECT_EQ(base::TimeDelta::FromHours(kDefaultUploadInterval),
            timer_->repeat_delay());

  ::testing::Mock::VerifyAndClearExpectations(client_);
  ::testing::Mock::VerifyAndClearExpectations(generator_);
}

TEST_F(ReportSchedulerTest, TimerDelayWithoutLastUploadTimestamp) {
  EXPECT_CALL(*client_, SetupRegistration(kDMToken, kClientId, _));

  CreateScheduler();
  EXPECT_TRUE(timer_->is_running());

  EXPECT_GT(base::TimeDelta(), timer_->first_delay());
  EXPECT_EQ(base::TimeDelta::FromHours(kDefaultUploadInterval),
            timer_->repeat_delay());

  ::testing::Mock::VerifyAndClearExpectations(client_);
}

TEST_F(ReportSchedulerTest,
       ReportingIsDisabledWhileNewReportIsScheduledButNotPosted) {
  EXPECT_CALL(*client_, SetupRegistration(kDMToken, kClientId, _));

  CreateScheduler();
  EXPECT_TRUE(timer_->is_running());

  ToggleCloudReport(false);

  // Next report is not scheduled.
  EXPECT_FALSE(timer_->is_running());
  ExpectLastUploadTimestampUpdated(false);

  ::testing::Mock::VerifyAndClearExpectations(client_);
  ::testing::Mock::VerifyAndClearExpectations(generator_);
}

TEST_F(ReportSchedulerTest, ReportingIsDisabledWhileNewReportIsPosted) {
  EXPECT_CALL(*client_, SetupRegistration(kDMToken, kClientId, _));
  EXPECT_CALL(*generator_, OnGenerate(_))
      .WillOnce(WithArgs<0>(ScheduleGeneratorCallback(1)));
  EXPECT_CALL(*uploader_, OnSetRequestAndUpload(_, _))
      .WillOnce(RunOnceCallback<1>(ReportUploader::kSuccess));

  CreateScheduler();
  EXPECT_TRUE(timer_->is_running());

  timer_->Fire();
  ToggleCloudReport(false);
  EXPECT_FALSE(timer_->is_running());

  // Run pending task.
  task_environment_.FastForwardBy(base::TimeDelta());

  ExpectLastUploadTimestampUpdated(true);
  // Next report is not scheduled.
  EXPECT_FALSE(timer_->is_running());

  ::testing::Mock::VerifyAndClearExpectations(client_);
  ::testing::Mock::VerifyAndClearExpectations(generator_);
}

TEST_F(ReportSchedulerTest, NoStaleProfileMetricsForSystemAndGuestProfile) {
  EXPECT_CALL(*client_, SetupRegistration(kDMToken, kClientId, _));
  CreateScheduler();
  // Does not record for system or guest profile.
  profile_manager_.CreateSystemProfile();
  profile_manager_.CreateGuestProfile();
  scheduler_.reset();
  histogram_tester_.ExpectTotalCount(kStaleProfileCountMetricsName, 0);
}

TEST_F(ReportSchedulerTest, NoStaleProfileMetricsBeforeFirstReport) {
  EXPECT_CALL(*client_, SetupRegistration(kDMToken, kClientId, _));
  CreateScheduler();
  profile_manager_.CreateTestingProfile("profile1");
  scheduler_.reset();
  histogram_tester_.ExpectTotalCount(kStaleProfileCountMetricsName, 0);
}

TEST_F(ReportSchedulerTest, StaleProfileMetricsForProfileAdded) {
  EXPECT_CALL(*client_, SetupRegistration(kDMToken, kClientId, _));
  EXPECT_CALL(*generator_, OnGenerate(_))
      .WillOnce(WithArgs<0>(ScheduleGeneratorCallback(1)));
  EXPECT_CALL(*uploader_, OnSetRequestAndUpload(_, _))
      .WillOnce(RunOnceCallback<1>(ReportUploader::kSuccess));

  CreateScheduler();
  timer_->Fire();
  task_environment_.FastForwardBy(base::TimeDelta());

  profile_manager_.CreateTestingProfile("profile1");

  scheduler_.reset();
  histogram_tester_.ExpectUniqueSample(kStaleProfileCountMetricsName, 1, 1);
}

TEST_F(ReportSchedulerTest, StaleProfileMetricsForProfileRemoved) {
  EXPECT_CALL(*client_, SetupRegistration(kDMToken, kClientId, _));
  EXPECT_CALL(*generator_, OnGenerate(_))
      .WillOnce(WithArgs<0>(ScheduleGeneratorCallback(1)));
  EXPECT_CALL(*uploader_, OnSetRequestAndUpload(_, _))
      .WillOnce(RunOnceCallback<1>(ReportUploader::kSuccess));

  // Create a profile a head of time to prevent default profile creation during
  // profile deleting.
  profile_manager_.CreateTestingProfile("profile0");
  CreateScheduler();
  timer_->Fire();
  task_environment_.FastForwardBy(base::TimeDelta());

  TestingProfile* profile = profile_manager_.CreateTestingProfile("profile1");
  profile_manager_.profile_manager()->ScheduleProfileForDeletion(
      profile->GetPath(), base::DoNothing());
  task_environment_.FastForwardBy(base::TimeDelta());

  scheduler_.reset();
  histogram_tester_.ExpectUniqueSample(kStaleProfileCountMetricsName, 0, 1);
}

TEST_F(ReportSchedulerTest, StaleProfileMetricsResetAfterNewUpload) {
  EXPECT_CALL(*client_, SetupRegistration(kDMToken, kClientId, _));
  EXPECT_CALL(*generator_, OnGenerate(_))
      .WillRepeatedly(WithArgs<0>(ScheduleGeneratorCallback(1)));
  EXPECT_CALL(*uploader_, OnSetRequestAndUpload(_, _))
      .WillOnce(RunOnceCallback<1>(ReportUploader::kSuccess));

  CreateScheduler();
  timer_->Fire();
  task_environment_.FastForwardBy(base::TimeDelta());

  // Re-assign uploader for the second upload
  auto new_uploader = std::make_unique<MockReportUploader>();
  uploader_ = new_uploader.get();
  scheduler_->SetReportUploaderForTesting(std::move(new_uploader));
  EXPECT_CALL(*uploader_, OnSetRequestAndUpload(_, _))
      .WillOnce(RunOnceCallback<1>(ReportUploader::kSuccess));

  profile_manager_.CreateTestingProfile("profile1");

  timer_->Fire();
  task_environment_.FastForwardBy(base::TimeDelta());

  scheduler_.reset();
  histogram_tester_.ExpectUniqueSample(kStaleProfileCountMetricsName, 0, 1);
}

}  // namespace enterprise_reporting
