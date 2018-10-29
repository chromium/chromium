// Copyright (c) 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/policy/status_uploader.h"

#include <utility>

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/callback.h"
#include "base/run_loop.h"
#include "base/test/test_simple_task_runner.h"
#include "base/time/time.h"
#include "chrome/browser/chromeos/policy/device_local_account.h"
#include "chrome/browser/chromeos/policy/device_status_collector.h"
#include "chrome/browser/chromeos/settings/scoped_cros_settings_test_helper.h"
#include "chromeos/dbus/dbus_thread_manager.h"
#include "chromeos/settings/cros_settings_names.h"
#include "components/policy/core/common/cloud/cloud_policy_client.h"
#include "components/policy/core/common/cloud/mock_cloud_policy_client.h"
#include "components/policy/core/common/cloud/mock_device_management_service.h"
#include "components/prefs/testing_pref_service.h"
#include "components/session_manager/core/session_manager.h"
#include "content/public/test/test_browser_thread_bundle.h"
#include "content/public/test/test_utils.h"
#include "net/url_request/url_request_context_getter.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/base/user_activity/user_activity_detector.h"
#include "ui/events/event.h"
#include "ui/events/event_utils.h"
#include "ui/events/platform/platform_event_source.h"
#include "ui/events/platform_event.h"

using ::testing::_;
using ::testing::Invoke;
using ::testing::Return;
using ::testing::SaveArg;
using ::testing::WithArgs;

namespace em = enterprise_management;

namespace {

constexpr base::TimeDelta kDefaultStatusUploadDelay =
    base::TimeDelta::FromHours(1);

class MockDeviceStatusCollector : public policy::DeviceStatusCollector {
 public:
  explicit MockDeviceStatusCollector(PrefService* local_state)
      : DeviceStatusCollector(
            local_state,
            nullptr,
            policy::DeviceStatusCollector::VolumeInfoFetcher(),
            policy::DeviceStatusCollector::CPUStatisticsFetcher(),
            policy::DeviceStatusCollector::CPUTempFetcher(),
            policy::DeviceStatusCollector::AndroidStatusFetcher(),
            policy::DeviceStatusCollector::TpmStatusFetcher(),
            base::TimeDelta(), /* Day starts at midnight */
            true /* is_enterprise_device */) {}

  MOCK_METHOD1(GetDeviceAndSessionStatusAsync,
               void(const policy::DeviceStatusCollector::StatusCallback&));

  MOCK_METHOD0(OnSubmittedSuccessfully, void());

  // Explicit mock implementation declared here, since gmock::Invoke can't
  // handle returning non-moveable types like scoped_ptr.
  std::unique_ptr<policy::DeviceLocalAccount> GetAutoLaunchedKioskSessionInfo()
      override {
    return std::make_unique<policy::DeviceLocalAccount>(
        policy::DeviceLocalAccount::TYPE_KIOSK_APP, "account_id", "app_id",
        "update_url");
  }
};

}  // namespace

namespace policy {
class StatusUploaderTest : public testing::Test {
 public:
  StatusUploaderTest() : task_runner_(new base::TestSimpleTaskRunner()) {
    DeviceStatusCollector::RegisterPrefs(prefs_.registry());
  }

  void SetUp() override {
    chromeos::DBusThreadManager::Initialize();
    client_.SetDMToken("dm_token");
    collector_.reset(new MockDeviceStatusCollector(&prefs_));
    settings_helper_.ReplaceDeviceSettingsProviderWithStub();

    // Keep a pointer to the mock collector because collector_ gets cleared
    // when it is passed to the StatusUploader constructor.
    collector_ptr_ = collector_.get();
  }

  void TearDown() override {
    content::RunAllTasksUntilIdle();
    chromeos::DBusThreadManager::Shutdown();
  }

  // Given a pending task to upload status, runs the task and returns the
  // callback waiting to get device status / session status. The status upload
  // task will be blocked until the test code calls that callback.
  DeviceStatusCollector::StatusCallback CollectStatusCallback() {
    // Running the task should pass a callback into
    // GetDeviceAndSessionStatusAsync. We'll grab this callback.
    EXPECT_TRUE(task_runner_->HasPendingTask());
    DeviceStatusCollector::StatusCallback status_callback;
    EXPECT_CALL(*collector_ptr_, GetDeviceAndSessionStatusAsync(_))
        .WillOnce(SaveArg<0>(&status_callback));
    task_runner_->RunPendingTasks();
    testing::Mock::VerifyAndClearExpectations(&device_management_service_);

    return status_callback;
  }

  // Given a pending task to upload status, mocks out a server response.
  void RunPendingUploadTaskAndCheckNext(const StatusUploader& uploader,
                                        base::TimeDelta expected_delay,
                                        bool upload_success) {
    DeviceStatusCollector::StatusCallback status_callback =
        CollectStatusCallback();

    // Running the status collected callback should trigger
    // CloudPolicyClient::UploadDeviceStatus.
    CloudPolicyClient::StatusCallback callback;
    EXPECT_CALL(client_, UploadDeviceStatus(_, _, _))
        .WillOnce(SaveArg<2>(&callback));

    // Send some "valid" (read: non-nullptr) device/session data to the
    // callback in order to simulate valid status data.
    std::unique_ptr<em::DeviceStatusReportRequest> device_status =
        std::make_unique<em::DeviceStatusReportRequest>();
    std::unique_ptr<em::SessionStatusReportRequest> session_status =
        std::make_unique<em::SessionStatusReportRequest>();
    status_callback.Run(std::move(device_status), std::move(session_status));

    testing::Mock::VerifyAndClearExpectations(&device_management_service_);

    // Make sure no status upload is queued up yet (since an upload is in
    // progress).
    EXPECT_FALSE(task_runner_->HasPendingTask());

    // StatusUpdater is only supposed to tell DeviceStatusCollector to clear its
    // caches if the status upload succeeded.
    EXPECT_CALL(*collector_ptr_, OnSubmittedSuccessfully())
        .Times(upload_success ? 1 : 0);

    // Now invoke the response.
    callback.Run(upload_success);

    // Now that the previous request was satisfied, a task to do the next
    // upload should be queued.
    EXPECT_EQ(1U, task_runner_->NumPendingTasks());

    CheckPendingTaskDelay(uploader, expected_delay);
  }

  void CheckPendingTaskDelay(const StatusUploader& uploader,
                             base::TimeDelta expected_delay) {
    // The next task should be scheduled sometime between |last_upload| +
    // |expected_delay| and |now| + |expected_delay|.
    base::Time now = base::Time::NowFromSystemTime();
    base::Time next_task = now + task_runner_->NextPendingTaskDelay();

    EXPECT_LE(next_task, now + expected_delay);
    EXPECT_GE(next_task, uploader.last_upload() + expected_delay);
  }

  content::TestBrowserThreadBundle thread_bundle_;
  scoped_refptr<base::TestSimpleTaskRunner> task_runner_;
  chromeos::ScopedCrosSettingsTestHelper settings_helper_;
  std::unique_ptr<MockDeviceStatusCollector> collector_;
  MockDeviceStatusCollector* collector_ptr_;
  ui::UserActivityDetector detector_;
  MockCloudPolicyClient client_;
  MockDeviceManagementService device_management_service_;
  TestingPrefServiceSimple prefs_;
  // This property is required to instantiate the session manager, a singleton
  // which is used by the device status collector.
  session_manager::SessionManager session_manager_;
};

TEST_F(StatusUploaderTest, BasicTest) {
  EXPECT_FALSE(task_runner_->HasPendingTask());
  StatusUploader uploader(&client_, std::move(collector_), task_runner_,
                          kDefaultStatusUploadDelay);
  EXPECT_EQ(1U, task_runner_->NumPendingTasks());
  // On startup, first update should happen in 1 minute.
  EXPECT_EQ(base::TimeDelta::FromMinutes(1),
            task_runner_->NextPendingTaskDelay());
}

TEST_F(StatusUploaderTest, DifferentFrequencyAtStart) {
  const base::TimeDelta new_delay = kDefaultStatusUploadDelay * 2;
  settings_helper_.SetInteger(chromeos::kReportUploadFrequency,
                              new_delay.InMilliseconds());
  EXPECT_FALSE(task_runner_->HasPendingTask());
  StatusUploader uploader(&client_, std::move(collector_), task_runner_,
                          kDefaultStatusUploadDelay);
  ASSERT_EQ(1U, task_runner_->NumPendingTasks());
  // On startup, first update should happen in 1 minute.
  EXPECT_EQ(base::TimeDelta::FromMinutes(1),
            task_runner_->NextPendingTaskDelay());

  // Second update should use the delay specified in settings.
  RunPendingUploadTaskAndCheckNext(uploader, new_delay,
                                   true /* upload_success */);
}

TEST_F(StatusUploaderTest, ResetTimerAfterStatusCollection) {
  StatusUploader uploader(&client_, std::move(collector_), task_runner_,
                          kDefaultStatusUploadDelay);
  RunPendingUploadTaskAndCheckNext(uploader, kDefaultStatusUploadDelay,
                                   true /* upload_success */);

  // Handle this response also, and ensure new task is queued.
  RunPendingUploadTaskAndCheckNext(uploader, kDefaultStatusUploadDelay,
                                   true /* upload_success */);

  // Now that the previous request was satisfied, a task to do the next
  // upload should be queued again.
  EXPECT_EQ(1U, task_runner_->NumPendingTasks());
}

TEST_F(StatusUploaderTest, ResetTimerAfterFailedStatusCollection) {
  StatusUploader uploader(&client_, std::move(collector_), task_runner_,
                          kDefaultStatusUploadDelay);

  // Running the queued task should pass a callback into
  // GetDeviceAndSessionStatusAsync. We'll grab this callback and send nullptrs
  // to it in order to simulate failure to get status.
  EXPECT_EQ(1U, task_runner_->NumPendingTasks());
  DeviceStatusCollector::StatusCallback status_callback;
  EXPECT_CALL(*collector_ptr_, GetDeviceAndSessionStatusAsync(_))
      .WillOnce(SaveArg<0>(&status_callback));
  task_runner_->RunPendingTasks();
  testing::Mock::VerifyAndClearExpectations(&device_management_service_);

  // Running the callback should trigger StatusUploader::OnStatusReceived, which
  // in turn should recognize the failure to get status and queue another status
  // upload.
  std::unique_ptr<em::DeviceStatusReportRequest> invalid_device_status;
  std::unique_ptr<em::SessionStatusReportRequest> invalid_session_status;
  status_callback.Run(std::move(invalid_device_status),
                      std::move(invalid_session_status));
  EXPECT_EQ(1U, task_runner_->NumPendingTasks());

  // Check the delay of the queued upload
  CheckPendingTaskDelay(uploader, kDefaultStatusUploadDelay);
}

TEST_F(StatusUploaderTest, ResetTimerAfterUploadError) {
  StatusUploader uploader(&client_, std::move(collector_), task_runner_,
                          kDefaultStatusUploadDelay);

  // Simulate upload error
  RunPendingUploadTaskAndCheckNext(uploader, kDefaultStatusUploadDelay,
                                   false /* upload_success */);

  // Now that the previous request was satisfied, a task to do the next
  // upload should be queued again.
  EXPECT_EQ(1U, task_runner_->NumPendingTasks());
}

TEST_F(StatusUploaderTest, ResetTimerAfterUnregisteredClient) {
  StatusUploader uploader(&client_, std::move(collector_), task_runner_,
                          kDefaultStatusUploadDelay);

  client_.SetDMToken("");
  EXPECT_FALSE(client_.is_registered());

  DeviceStatusCollector::StatusCallback status_callback =
      CollectStatusCallback();

  // Make sure no status upload is queued up yet (since an upload is in
  // progress).
  EXPECT_FALSE(task_runner_->HasPendingTask());

  // StatusUploader should not try to upload using an unregistered client
  EXPECT_CALL(client_, UploadDeviceStatus(_, _, _)).Times(0);
  std::unique_ptr<em::DeviceStatusReportRequest> device_status =
      std::make_unique<em::DeviceStatusReportRequest>();
  std::unique_ptr<em::SessionStatusReportRequest> session_status =
      std::make_unique<em::SessionStatusReportRequest>();
  status_callback.Run(std::move(device_status), std::move(session_status));

  // A task to try again should be queued.
  ASSERT_EQ(1U, task_runner_->NumPendingTasks());

  CheckPendingTaskDelay(uploader, kDefaultStatusUploadDelay);
}

TEST_F(StatusUploaderTest, ChangeFrequency) {
  StatusUploader uploader(&client_, std::move(collector_), task_runner_,
                          kDefaultStatusUploadDelay);
  // Change the frequency. The new frequency should be reflected in the timing
  // used for the next callback.
  const base::TimeDelta new_delay = kDefaultStatusUploadDelay * 2;
  settings_helper_.SetInteger(chromeos::kReportUploadFrequency,
                              new_delay.InMilliseconds());
  RunPendingUploadTaskAndCheckNext(uploader, new_delay,
                                   true /* upload_success */);
}

TEST_F(StatusUploaderTest, NoUploadAfterUserInput) {
  StatusUploader uploader(&client_, std::move(collector_), task_runner_,
                          kDefaultStatusUploadDelay);
  // Should allow data upload before there is user input.
  EXPECT_TRUE(uploader.IsSessionDataUploadAllowed());

  // Now mock user input, and no session data should be allowed.
  ui::MouseEvent e(ui::ET_MOUSE_PRESSED, gfx::Point(), gfx::Point(),
                   ui::EventTimeForNow(), 0, 0);
  const ui::PlatformEvent& native_event = &e;
  ui::UserActivityDetector::Get()->DidProcessEvent(native_event);
  EXPECT_FALSE(uploader.IsSessionDataUploadAllowed());
}

TEST_F(StatusUploaderTest, NoUploadAfterVideoCapture) {
  StatusUploader uploader(&client_, std::move(collector_), task_runner_,
                          kDefaultStatusUploadDelay);
  // Should allow data upload before there is video capture.
  EXPECT_TRUE(uploader.IsSessionDataUploadAllowed());

  // Now mock video capture, and no session data should be allowed.
  MediaCaptureDevicesDispatcher::GetInstance()->OnMediaRequestStateChanged(
      0, 0, 0, GURL("http://www.google.com"),
      content::MEDIA_DEVICE_VIDEO_CAPTURE,
      content::MEDIA_REQUEST_STATE_OPENING);
  base::RunLoop().RunUntilIdle();
  EXPECT_FALSE(uploader.IsSessionDataUploadAllowed());
}

}  // namespace policy
