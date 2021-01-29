// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/enterprise/browser/reporting/report_scheduler.h"

#include <utility>

#include "base/callback_helpers.h"
#include "base/test/gmock_callback_support.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/scoped_feature_list.h"
#include "build/build_config.h"
#include "build/chromeos_buildflags.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/enterprise/reporting/extension_request/extension_request_report_throttler.h"
#include "chrome/browser/enterprise/reporting/prefs.h"
#include "chrome/browser/enterprise/reporting/report_scheduler_desktop.h"
#include "chrome/browser/enterprise/reporting/reporting_delegate_factory_desktop.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/upgrade_detector/build_state.h"
#include "chrome/common/chrome_constants.h"
#include "chrome/common/chrome_features.h"
#include "chrome/common/pref_names.h"
#include "chrome/test/base/scoped_testing_local_state.h"
#include "chrome/test/base/testing_browser_process.h"
#include "chrome/test/base/testing_profile_manager.h"
#include "components/enterprise/browser/controller/fake_browser_dm_token_storage.h"
#include "components/enterprise/browser/reporting/common_pref_names.h"
#include "components/enterprise/browser/reporting/report_generator.h"
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

constexpr char kUploadTriggerMetricName[] =
    "Enterprise.CloudReportingUploadTrigger";

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
  explicit MockReportGenerator(
      ReportingDelegateFactoryDesktop* delegate_factory)
      : ReportGenerator(delegate_factory) {}
  void Generate(ReportType report_type, ReportCallback callback) override {
    OnGenerate(report_type, callback);
  }
  MOCK_METHOD2(OnGenerate,
               void(ReportType report_type, ReportCallback& callback));
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
    scoped_feature_list_.InitAndEnableFeature(
        features::kEnterpriseRealtimeExtensionRequest);
    ASSERT_TRUE(profile_manager_.SetUp());
    client_ptr_ = std::make_unique<policy::MockCloudPolicyClient>();
    client_ = client_ptr_.get();
    generator_ptr_ =
        std::make_unique<MockReportGenerator>(&report_delegate_factory_);
    generator_ = generator_ptr_.get();
    uploader_ptr_ = std::make_unique<MockReportUploader>();
    uploader_ = uploader_ptr_.get();
#if !BUILDFLAG(IS_CHROMEOS_ASH)
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
    scheduler_ = std::make_unique<ReportScheduler>(
        client_, std::move(generator_ptr_), &report_delegate_factory_);
    scheduler_->SetReportUploaderForTesting(std::move(uploader_ptr_));
  }

  void SetLastUploadInHour(base::TimeDelta gap) {
    previous_set_last_upload_timestamp_ = base::Time::Now() - gap;
    local_state_.Get()->SetTime(kLastUploadTimestamp,
                                previous_set_last_upload_timestamp_);
  }

  void ToggleCloudReport(bool enabled) {
    local_state_.Get()->SetManagedPref(kCloudReportingEnabled,
                                       std::make_unique<base::Value>(enabled));
  }

#if !BUILDFLAG(IS_CHROMEOS_ASH)
  void SetLastUploadVersion(const std::string& version) {
    local_state_.Get()->SetString(kLastUploadVersion, version);
  }

  void ExpectLastUploadVersion(const std::string& version) {
    EXPECT_EQ(local_state_.Get()->GetString(kLastUploadVersion), version);
  }
#endif  // !BUILDFLAG(IS_CHROMEOS_ASH)

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
#if BUILDFLAG(IS_CHROMEOS_ASH)
    EXPECT_CALL(*client_, SetupRegistration(_, _, _)).Times(0);
#else
    EXPECT_CALL(*client_, SetupRegistration(kDMToken, kClientId, _));
#endif
  }

  void EXPECT_CALL_SetupRegistrationWithSetDMToken() {
#if BUILDFLAG(IS_CHROMEOS_ASH)
    EXPECT_CALL(*client_, SetupRegistration(_, _, _)).Times(0);
#else
    EXPECT_CALL(*client_, SetupRegistration(kDMToken, kClientId, _))
        .WillOnce(WithArgs<0>(
            Invoke(client_, &policy::MockCloudPolicyClient::SetDMToken)));
#endif
  }

  void TriggerExtensionRequestReport() {
    ASSERT_TRUE(ExtensionRequestReportThrottler::Get());
    ASSERT_TRUE(ExtensionRequestReportThrottler::Get()->IsEnabled());
    ExtensionRequestReportThrottler::Get()->AddProfile(
        profile_manager_.CreateTestingProfile("profile")->GetPath());
  }

  base::test::ScopedFeatureList scoped_feature_list_;
  content::BrowserTaskEnvironment task_environment_;
  ScopedTestingLocalState local_state_;
  TestingProfileManager profile_manager_;

  ReportingDelegateFactoryDesktop report_delegate_factory_;
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

class ReportSchedulerFeatureTest : public ::testing::WithParamInterface<bool>,
                                   public ReportSchedulerTest {
  void SetUp() override {
    ReportSchedulerTest::SetUp();
    if (is_realtime_feature_enabled()) {
      scoped_feature_list_.Reset();
      scoped_feature_list_.Init();
    }
  }

  bool is_realtime_feature_enabled() { return GetParam(); }
};

INSTANTIATE_TEST_SUITE_P(ReportSchedulerTest,
                         ReportSchedulerFeatureTest,
                         ::testing::Bool());

TEST_P(ReportSchedulerFeatureTest, NoReportWithoutPolicy) {
  Init(false, kDMToken, kClientId);
  CreateScheduler();
  EXPECT_FALSE(scheduler_->IsNextReportScheduledForTesting());
}

// Chrome OS needn't set dm token and client id in the report scheduler.
#if !BUILDFLAG(IS_CHROMEOS_ASH)
TEST_P(ReportSchedulerFeatureTest, NoReportWithoutDMToken) {
  Init(true, "", kClientId);
  CreateScheduler();
  EXPECT_FALSE(scheduler_->IsNextReportScheduledForTesting());
}

TEST_P(ReportSchedulerFeatureTest, NoReportWithoutClientId) {
  Init(true, kDMToken, "");
  CreateScheduler();
  EXPECT_FALSE(scheduler_->IsNextReportScheduledForTesting());
}
#endif

TEST_P(ReportSchedulerFeatureTest, UploadReportSucceeded) {
  EXPECT_CALL_SetupRegistration();
  EXPECT_CALL(*generator_, OnGenerate(ReportType::kFull, _))
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

TEST_P(ReportSchedulerFeatureTest, UploadReportTransientError) {
  EXPECT_CALL_SetupRegistration();
  EXPECT_CALL(*generator_, OnGenerate(ReportType::kFull, _))
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

TEST_P(ReportSchedulerFeatureTest, UploadReportPersistentError) {
  EXPECT_CALL_SetupRegistrationWithSetDMToken();
  EXPECT_CALL(*generator_, OnGenerate(ReportType::kFull, _))
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

TEST_P(ReportSchedulerFeatureTest, NoReportGenerate) {
  EXPECT_CALL_SetupRegistrationWithSetDMToken();
  EXPECT_CALL(*generator_, OnGenerate(ReportType::kFull, _))
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

TEST_P(ReportSchedulerFeatureTest, TimerDelayWithLastUploadTimestamp) {
  const base::TimeDelta gap = base::TimeDelta::FromHours(10);
  SetLastUploadInHour(gap);

  EXPECT_CALL_SetupRegistration();
  EXPECT_CALL(*generator_, OnGenerate(ReportType::kFull, _))
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

TEST_P(ReportSchedulerFeatureTest, TimerDelayWithoutLastUploadTimestamp) {
  EXPECT_CALL_SetupRegistration();
  EXPECT_CALL(*generator_, OnGenerate(ReportType::kFull, _))
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

TEST_P(ReportSchedulerFeatureTest,
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

TEST_P(ReportSchedulerFeatureTest, ReportingIsDisabledWhileNewReportIsPosted) {
  EXPECT_CALL_SetupRegistration();
  EXPECT_CALL(*generator_, OnGenerate(ReportType::kFull, _))
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

#if !BUILDFLAG(IS_CHROMEOS_ASH)

// Tests that a basic report is generated and uploaded when a browser update is
// detected.
TEST_P(ReportSchedulerFeatureTest, OnUpdate) {
  // Pretend that a periodic report was generated recently so that one isn't
  // kicked off during startup.
  SetLastUploadInHour(base::TimeDelta::FromHours(1));
  EXPECT_CALL_SetupRegistration();
  EXPECT_CALL(*generator_, OnGenerate(ReportType::kBrowserVersion, _))
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

TEST_P(ReportSchedulerFeatureTest, OnUpdateAndPersistentError) {
  // Pretend that a periodic report was generated recently so that one isn't
  // kicked off during startup.
  SetLastUploadInHour(base::TimeDelta::FromHours(1));
  EXPECT_CALL_SetupRegistration();
  EXPECT_CALL(*generator_, OnGenerate(ReportType::kBrowserVersion, _))
      .WillOnce(WithArgs<1>(ScheduleGeneratorCallback(1)));
  EXPECT_CALL(*uploader_, SetRequestAndUpload(_, _))
      .WillOnce(RunOnceCallback<1>(ReportUploader::kPersistentError));

  CreateScheduler();
  g_browser_process->GetBuildState()->SetUpdate(
      BuildState::UpdateType::kNormalUpdate,
      base::Version("1" + version_info::GetVersionNumber()), base::nullopt);
  task_environment_.RunUntilIdle();

  // The timestamp should not have been updated, since a periodic report was not
  // generated/uploaded.
  ExpectLastUploadTimestampUpdated(false);

  histogram_tester_.ExpectUniqueSample(kUploadTriggerMetricName, 2, 1);

  // The report should be stopped in case of persistent error.
  g_browser_process->GetBuildState()->SetUpdate(
      BuildState::UpdateType::kNormalUpdate,
      base::Version("2" + version_info::GetVersionNumber()), base::nullopt);
  histogram_tester_.ExpectUniqueSample(kUploadTriggerMetricName, 2, 1);
}

// Tests that a full report is generated and uploaded following a basic report
// if the timer fires while the basic report is being uploaded.
TEST_P(ReportSchedulerFeatureTest, DeferredTimer) {
  EXPECT_CALL_SetupRegistration();
  CreateScheduler();

  // An update arrives, triggering report generation and upload (sans profiles).
  EXPECT_CALL(*generator_, OnGenerate(ReportType::kBrowserVersion, _))
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
  EXPECT_CALL(*generator_, OnGenerate(ReportType::kFull, _))
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
TEST_P(ReportSchedulerFeatureTest, OnNewVersion) {
  // Pretend that the last upload was from a different browser version.
  SetLastUploadVersion(chrome::kChromeVersion + std::string("1"));

  // Pretend that a periodic report was generated recently.
  SetLastUploadInHour(base::TimeDelta::FromHours(1));

  EXPECT_CALL_SetupRegistration();
  EXPECT_CALL(*generator_, OnGenerate(ReportType::kBrowserVersion, _))
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
TEST_P(ReportSchedulerFeatureTest, OnNewVersionRegularReport) {
  // Pretend that the last upload was from a different browser version.
  SetLastUploadVersion(chrome::kChromeVersion + std::string("1"));

  // Pretend that a periodic report was last generated over a day ago.
  SetLastUploadInHour(base::TimeDelta::FromHours(25));

  EXPECT_CALL_SetupRegistration();
  EXPECT_CALL(*generator_, OnGenerate(ReportType::kFull, _))
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

#endif  // !BUILDFLAG(IS_CHROMEOS_ASH)

TEST_F(ReportSchedulerTest, OnExtensionRequest) {
  SetLastUploadInHour(base::TimeDelta::FromHours(1));

  EXPECT_CALL_SetupRegistration();
  EXPECT_CALL(*generator_, OnGenerate(ReportType::kExtensionRequest, _))
      .WillOnce(WithArgs<1>(ScheduleGeneratorCallback(1)));
  EXPECT_CALL(*uploader_, SetRequestAndUpload(_, _))
      .WillOnce(RunOnceCallback<1>(ReportUploader::kSuccess));

  CreateScheduler();

  TriggerExtensionRequestReport();

  task_environment_.RunUntilIdle();

  // The timestamp should not have been updated, since a periodic report was not
  // generated/uploaded.
  ExpectLastUploadTimestampUpdated(false);

  histogram_tester_.ExpectUniqueSample(kUploadTriggerMetricName, 4, 1);
}

TEST_F(ReportSchedulerTest, OnExtensionRequestWithPersistentError) {
  base::TimeDelta last_report = base::TimeDelta::FromHours(23);
  SetLastUploadInHour(last_report);

  EXPECT_CALL_SetupRegistration();
  EXPECT_CALL(*generator_, OnGenerate(ReportType::kExtensionRequest, _))
      .WillOnce(WithArgs<1>(ScheduleGeneratorCallback(1)));
  EXPECT_CALL(*uploader_, SetRequestAndUpload(_, _))
      .WillOnce(RunOnceCallback<1>(ReportUploader::kPersistentError));

  CreateScheduler();

  TriggerExtensionRequestReport();

  task_environment_.RunUntilIdle();

  ExpectLastUploadTimestampUpdated(false);
  EXPECT_FALSE(ExtensionRequestReportThrottler::Get()->IsEnabled());
  histogram_tester_.ExpectUniqueSample(kUploadTriggerMetricName, 4, 1);

  ::testing::Mock::VerifyAndClearExpectations(uploader_);
  ::testing::Mock::VerifyAndClearExpectations(generator_);

  EXPECT_CALL(*generator_, OnGenerate(_, _)).Times(0);
  EXPECT_CALL(*uploader_, SetRequestAndUpload(_, _)).Times(0);

  // Persistent error also stops regular reports.
  task_environment_.FastForwardBy(kDefaultUploadInterval);

  ::testing::Mock::VerifyAndClearExpectations(uploader_);
  ::testing::Mock::VerifyAndClearExpectations(generator_);
}

#if !BUILDFLAG(IS_CHROMEOS_ASH)

TEST_F(ReportSchedulerTest, OnExtensionRequestAndUpdate) {
  SetLastUploadInHour(base::TimeDelta::FromHours(1));

  ReportUploader::ReportCallback saved_callback;
  auto new_uploader = std::make_unique<MockReportUploader>();

  EXPECT_CALL_SetupRegistration();
  EXPECT_CALL(*generator_, OnGenerate(ReportType::kExtensionRequest, _))
      .WillOnce(WithArgs<1>(ScheduleGeneratorCallback(1)));
  EXPECT_CALL(*generator_, OnGenerate(ReportType::kBrowserVersion, _))
      .WillOnce(WithArgs<1>(ScheduleGeneratorCallback(1)));

  EXPECT_CALL(*uploader_, SetRequestAndUpload(_, _))
      .WillOnce([&saved_callback](ReportUploader::ReportRequests requests,
                                  ReportUploader::ReportCallback callback) {
        saved_callback = std::move(callback);
      });

  CreateScheduler();

  g_browser_process->GetBuildState()->SetUpdate(
      BuildState::UpdateType::kNormalUpdate,
      base::Version("1" + version_info::GetVersionNumber()), base::nullopt);
  TriggerExtensionRequestReport();

  task_environment_.RunUntilIdle();

  // Release the first request and set uploader for the second request.
  EXPECT_CALL(*new_uploader, SetRequestAndUpload(_, _))
      .WillOnce(RunOnceCallback<1>(ReportUploader::kSuccess));
  std::move(saved_callback).Run(ReportUploader::kSuccess);
  uploader_ = new_uploader.get();
  scheduler_->SetReportUploaderForTesting(std::move(new_uploader));

  task_environment_.RunUntilIdle();

  // The timestamp should not have been updated, since a periodic report was not
  // generated/uploaded.
  ExpectLastUploadTimestampUpdated(false);

  histogram_tester_.ExpectTotalCount(kUploadTriggerMetricName, 2);
  histogram_tester_.ExpectBucketCount(kUploadTriggerMetricName, 2, 1);
  histogram_tester_.ExpectBucketCount(kUploadTriggerMetricName, 4, 1);
}

#endif  // !BUILDFLAG(IS_CHROMEOS_ASH)

}  // namespace enterprise_reporting
