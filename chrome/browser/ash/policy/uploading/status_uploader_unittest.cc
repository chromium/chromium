// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/policy/uploading/status_uploader.h"

#include <memory>
#include <tuple>
#include <utility>

#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/functional/callback_helpers.h"
#include "base/memory/raw_ptr.h"
#include "base/run_loop.h"
#include "base/test/gmock_move_support.h"
#include "base/test/task_environment.h"
#include "base/test/test_simple_task_runner.h"
#include "base/time/time.h"
#include "chrome/browser/ash/policy/core/device_local_account.h"
#include "chrome/browser/ash/policy/status_collector/device_status_collector.h"
#include "chrome/browser/ash/settings/scoped_testing_cros_settings.h"
#include "chrome/browser/ash/settings/stub_cros_settings_provider.h"
#include "chromeos/ash/components/settings/cros_settings_names.h"
#include "chromeos/dbus/power/power_manager_client.h"
#include "chromeos/dbus/tpm_manager/tpm_manager_client.h"
#include "components/policy/core/common/cloud/cloud_policy_client.h"
#include "components/policy/core/common/cloud/mock_cloud_policy_client.h"
#include "components/policy/core/common/device_local_account_type.h"
#include "components/prefs/testing_pref_service.h"
#include "components/session_manager/core/session_manager.h"
#include "content/public/test/browser_task_environment.h"
#include "content/public/test/test_utils.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/base/user_activity/user_activity_detector.h"
#include "ui/events/event.h"
#include "ui/events/event_utils.h"
#include "ui/events/platform/platform_event_source.h"
#include "ui/events/platform_event.h"

namespace policy {

namespace {

constexpr base::TimeDelta kDefaultStatusUploadDelay = base::Hours(1);
constexpr base::TimeDelta kMinImmediateUploadInterval = base::Seconds(10);
constexpr base::TimeDelta kMinimumScreenshotIdlenessCutoff = base::Minutes(5);

// Using a DeviceStatusCollector to have a concrete StatusCollector, but the
// exact type doesn't really matter, as it is being mocked.
class MockDeviceStatusCollector : public DeviceStatusCollector {
 public:
  explicit MockDeviceStatusCollector(PrefService* local_state)
      : DeviceStatusCollector(local_state, nullptr, nullptr, nullptr) {}
  MOCK_METHOD1(GetStatusAsync, void(StatusCollectorCallback));

  MOCK_METHOD0(OnSubmittedSuccessfully, void());

  // Explicit mock implementation declared here, since gmock::Invoke can't
  // handle returning non-moveable types like scoped_ptr.
  std::unique_ptr<DeviceLocalAccount> GetAutoLaunchedKioskSessionInfo()
      override {
    return std::make_unique<DeviceLocalAccount>(
        DeviceLocalAccountType::kKioskApp,
        policy::DeviceLocalAccount::EphemeralMode::kUnset, "account_id",
        "app_id", "update_url");
  }
};

}  // namespace

class StatusUploaderTest : public testing::Test {
 public:
  StatusUploaderTest() : task_runner_(new base::TestSimpleTaskRunner()) {
    DeviceStatusCollector::RegisterPrefs(prefs_.registry());
  }

  void SetUp() override {
    // Required for `DeviceStatusCollector`.
    chromeos::PowerManagerClient::InitializeFake();
    chromeos::TpmManagerClient::InitializeFake();
    client_.SetDMToken("dm_token");
    collector_ = std::make_unique<MockDeviceStatusCollector>(&prefs_);

    // Keep a pointer to the mock collector because collector_ gets cleared
    // when it is passed to the StatusUploader constructor.
    collector_ptr_ = collector_.get();
  }

  void TearDown() override {
    content::RunAllTasksUntilIdle();
    chromeos::TpmManagerClient::Shutdown();
    chromeos::PowerManagerClient::Shutdown();
  }

  // Given a pending task to upload status, runs the task and returns the
  // callback waiting to get device status / session status. The status upload
  // task will be blocked until the test code calls that callback.
  StatusCollectorCallback CollectStatusCallback() {
    // Running the task should pass a callback into
    // GetStatusAsync. We'll grab this callback.
    EXPECT_TRUE(task_runner_->HasPendingTask());
    StatusCollectorCallback status_callback;
    EXPECT_CALL(*collector_ptr_, GetStatusAsync)
        .WillOnce(MoveArg<0>(&status_callback));
    task_runner_->RunPendingTasks();

    return status_callback;
  }

  // Given a pending task to upload status, mocks out a server response.
  void RunPendingUploadTaskAndCheckNext(const StatusUploader& uploader,
                                        base::TimeDelta expected_delay,
                                        bool upload_success) {
    StatusCollectorCallback status_callback = CollectStatusCallback();

    // Running the status collected callback should trigger
    // CloudPolicyClient::UploadDeviceStatus.
    CloudPolicyClient::ResultCallback callback;
    EXPECT_CALL(client_, UploadDeviceStatus).WillOnce(MoveArg<3>(&callback));

    // Send some "valid" (read: non-nullptr) device/session data to the
    // callback in order to simulate valid status data.
    StatusCollectorParams status_params;
    std::move(status_callback).Run(std::move(status_params));

    // Make sure no status upload is queued up yet (since an upload is in
    // progress).
    EXPECT_FALSE(task_runner_->HasPendingTask());

    // StatusUpdater is only supposed to tell DeviceStatusCollector to clear its
    // caches if the status upload succeeded.
    EXPECT_CALL(*collector_ptr_, OnSubmittedSuccessfully())
        .Times(upload_success ? 1 : 0);

    // Now invoke the response.
    std::move(callback).Run(
        upload_success ? CloudPolicyClient::Result(DM_STATUS_SUCCESS)
                       : CloudPolicyClient::Result(DM_STATUS_REQUEST_FAILED));

    // Now that the previous request was satisfied, a task to do the next
    // upload should be queued.
    EXPECT_EQ(1U, task_runner_->NumPendingTasks());

    CheckPendingTaskDelay(uploader, expected_delay,
                          task_runner_->NextPendingTaskDelay());
  }

  void CheckPendingTaskDelay(const StatusUploader& uploader,
                             base::TimeDelta expected_delay,
                             base::TimeDelta task_delay) {
    // The next task should be scheduled sometime between |last_upload| +
    // |expected_delay| and |now| + |expected_delay|.
    base::Time now = base::Time::NowFromSystemTime();
    base::Time next_task = now + task_delay;

    EXPECT_LE(next_task, now + expected_delay);
    EXPECT_GE(next_task, uploader.last_upload() + expected_delay);
  }

  std::unique_ptr<StatusUploader> CreateStatusUploader() {
    return std::make_unique<StatusUploader>(&client_, std::move(collector_),
                                            task_runner_,
                                            kDefaultStatusUploadDelay);
  }

  void MockUserInput() {
    ui::MouseEvent e(ui::EventType::kMousePressed, gfx::Point(), gfx::Point(),
                     ui::EventTimeForNow(), 0, 0);
    const ui::PlatformEvent& native_event = &e;
    ui::UserActivityDetector::Get()->DidProcessEvent(native_event);
  }

  content::BrowserTaskEnvironment task_environment_{
      content::BrowserTaskEnvironment::TimeSource::MOCK_TIME};
  scoped_refptr<base::TestSimpleTaskRunner> task_runner_;
  ash::ScopedTestingCrosSettings scoped_testing_cros_settings_;
  std::unique_ptr<MockDeviceStatusCollector> collector_;
  raw_ptr<MockDeviceStatusCollector, DanglingUntriaged> collector_ptr_;
  MockCloudPolicyClient client_;
  TestingPrefServiceSimple prefs_;
  // This property is required to instantiate the session manager, a singleton
  // which is used by the device status collector.
  session_manager::SessionManager session_manager_;
};

TEST_F(StatusUploaderTest, BasicTest) {
  EXPECT_FALSE(task_runner_->HasPendingTask());
  auto uploader = CreateStatusUploader();
  EXPECT_EQ(1U, task_runner_->NumPendingTasks());
  // On startup, first update should happen in 1 minute.
  EXPECT_EQ(base::Minutes(1), task_runner_->NextPendingTaskDelay());
}

TEST_F(StatusUploaderTest, DifferentFrequencyAtStart) {
  const base::TimeDelta new_delay = kDefaultStatusUploadDelay * 2;

  scoped_testing_cros_settings_.device_settings()->SetInteger(
      ash::kReportUploadFrequency, new_delay.InMilliseconds());
  EXPECT_FALSE(task_runner_->HasPendingTask());
  auto uploader = CreateStatusUploader();
  ASSERT_EQ(1U, task_runner_->NumPendingTasks());
  // On startup, first update should happen in 1 minute.
  EXPECT_EQ(base::Minutes(1), task_runner_->NextPendingTaskDelay());

  // Second update should use the delay specified in settings.
  RunPendingUploadTaskAndCheckNext(*uploader, new_delay,
                                   true /* upload_success */);
}

TEST_F(StatusUploaderTest, ResetTimerAfterStatusCollection) {
  auto uploader = CreateStatusUploader();
  RunPendingUploadTaskAndCheckNext(*uploader, kDefaultStatusUploadDelay,
                                   true /* upload_success */);

  // Handle this response also, and ensure new task is queued.
  RunPendingUploadTaskAndCheckNext(*uploader, kDefaultStatusUploadDelay,
                                   true /* upload_success */);

  // Now that the previous request was satisfied, a task to do the next
  // upload should be queued again.
  EXPECT_EQ(1U, task_runner_->NumPendingTasks());
}

TEST_F(StatusUploaderTest, ResetTimerAfterFailedStatusCollection) {
  auto uploader = CreateStatusUploader();

  // Running the queued task should pass a callback into
  // GetStatusAsync. We'll grab this callback and send nullptrs
  // to it in order to simulate failure to get status.
  EXPECT_EQ(1U, task_runner_->NumPendingTasks());
  StatusCollectorCallback status_callback;
  EXPECT_CALL(*collector_ptr_, GetStatusAsync)
      .WillOnce(MoveArg<0>(&status_callback));
  task_runner_->RunPendingTasks();

  // Running the callback should trigger StatusUploader::OnStatusReceived, which
  // in turn should recognize the failure to get status and queue another status
  // upload.
  StatusCollectorParams status_params;
  status_params.device_status.reset();
  status_params.session_status.reset();
  status_params.child_status.reset();
  std::move(status_callback).Run(std::move(status_params));
  EXPECT_EQ(1U, task_runner_->NumPendingTasks());

  // Check the delay of the queued upload
  CheckPendingTaskDelay(*uploader, kDefaultStatusUploadDelay,
                        task_runner_->NextPendingTaskDelay());
}

TEST_F(StatusUploaderTest, ResetTimerAfterUploadError) {
  auto uploader = CreateStatusUploader();

  // Simulate upload error
  RunPendingUploadTaskAndCheckNext(*uploader, kDefaultStatusUploadDelay,
                                   false /* upload_success */);

  // Now that the previous request was satisfied, a task to do the next
  // upload should be queued again.
  EXPECT_EQ(1U, task_runner_->NumPendingTasks());
}

TEST_F(StatusUploaderTest, ResetTimerAfterUnregisteredClient) {
  auto uploader = CreateStatusUploader();

  client_.SetDMToken("");
  EXPECT_FALSE(client_.is_registered());

  StatusCollectorCallback status_callback = CollectStatusCallback();

  // Running the status collected callback should trigger
  // CloudPolicyClient::UploadDeviceStatus.
  CloudPolicyClient::ResultCallback callback;
  EXPECT_CALL(client_, UploadDeviceStatus).WillOnce(MoveArg<3>(&callback));

  // Send some "valid" (read: non-nullptr) device/session data to the
  // callback in order to simulate valid status data.
  StatusCollectorParams status_params;
  std::move(status_callback).Run(std::move(status_params));

  // Make sure no status upload is queued up yet (since an upload is in
  // progress).
  EXPECT_FALSE(task_runner_->HasPendingTask());

  // No successful submit will happen if not registered.
  EXPECT_CALL(*collector_ptr_, OnSubmittedSuccessfully()).Times(0);

  // Now invoke the response.
  std::move(callback).Run(
      CloudPolicyClient::Result(CloudPolicyClient::NotRegistered()));

  // A task to try again should be queued.
  ASSERT_EQ(1U, task_runner_->NumPendingTasks());

  CheckPendingTaskDelay(*uploader, kDefaultStatusUploadDelay,
                        task_runner_->NextPendingTaskDelay());
}

TEST_F(StatusUploaderTest, ChangeFrequency) {
  auto uploader = CreateStatusUploader();
  // Change the frequency. The new frequency should be reflected in the timing
  // used for the next callback.
  const base::TimeDelta new_delay = kDefaultStatusUploadDelay * 2;
  scoped_testing_cros_settings_.device_settings()->SetInteger(
      ash::kReportUploadFrequency, new_delay.InMilliseconds());
  RunPendingUploadTaskAndCheckNext(*uploader, new_delay,
                                   true /* upload_success */);
}

TEST_F(StatusUploaderTest, ScreenshotUploadAllowOnlyAfterCutoffTime) {
  auto uploader = CreateStatusUploader();
  // Should allow data upload before there is user input.
  EXPECT_TRUE(uploader->IsScreenshotAllowed());

  MockUserInput();
  EXPECT_FALSE(uploader->IsScreenshotAllowed());

  MockUserInput();
  task_environment_.AdvanceClock(kMinimumScreenshotIdlenessCutoff -
                                 base::Seconds(1));
  EXPECT_FALSE(uploader->IsScreenshotAllowed());

  // Screenshot is allowed again after a period of inactivity
  MockUserInput();
  task_environment_.AdvanceClock(kMinimumScreenshotIdlenessCutoff +
                                 base::Seconds(1));
  EXPECT_TRUE(uploader->IsScreenshotAllowed());
}

TEST_F(StatusUploaderTest, NoUploadAfterVideoCapture) {
  auto uploader = CreateStatusUploader();
  // Should allow data upload before there is video capture.
  EXPECT_TRUE(uploader->IsScreenshotAllowed());

  // Now mock video capture, and no session data should be allowed.
  MediaCaptureDevicesDispatcher::GetInstance()->OnMediaRequestStateChanged(
      0, 0, 0, GURL("http://www.google.com"),
      blink::mojom::MediaStreamType::DEVICE_VIDEO_CAPTURE,
      content::MEDIA_REQUEST_STATE_OPENING);
  base::RunLoop().RunUntilIdle();
  EXPECT_FALSE(uploader->IsScreenshotAllowed());
}

TEST_F(StatusUploaderTest, ScheduleImmediateStatusUpload) {
  EXPECT_FALSE(task_runner_->HasPendingTask());
  auto uploader = CreateStatusUploader();
  EXPECT_EQ(1U, task_runner_->NumPendingTasks());

  // On startup, first update should happen in 1 minute.
  EXPECT_EQ(base::Minutes(1), task_runner_->NextPendingTaskDelay());

  // Schedule an immediate status upload.
  uploader->ScheduleNextStatusUploadImmediately();
  EXPECT_EQ(2U, task_runner_->NumPendingTasks());
  CheckPendingTaskDelay(*uploader, base::TimeDelta(),
                        task_runner_->FinalPendingTaskDelay());
}

TEST_F(StatusUploaderTest, ScheduleImmediateStatusUploadConsecutively) {
  EXPECT_FALSE(task_runner_->HasPendingTask());
  auto uploader = CreateStatusUploader();
  EXPECT_EQ(1U, task_runner_->NumPendingTasks());

  // On startup, first update should happen in 1 minute.
  EXPECT_EQ(base::Minutes(1), task_runner_->NextPendingTaskDelay());

  // Schedule an immediate status upload and run it.
  uploader->ScheduleNextStatusUploadImmediately();
  RunPendingUploadTaskAndCheckNext(*uploader, kDefaultStatusUploadDelay,
                                   true /* upload_success */);

  // Schedule the next one and check that it was scheduled after
  // kMinImmediateUploadInterval of the last upload.
  uploader->ScheduleNextStatusUploadImmediately();
  EXPECT_EQ(2U, task_runner_->NumPendingTasks());
  CheckPendingTaskDelay(*uploader, kMinImmediateUploadInterval,
                        task_runner_->FinalPendingTaskDelay());
}

}  // namespace policy
