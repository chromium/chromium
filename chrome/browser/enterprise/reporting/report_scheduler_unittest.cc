// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/enterprise/browser/reporting/report_scheduler.h"

#include <utility>

#include "base/functional/callback_helpers.h"
#include "base/memory/raw_ptr.h"
#include "base/strings/strcat.h"
#include "base/task/single_thread_task_runner.h"
#include "base/test/gmock_callback_support.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/mock_callback.h"
#include "base/time/time.h"
#include "build/build_config.h"
#include "build/chromeos_buildflags.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/enterprise/reporting/prefs.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/upgrade_detector/build_state.h"
#include "chrome/common/chrome_constants.h"
#include "chrome/common/pref_names.h"
#include "chrome/test/base/scoped_testing_local_state.h"
#include "chrome/test/base/testing_browser_process.h"
#include "chrome/test/base/testing_profile_manager.h"
#include "components/enterprise/browser/controller/fake_browser_dm_token_storage.h"
#include "components/enterprise/browser/reporting/chrome_profile_request_generator.h"
#include "components/enterprise/browser/reporting/common_pref_names.h"
#include "components/enterprise/browser/reporting/report_generator.h"
#include "components/enterprise/browser/reporting/report_request.h"
#include "components/policy/core/common/cloud/mock_cloud_policy_client.h"
#include "components/reporting/client/report_queue_provider.h"
#include "components/reporting/proto/synced/record_constants.pb.h"
#include "components/sync_preferences/testing_pref_service_syncable.h"
#include "components/version_info/version_info.h"
#include "content/public/test/browser_task_environment.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

#if BUILDFLAG(IS_ANDROID)
#include "chrome/browser/enterprise/reporting/report_scheduler_android.h"
#include "chrome/browser/enterprise/reporting/reporting_delegate_factory_android.h"
#else
#include "chrome/browser/enterprise/reporting/report_scheduler_desktop.h"
#include "chrome/browser/enterprise/reporting/reporting_delegate_factory_desktop.h"
#endif  // BUILDFLAG(IS_ANDROID)

using ::base::test::RunOnceCallback;
using ::testing::_;
using ::testing::ByMove;
using ::testing::DoAll;
using ::testing::Invoke;
using ::testing::InvokeWithoutArgs;
using ::testing::Return;
using ::testing::WithArgs;

namespace em = enterprise_management;
namespace enterprise_reporting {

namespace {

constexpr char kDMToken[] = "dm_token";
constexpr char kClientId[] = "client_id";
constexpr base::TimeDelta kUploadFrequency = base::Hours(12);
constexpr base::TimeDelta kNewUploadFrequency = base::Hours(10);

constexpr char kUploadTriggerMetricName[] =
    "Enterprise.CloudReportingUploadTrigger";

}  // namespace

ACTION_P(ScheduleGeneratorCallback, request_number) {
  ReportRequestQueue requests;
  for (int i = 0; i < request_number; i++)
    requests.push(std::make_unique<ReportRequest>(ReportType::kFull));
  base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE, base::BindOnce(std::move(arg0), std::move(requests)));
}

ACTION(ScheduleProfileRequestGeneratorCallback) {
  ReportRequestQueue requests;
  requests.push(std::make_unique<ReportRequest>(ReportType::kProfileReport));
  base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE, base::BindOnce(std::move(arg0), std::move(requests)));
}

class MockReportGenerator : public ReportGenerator {
 public:
#if BUILDFLAG(IS_ANDROID)
  explicit MockReportGenerator(
      ReportingDelegateFactoryAndroid* delegate_factory)
      : ReportGenerator(delegate_factory) {}
#else
  explicit MockReportGenerator(
      ReportingDelegateFactoryDesktop* delegate_factory)
      : ReportGenerator(delegate_factory) {}
#endif  // BUILDFLAG(IS_ANDROID)
  void Generate(ReportType report_type, ReportCallback callback) override {
    OnGenerate(report_type, callback);
  }
  MOCK_METHOD2(OnGenerate,
               void(ReportType report_type, ReportCallback& callback));
  MOCK_METHOD0(GenerateBasic, ReportRequestQueue());
};

class MockReportUploader : public ReportUploader {
 public:
  MockReportUploader() : ReportUploader(nullptr, 0) {}

  MockReportUploader(const MockReportUploader&) = delete;
  MockReportUploader& operator=(const MockReportUploader&) = delete;

  ~MockReportUploader() override = default;
  MOCK_METHOD3(SetRequestAndUpload,
               void(ReportType, ReportRequestQueue, ReportCallback));
};

class MockChromeProfileRequestGenerator : public ChromeProfileRequestGenerator {
 public:
#if BUILDFLAG(IS_ANDROID)
  explicit MockChromeProfileRequestGenerator(
      ReportingDelegateFactoryAndroid* delegate_factory)
#else
  explicit MockChromeProfileRequestGenerator(
      ReportingDelegateFactoryDesktop* delegate_factory)
#endif  // BUILDFLAG(IS_ANDROID)
      : ChromeProfileRequestGenerator(/*profile_path=*/base::FilePath(),
                                      /*profile_name*/ std::string(),
                                      delegate_factory) {
  }
  void Generate(ReportCallback callback) override { OnGenerate(callback); }
  MOCK_METHOD1(OnGenerate, void(ReportCallback& callback));
};

class ReportSchedulerTest : public ::testing::Test {
 public:
  ReportSchedulerTest()
      : task_environment_(base::test::TaskEnvironment::TimeSource::MOCK_TIME),
        local_state_(TestingBrowserProcess::GetGlobal()),
        profile_manager_(TestingBrowserProcess::GetGlobal(), &local_state_) {}

  ReportSchedulerTest(const ReportSchedulerTest&) = delete;
  ReportSchedulerTest& operator=(const ReportSchedulerTest&) = delete;

  ~ReportSchedulerTest() override = default;

  void SetUp() override {
    ASSERT_TRUE(profile_manager_.SetUp());
    client_ptr_ = std::make_unique<policy::MockCloudPolicyClient>();
    client_ = client_ptr_.get();
    generator_ptr_ =
        std::make_unique<MockReportGenerator>(&report_delegate_factory_);
    generator_ = generator_ptr_.get();
    uploader_ptr_ = std::make_unique<MockReportUploader>();
    uploader_ = uploader_ptr_.get();

    profile_request_generator_ptr_ =
        std::make_unique<MockChromeProfileRequestGenerator>(
            &report_delegate_factory_);
    profile_request_generator_ = profile_request_generator_ptr_.get();

#if !BUILDFLAG(IS_CHROMEOS_ASH)
    SetLastUploadVersion(chrome::kChromeVersion);
#endif
    Init(true, kDMToken, kClientId);
  }

  void Init(bool policy_enabled,
            const std::string& dm_token,
            const std::string& client_id) {
    ToggleCloudReport(policy_enabled);
#if !BUILDFLAG(IS_CHROMEOS_ASH)
    storage_.SetDMToken(dm_token);
    storage_.SetClientId(client_id);
#endif
  }

  void CreateScheduler() {
    ReportScheduler::CreateParams params;
    params.client = client_;
    params.delegate = report_delegate_factory_.GetReportSchedulerDelegate();
    params.report_generator = std::move(generator_ptr_);
    scheduler_ = std::make_unique<ReportScheduler>(std::move(params));
    scheduler_->SetReportUploaderForTesting(std::move(uploader_ptr_));
  }

#if !BUILDFLAG(IS_CHROMEOS_ASH)
  void CreateSchedulerForProfileReporting(Profile* profile) {
    ReportScheduler::CreateParams params;
    params.client = client_;
    client_->SetDMToken("dm-token");
    params.delegate =
#if BUILDFLAG(IS_ANDROID)
        std::make_unique<ReportSchedulerAndroid>(profile);
#else
        std::make_unique<ReportSchedulerDesktop>(profile);
#endif  // BUILDFLAG(IS_ANDROID)
    params.profile_request_generator =
        std::move(profile_request_generator_ptr_);
    scheduler_ = std::make_unique<ReportScheduler>(std::move(params));
    scheduler_->SetReportUploaderForTesting(std::move(uploader_ptr_));
  }
#endif  // !BUILDFLAG(IS_CHROMEOS_ASH)

  void SetLastUploadInHour(base::TimeDelta gap) {
    previous_set_last_upload_timestamp_ = base::Time::Now() - gap;
    local_state_.Get()->SetTime(kLastUploadTimestamp,
                                previous_set_last_upload_timestamp_);
  }

  void SetReportFrequency(base::TimeDelta frequency) {
    local_state_.Get()->SetTimeDelta(kCloudReportingUploadFrequency, frequency);
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

  ReportRequestQueue CreateRequests(int number) {
    ReportRequestQueue requests;
    for (int i = 0; i < number; i++)
      requests.push(std::make_unique<ReportRequest>(ReportType::kFull));
    return requests;
  }

  // Chrome OS needn't setup registration.
  void EXPECT_CALL_SetupRegistration() {
#if BUILDFLAG(IS_CHROMEOS_ASH)
    EXPECT_CALL(*client_, SetupRegistration(_, _, _)).Times(0);
#else
    EXPECT_CALL(*client_, SetupRegistration(kDMToken, kClientId, _))
        .WillOnce(WithArgs<0>(
            Invoke(client_.get(), &policy::MockCloudPolicyClient::SetDMToken)));
#endif
  }

  content::BrowserTaskEnvironment task_environment_;
  ScopedTestingLocalState local_state_;
  TestingProfileManager profile_manager_;

#if BUILDFLAG(IS_ANDROID)
  ReportingDelegateFactoryAndroid report_delegate_factory_;
#else
  ReportingDelegateFactoryDesktop report_delegate_factory_;
#endif  // BUILDFLAG(IS_ANDROID)
  std::unique_ptr<ReportScheduler> scheduler_;
  raw_ptr<policy::MockCloudPolicyClient, DanglingUntriaged> client_;
  raw_ptr<MockReportGenerator, DanglingUntriaged> generator_;
  raw_ptr<MockReportUploader, DanglingUntriaged> uploader_;
  raw_ptr<MockChromeProfileRequestGenerator, DanglingUntriaged>
      profile_request_generator_;
#if !BUILDFLAG(IS_CHROMEOS_ASH)
  policy::FakeBrowserDMTokenStorage storage_;
#endif
  base::Time previous_set_last_upload_timestamp_;
  base::HistogramTester histogram_tester_;

 private:
  std::unique_ptr<policy::MockCloudPolicyClient> client_ptr_;
  std::unique_ptr<MockReportGenerator> generator_ptr_;
  std::unique_ptr<MockReportUploader> uploader_ptr_;
  std::unique_ptr<MockChromeProfileRequestGenerator>
      profile_request_generator_ptr_;
};

TEST_F(ReportSchedulerTest, NoReportWithoutPolicy) {
  Init(false, kDMToken, kClientId);
  CreateScheduler();
  EXPECT_FALSE(scheduler_->IsNextReportScheduledForTesting());
}

// Chrome OS needn't set dm token and client id in the report scheduler.
#if !BUILDFLAG(IS_CHROMEOS_ASH)
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
  EXPECT_CALL(*generator_, OnGenerate(ReportType::kFull, _))
      .WillOnce(WithArgs<1>(ScheduleGeneratorCallback(1)));
  EXPECT_CALL(*uploader_, SetRequestAndUpload(ReportType::kFull, _, _))
      .WillOnce(RunOnceCallback<2>(ReportUploader::kSuccess));

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

// Profile reporting does not support ash.
#if !BUILDFLAG(IS_CHROMEOS_ASH)
TEST_F(ReportSchedulerTest, UploadReportSucceededForProfileReporting) {
  EXPECT_CALL(*profile_request_generator_, OnGenerate(_))
      .WillOnce(WithArgs<0>(ScheduleProfileRequestGeneratorCallback()));
  EXPECT_CALL(*uploader_, SetRequestAndUpload(ReportType::kProfileReport, _, _))
      .WillOnce(RunOnceCallback<2>(ReportUploader::kSuccess));

  TestingProfile* profile = profile_manager_.CreateTestingProfile("profile");
  profile->GetTestingPrefService()->SetManagedPref(
      kCloudProfileReportingEnabled, std::make_unique<base::Value>(true));
  CreateSchedulerForProfileReporting(profile);
  EXPECT_TRUE(scheduler_->IsNextReportScheduledForTesting());

  // Run pending task.
  task_environment_.FastForwardBy(base::TimeDelta());

  // Next report is scheduled.
  EXPECT_TRUE(scheduler_->IsNextReportScheduledForTesting());
  auto current_last_upload_timestamp =
      profile->GetPrefs()->GetTime(kLastUploadTimestamp);
  EXPECT_EQ(base::Time::Now(), current_last_upload_timestamp);

  ::testing::Mock::VerifyAndClearExpectations(client_);
  ::testing::Mock::VerifyAndClearExpectations(profile_request_generator_);
}
#endif  // !BUILDFLAG(IS_CHROMEOS_ASH)

TEST_F(ReportSchedulerTest, UploadReportTransientError) {
  EXPECT_CALL_SetupRegistration();
  EXPECT_CALL(*generator_, OnGenerate(ReportType::kFull, _))
      .WillOnce(WithArgs<1>(ScheduleGeneratorCallback(1)));
  EXPECT_CALL(*uploader_, SetRequestAndUpload(ReportType::kFull, _, _))
      .WillOnce(RunOnceCallback<2>(ReportUploader::kTransientError));

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
  EXPECT_CALL_SetupRegistration();
  EXPECT_CALL(*generator_, OnGenerate(ReportType::kFull, _))
      .WillOnce(WithArgs<1>(ScheduleGeneratorCallback(1)));
  EXPECT_CALL(*uploader_, SetRequestAndUpload(ReportType::kFull, _, _))
      .WillOnce(RunOnceCallback<2>(ReportUploader::kPersistentError));

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
  EXPECT_CALL_SetupRegistration();
  EXPECT_CALL(*generator_, OnGenerate(ReportType::kFull, _))
      .WillOnce(WithArgs<1>(ScheduleGeneratorCallback(0)));
  EXPECT_CALL(*uploader_, SetRequestAndUpload(_, _, _)).Times(0);

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
  const base::TimeDelta gap = base::Hours(10);
  SetLastUploadInHour(gap);
  SetReportFrequency(kUploadFrequency);

  EXPECT_CALL_SetupRegistration();
  EXPECT_CALL(*generator_, OnGenerate(ReportType::kFull, _))
      .WillOnce(WithArgs<1>(ScheduleGeneratorCallback(1)));
  EXPECT_CALL(*uploader_, SetRequestAndUpload(ReportType::kFull, _, _))
      .WillOnce(RunOnceCallback<2>(ReportUploader::kSuccess));

  CreateScheduler();
  EXPECT_TRUE(scheduler_->IsNextReportScheduledForTesting());

  base::TimeDelta next_report_delay = kUploadFrequency - gap;
  task_environment_.FastForwardBy(next_report_delay - base::Seconds(1));
  ExpectLastUploadTimestampUpdated(false);
  task_environment_.FastForwardBy(base::Seconds(1));
  ExpectLastUploadTimestampUpdated(true);

  ::testing::Mock::VerifyAndClearExpectations(client_);
  ::testing::Mock::VerifyAndClearExpectations(generator_);
}

TEST_F(ReportSchedulerTest, TimerDelayWithoutLastUploadTimestamp) {
  EXPECT_CALL_SetupRegistration();
  EXPECT_CALL(*generator_, OnGenerate(ReportType::kFull, _))
      .WillOnce(WithArgs<1>(ScheduleGeneratorCallback(1)));
  EXPECT_CALL(*uploader_, SetRequestAndUpload(ReportType::kFull, _, _))
      .WillOnce(RunOnceCallback<2>(ReportUploader::kSuccess));

  CreateScheduler();
  EXPECT_TRUE(scheduler_->IsNextReportScheduledForTesting());

  ExpectLastUploadTimestampUpdated(false);
  task_environment_.FastForwardBy(base::TimeDelta());
  ExpectLastUploadTimestampUpdated(true);

  ::testing::Mock::VerifyAndClearExpectations(client_);
}

TEST_F(ReportSchedulerTest, TimerDelayUpdate) {
  const base::TimeDelta gap = base::Hours(5);
  SetLastUploadInHour(gap);
  SetReportFrequency(kUploadFrequency);

  EXPECT_CALL_SetupRegistration();
  EXPECT_CALL(*generator_, OnGenerate(ReportType::kFull, _))
      .WillOnce(WithArgs<1>(ScheduleGeneratorCallback(1)));
  EXPECT_CALL(*uploader_, SetRequestAndUpload(ReportType::kFull, _, _))
      .WillOnce(RunOnceCallback<2>(ReportUploader::kSuccess));

  CreateScheduler();
  EXPECT_TRUE(scheduler_->IsNextReportScheduledForTesting());

  SetReportFrequency(kNewUploadFrequency);

  // The report should be re-scheduled, moving the time forward with the new
  // interval.
  EXPECT_TRUE(scheduler_->IsNextReportScheduledForTesting());

  base::TimeDelta next_report_delay = kNewUploadFrequency - gap;
  task_environment_.FastForwardBy(next_report_delay - base::Seconds(1));
  ExpectLastUploadTimestampUpdated(false);
  task_environment_.FastForwardBy(base::Seconds(1));
  ExpectLastUploadTimestampUpdated(true);

  ::testing::Mock::VerifyAndClearExpectations(client_);
  ::testing::Mock::VerifyAndClearExpectations(generator_);
}

TEST_F(ReportSchedulerTest, IgnoreFrequencyWithoutReportEnabled) {
  Init(false, kDMToken, kClientId);
  CreateScheduler();
  EXPECT_FALSE(scheduler_->IsNextReportScheduledForTesting());

  SetReportFrequency(kUploadFrequency);
  EXPECT_FALSE(scheduler_->IsNextReportScheduledForTesting());

  // Toggle reporting on and off.
  EXPECT_CALL_SetupRegistration();
  ToggleCloudReport(true);
  ToggleCloudReport(false);

  EXPECT_FALSE(scheduler_->IsNextReportScheduledForTesting());

  SetReportFrequency(kNewUploadFrequency);

  EXPECT_FALSE(scheduler_->IsNextReportScheduledForTesting());
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
  EXPECT_CALL(*generator_, OnGenerate(ReportType::kFull, _))
      .WillOnce(WithArgs<1>(ScheduleGeneratorCallback(1)));
  EXPECT_CALL(*uploader_, SetRequestAndUpload(ReportType::kFull, _, _))
      .WillOnce(RunOnceCallback<2>(ReportUploader::kSuccess));

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

TEST_F(ReportSchedulerTest, ManualReport) {
  SetLastUploadInHour(base::Hours(1));
  EXPECT_CALL_SetupRegistration();

  EXPECT_CALL(*generator_, OnGenerate(ReportType::kFull, _))
      .WillOnce(WithArgs<1>(ScheduleGeneratorCallback(1)));
  EXPECT_CALL(*uploader_, SetRequestAndUpload(ReportType::kFull, _, _))
      .WillOnce(RunOnceCallback<2>(ReportUploader::kSuccess));

  CreateScheduler();

  base::MockOnceClosure callback;
  EXPECT_CALL(callback, Run()).Times(1);
  scheduler_->UploadFullReport(callback.Get());
  task_environment_.RunUntilIdle();

  ExpectLastUploadTimestampUpdated(true);
  histogram_tester_.ExpectUniqueSample(kUploadTriggerMetricName, 6, 1);
  ::testing::Mock::VerifyAndClearExpectations(generator_);
  ::testing::Mock::VerifyAndClearExpectations(uploader_);
}

TEST_F(ReportSchedulerTest, ScheduledReportAfterManualReport) {
  EXPECT_CALL_SetupRegistration();
  EXPECT_CALL(*generator_, OnGenerate(ReportType::kFull, _))
      .WillOnce(WithArgs<1>(ScheduleGeneratorCallback(1)));
  EXPECT_CALL(*uploader_, SetRequestAndUpload(ReportType::kFull, _, _))
      .WillOnce(RunOnceCallback<2>(ReportUploader::kSuccess));

  CreateScheduler();

  base::MockOnceClosure callback;
  EXPECT_CALL(callback, Run()).Times(1);

  // Trigger manual report first and then move forward time to trigger timer
  // report.
  scheduler_->UploadFullReport(callback.Get());
  task_environment_.RunUntilIdle();

  ExpectLastUploadTimestampUpdated(true);
  histogram_tester_.ExpectUniqueSample(kUploadTriggerMetricName, 6, 1);
  ::testing::Mock::VerifyAndClearExpectations(generator_);
  ::testing::Mock::VerifyAndClearExpectations(uploader_);
}

TEST_F(ReportSchedulerTest, ManualReportWithRegularOneOngoing) {
  EXPECT_CALL_SetupRegistration();
  EXPECT_CALL(*generator_, OnGenerate(ReportType::kFull, _))
      .WillOnce(WithArgs<1>(ScheduleGeneratorCallback(1)));

  // Callback for timer report will be hold.
  ReportUploader::ReportCallback saved_timer_callback;
  EXPECT_CALL(*uploader_, SetRequestAndUpload(ReportType::kFull, _, _))
      .WillOnce([&saved_timer_callback](
                    ReportType report_type, ReportRequestQueue requests,
                    ReportUploader::ReportCallback callback) {
        saved_timer_callback = std::move(callback);
      });
  CreateScheduler();
  // Trigger timer report first.
  task_environment_.RunUntilIdle();

  base::MockOnceClosure callback;
  EXPECT_CALL(callback, Run()).Times(1);

  // Trigger manual report and then release timer report callback.
  scheduler_->UploadFullReport(callback.Get());
  std::move(saved_timer_callback).Run(ReportUploader::kSuccess);
  task_environment_.RunUntilIdle();

  ExpectLastUploadTimestampUpdated(true);
  histogram_tester_.ExpectUniqueSample(kUploadTriggerMetricName, 1, 1);
  ::testing::Mock::VerifyAndClearExpectations(generator_);
  ::testing::Mock::VerifyAndClearExpectations(uploader_);
}

// Android does not support version updates
#if !BUILDFLAG(IS_ANDROID) && !BUILDFLAG(IS_CHROMEOS_ASH)

// Tests that a basic report is generated and uploaded when a browser update is
// detected.
TEST_F(ReportSchedulerTest, OnUpdate) {
  // Pretend that a periodic report was generated recently so that one isn't
  // kicked off during startup.
  SetLastUploadInHour(base::Hours(1));
  EXPECT_CALL_SetupRegistration();
  EXPECT_CALL(*generator_, OnGenerate(ReportType::kBrowserVersion, _))
      .WillOnce(WithArgs<1>(ScheduleGeneratorCallback(1)));
  EXPECT_CALL(*uploader_,
              SetRequestAndUpload(ReportType::kBrowserVersion, _, _))
      .WillOnce(RunOnceCallback<2>(ReportUploader::kSuccess));

  CreateScheduler();
  g_browser_process->GetBuildState()->SetUpdate(
      BuildState::UpdateType::kNormalUpdate,
      base::Version(base::StrCat({"1", version_info::GetVersionNumber()})),
      std::nullopt);
  task_environment_.RunUntilIdle();

  // The timestamp should not have been updated, since a periodic report was not
  // generated/uploaded.
  ExpectLastUploadTimestampUpdated(false);

  histogram_tester_.ExpectUniqueSample(kUploadTriggerMetricName, 2, 1);
}

TEST_F(ReportSchedulerTest, OnUpdateAndPersistentError) {
  // Pretend that a periodic report was generated recently so that one isn't
  // kicked off during startup.
  SetLastUploadInHour(base::Hours(1));
  EXPECT_CALL_SetupRegistration();
  EXPECT_CALL(*generator_, OnGenerate(ReportType::kBrowserVersion, _))
      .WillOnce(WithArgs<1>(ScheduleGeneratorCallback(1)));
  EXPECT_CALL(*uploader_,
              SetRequestAndUpload(ReportType::kBrowserVersion, _, _))
      .WillOnce(RunOnceCallback<2>(ReportUploader::kPersistentError));

  CreateScheduler();
  g_browser_process->GetBuildState()->SetUpdate(
      BuildState::UpdateType::kNormalUpdate,
      base::Version(base::StrCat({"1", version_info::GetVersionNumber()})),
      std::nullopt);
  task_environment_.RunUntilIdle();

  // The timestamp should not have been updated, since a periodic report was not
  // generated/uploaded.
  ExpectLastUploadTimestampUpdated(false);

  histogram_tester_.ExpectUniqueSample(kUploadTriggerMetricName, 2, 1);

  // The report should be stopped in case of persistent error.
  g_browser_process->GetBuildState()->SetUpdate(
      BuildState::UpdateType::kNormalUpdate,
      base::Version(base::StrCat({"2", version_info::GetVersionNumber()})),
      std::nullopt);
  histogram_tester_.ExpectUniqueSample(kUploadTriggerMetricName, 2, 1);
}

// Tests that a full report is generated and uploaded following a basic report
// if the timer fires while the basic report is being uploaded.
TEST_F(ReportSchedulerTest, DeferredTimer) {
  EXPECT_CALL_SetupRegistration();
  CreateScheduler();

  // An update arrives, triggering report generation and upload (sans profiles).
  EXPECT_CALL(*generator_, OnGenerate(ReportType::kBrowserVersion, _))
      .WillOnce(WithArgs<1>(ScheduleGeneratorCallback(1)));

  // Hang on to the uploader's ReportCallback.
  ReportUploader::ReportCallback saved_callback;
  EXPECT_CALL(*uploader_,
              SetRequestAndUpload(ReportType::kBrowserVersion, _, _))
      .WillOnce([&saved_callback](ReportType report_type,
                                  ReportRequestQueue requests,
                                  ReportUploader::ReportCallback callback) {
        saved_callback = std::move(callback);
      });

  g_browser_process->GetBuildState()->SetUpdate(
      BuildState::UpdateType::kNormalUpdate,
      base::Version(base::StrCat({"1", version_info::GetVersionNumber()})),
      std::nullopt);
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
  EXPECT_CALL(*new_uploader, SetRequestAndUpload(ReportType::kFull, _, _))
      .WillOnce(RunOnceCallback<2>(ReportUploader::kSuccess));
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
  SetLastUploadInHour(base::Hours(1));

  EXPECT_CALL_SetupRegistration();
  EXPECT_CALL(*generator_, OnGenerate(ReportType::kBrowserVersion, _))
      .WillOnce(WithArgs<1>(ScheduleGeneratorCallback(1)));
  EXPECT_CALL(*uploader_,
              SetRequestAndUpload(ReportType::kBrowserVersion, _, _))
      .WillOnce(RunOnceCallback<2>(ReportUploader::kSuccess));

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
  SetLastUploadInHour(base::Hours(25));

  EXPECT_CALL_SetupRegistration();
  EXPECT_CALL(*generator_, OnGenerate(ReportType::kFull, _))
      .WillOnce(WithArgs<1>(ScheduleGeneratorCallback(1)));
  EXPECT_CALL(*uploader_, SetRequestAndUpload(ReportType::kFull, _, _))
      .WillOnce(RunOnceCallback<2>(ReportUploader::kSuccess));

  CreateScheduler();

  task_environment_.RunUntilIdle();

  // The timestamp should have been updated, since a periodic report was
  // generated/uploaded.
  ExpectLastUploadTimestampUpdated(true);

  // The last upload is now from this version.
  ExpectLastUploadVersion(chrome::kChromeVersion);

  histogram_tester_.ExpectUniqueSample(kUploadTriggerMetricName, 1, 1);
}

#endif  // !BUILDFLAG(IS_ANDROID) && !BUILDFLAG(IS_CHROMEOS_ASH)

}  // namespace enterprise_reporting
