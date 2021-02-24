// Copyright (c) 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/policy/heartbeat_scheduler.h"

#include <stdint.h>

#include <vector>

#include "base/macros.h"
#include "base/strings/string_number_conversions.h"
#include "base/test/gmock_move_support.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/test_simple_task_runner.h"
#include "chrome/browser/chromeos/settings/scoped_testing_cros_settings.h"
#include "chrome/browser/chromeos/settings/stub_cros_settings_provider.h"
#include "chromeos/settings/cros_settings_names.h"
#include "components/gcm_driver/common/gcm_message.h"
#include "components/gcm_driver/fake_gcm_driver.h"
#include "components/policy/core/common/cloud/cloud_policy_client.h"
#include "components/policy/core/common/cloud/mock_cloud_policy_client.h"
#include "components/policy/core/common/cloud/mock_cloud_policy_store.h"
#include "content/public/test/browser_task_environment.h"
#include "content/public/test/test_utils.h"
#include "net/base/ip_endpoint.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using ::testing::_;
using ::testing::AnyNumber;
using ::testing::AtLeast;
using ::testing::Contains;
using ::testing::Field;
using ::testing::Key;
using ::testing::Matches;
using ::testing::Pair;
using ::testing::SaveArg;

namespace {
const char* const kFakeCustomerId = "fake_customer_id";
const char* const kFakeDeviceId = "fake_device_id";
const char* const kHeartbeatGCMAppID = "com.google.chromeos.monitoring";
const char* const kRegistrationId = "registration_id";
const char* const kDMToken = "fake_dm_token";

MATCHER(IsHeartbeatMsg, "") {
  return Matches(
      Field(&gcm::OutgoingMessage::data, Contains(Pair("type", "hb"))))(arg);
}

MATCHER(IsUpstreamNotificationMsg, "") {
  return Matches(Field(&gcm::OutgoingMessage::data, Contains(Key("notify"))))(
      arg);
}

class MockGCMDriver : public testing::StrictMock<gcm::FakeGCMDriver> {
 public:
  MockGCMDriver() { IgnoreDefaultHeartbeatsInterval(); }

  ~MockGCMDriver() override {
  }

  MOCK_METHOD2(RegisterImpl,
               void(const std::string&, const std::vector<std::string>&));
  MOCK_METHOD3(SendImpl,
               void(const std::string&,
                    const std::string&,
                    const gcm::OutgoingMessage& message));

  MOCK_METHOD2(AddHeartbeatInterval,
               void(const std::string& scope, int interval_ms));

  // Helper function to complete a registration previously started by
  // Register().
  void CompleteRegistration(const std::string& app_id,
                            gcm::GCMClient::Result result) {
    RegisterFinished(app_id, kRegistrationId, result);
  }

  // Helper function to complete a send operation previously started by
  // Send().
  void CompleteSend(const std::string& app_id,
                    const std::string& message_id,
                    gcm::GCMClient::Result result) {
    SendFinished(app_id, message_id, result);
  }

  void AddConnectionObserver(gcm::GCMConnectionObserver* observer) override {
    EXPECT_FALSE(observer_);
    observer_ = observer;
  }

  void RemoveConnectionObserver(gcm::GCMConnectionObserver* observer) override {
    EXPECT_TRUE(observer_);
    observer_ = nullptr;
  }

  void NotifyConnected() {
    ASSERT_TRUE(observer_);
    observer_->OnConnected(net::IPEndPoint());
  }

  void IgnoreDefaultHeartbeatsInterval() {
    // Ignore default setting for heartbeats interval.
    EXPECT_CALL(*this, AddHeartbeatInterval(_, 2 * 60 * 1000))
        .Times(AnyNumber());
  }

  void ResetStore() {
    // Defensive copy in case OnStoreReset calls Add/RemoveAppHandler.
    std::vector<gcm::GCMAppHandler*> app_handler_values;
    for (const auto& key_value : app_handlers())
      app_handler_values.push_back(key_value.second);
    for (gcm::GCMAppHandler* app_handler : app_handler_values) {
      app_handler->OnStoreReset();
      // app_handler might now have been deleted.
    }
  }

 private:
  gcm::GCMConnectionObserver* observer_ = nullptr;

  DISALLOW_COPY_AND_ASSIGN(MockGCMDriver);
};

class HeartbeatSchedulerTest : public testing::Test {
 public:
  HeartbeatSchedulerTest()
      : task_runner_(new base::TestSimpleTaskRunner()),
        scheduler_(&gcm_driver_,
                   &cloud_policy_client_,
                   &cloud_policy_store_,
                   kFakeDeviceId,
                   task_runner_) {}

  void SetUp() {
    cloud_policy_store_.InitPolicyData();
    cloud_policy_store_.GetMutablePolicyData()->set_obfuscated_customer_id(
        kFakeCustomerId);
  }

  void TearDown() override { content::RunAllTasksUntilIdle(); }

  void CheckPendingTaskDelay(base::Time last_heartbeat,
                             base::TimeDelta expected_delay) {
    EXPECT_FALSE(last_heartbeat.is_null());
    base::Time now = base::Time::NowFromSystemTime();
    EXPECT_GE(now, last_heartbeat);
    base::TimeDelta actual_delay = task_runner_->NextPendingTaskDelay();

    // NextPendingTaskDelay() returns the exact original delay value the task
    // was posted with. The heartbeat task would have been calculated to fire at
    // |last_heartbeat| + |expected_delay|, but we don't know the exact time
    // when the task was posted (if it was a couple of milliseconds after
    // |last_heartbeat|, then |actual_delay| would be a couple of milliseconds
    // smaller than |expected_delay|.
    //
    // We do know that the task was posted sometime between |last_heartbeat|
    // and |now|, so we know that 0 <= |expected_delay| - |actual_delay| <=
    // |now| - |last_heartbeat|.
    base::TimeDelta delta = expected_delay - actual_delay;
    EXPECT_LE(base::TimeDelta(), delta);
    EXPECT_GE(now - last_heartbeat, delta);
  }

  void IgnoreUpstreamNotificationMsg() {
    EXPECT_CALL(gcm_driver_,
                SendImpl(kHeartbeatGCMAppID, _, IsUpstreamNotificationMsg()))
        .Times(AnyNumber());
  }

  content::BrowserTaskEnvironment task_environment_;
  MockGCMDriver gcm_driver_;
  chromeos::ScopedTestingCrosSettings scoped_testing_cros_settings_;
  testing::NiceMock<policy::MockCloudPolicyClient> cloud_policy_client_;
  testing::NiceMock<policy::MockCloudPolicyStore> cloud_policy_store_;

  // TaskRunner used to run individual tests.
  scoped_refptr<base::TestSimpleTaskRunner> task_runner_;

  // The HeartbeatScheduler instance under test.
  policy::HeartbeatScheduler scheduler_;

  base::HistogramTester histogram_tester_;
};

TEST_F(HeartbeatSchedulerTest, Basic) {
  // Just makes sure we can spin up and shutdown the scheduler with
  // heartbeats disabled.
  scoped_testing_cros_settings_.device_settings()->SetBoolean(
      chromeos::kHeartbeatEnabled, false);
  ASSERT_FALSE(task_runner_->HasPendingTask());
}

TEST_F(HeartbeatSchedulerTest, PermanentlyFailedGCMRegistration) {
  // If heartbeats are enabled, we should register with GCMDriver.
  EXPECT_CALL(gcm_driver_, RegisterImpl(kHeartbeatGCMAppID, _));
  scoped_testing_cros_settings_.device_settings()->SetBoolean(
      chromeos::kHeartbeatEnabled, true);
  gcm_driver_.CompleteRegistration(
      kHeartbeatGCMAppID, gcm::GCMClient::GCM_DISABLED);

  // There should be no heartbeat tasks pending, because registration failed.
  ASSERT_FALSE(task_runner_->HasPendingTask());
}

TEST_F(HeartbeatSchedulerTest, TemporarilyFailedGCMRegistration) {
  IgnoreUpstreamNotificationMsg();

  EXPECT_CALL(gcm_driver_, RegisterImpl(kHeartbeatGCMAppID, _));
  scoped_testing_cros_settings_.device_settings()->SetBoolean(
      chromeos::kHeartbeatEnabled, true);
  gcm_driver_.CompleteRegistration(
      kHeartbeatGCMAppID, gcm::GCMClient::SERVER_ERROR);
  testing::Mock::VerifyAndClearExpectations(&gcm_driver_);

  IgnoreUpstreamNotificationMsg();
  gcm_driver_.IgnoreDefaultHeartbeatsInterval();

  // Should have a pending task to try registering again.
  ASSERT_TRUE(task_runner_->HasPendingTask());
  EXPECT_CALL(gcm_driver_, RegisterImpl(kHeartbeatGCMAppID, _));
  task_runner_->RunPendingTasks();
  testing::Mock::VerifyAndClearExpectations(&gcm_driver_);

  IgnoreUpstreamNotificationMsg();
  gcm_driver_.IgnoreDefaultHeartbeatsInterval();

  // Once we have successfully registered, we should send a heartbeat.
  EXPECT_CALL(gcm_driver_, SendImpl(kHeartbeatGCMAppID, _, IsHeartbeatMsg()));
  gcm_driver_.CompleteRegistration(
      kHeartbeatGCMAppID, gcm::GCMClient::SUCCESS);
  task_runner_->RunPendingTasks();
}

TEST_F(HeartbeatSchedulerTest, StoreResetDuringRegistration) {
  IgnoreUpstreamNotificationMsg();

  EXPECT_CALL(gcm_driver_, RegisterImpl(kHeartbeatGCMAppID, _));
  scoped_testing_cros_settings_.device_settings()->SetBoolean(
      chromeos::kHeartbeatEnabled, true);
  testing::Mock::VerifyAndClearExpectations(&gcm_driver_);

  gcm_driver_.ResetStore();

  IgnoreUpstreamNotificationMsg();

  // Successful registration handled ok despite store reset.
  EXPECT_FALSE(task_runner_->HasPendingTask());
  EXPECT_CALL(gcm_driver_, SendImpl(kHeartbeatGCMAppID, _, IsHeartbeatMsg()));
  gcm_driver_.CompleteRegistration(
      kHeartbeatGCMAppID, gcm::GCMClient::SUCCESS);
  EXPECT_EQ(1U, task_runner_->NumPendingTasks());
  task_runner_->RunPendingTasks();
  testing::Mock::VerifyAndClearExpectations(&gcm_driver_);
  histogram_tester_.ExpectTotalCount(
      policy::HeartbeatScheduler::kHeartbeatSignalHistogram, 0);
}

TEST_F(HeartbeatSchedulerTest, StoreResetAfterRegistration) {
  IgnoreUpstreamNotificationMsg();

  // Start from a successful registration.
  EXPECT_CALL(gcm_driver_, RegisterImpl(kHeartbeatGCMAppID, _));
  scoped_testing_cros_settings_.device_settings()->SetBoolean(
      chromeos::kHeartbeatEnabled, true);
  EXPECT_CALL(gcm_driver_, SendImpl(kHeartbeatGCMAppID, _, IsHeartbeatMsg()));
  gcm_driver_.CompleteRegistration(
      kHeartbeatGCMAppID, gcm::GCMClient::SUCCESS);
  EXPECT_EQ(1U, task_runner_->NumPendingTasks());
  task_runner_->RunPendingTasks();
  testing::Mock::VerifyAndClearExpectations(&gcm_driver_);

  gcm_driver_.IgnoreDefaultHeartbeatsInterval();

  // Reseting the store should trigger re-registration.
  EXPECT_CALL(gcm_driver_, RegisterImpl(kHeartbeatGCMAppID, _));
  gcm_driver_.ResetStore();
  testing::Mock::VerifyAndClearExpectations(&gcm_driver_);

  IgnoreUpstreamNotificationMsg();

  // Once we have successfully re-registered, we should send a heartbeat.
  EXPECT_FALSE(task_runner_->HasPendingTask());
  EXPECT_CALL(gcm_driver_, SendImpl(kHeartbeatGCMAppID, _, IsHeartbeatMsg()));
  gcm_driver_.CompleteRegistration(
      kHeartbeatGCMAppID, gcm::GCMClient::SUCCESS);
  EXPECT_EQ(1U, task_runner_->NumPendingTasks());
  task_runner_->RunPendingTasks();
  testing::Mock::VerifyAndClearExpectations(&gcm_driver_);
}

TEST_F(HeartbeatSchedulerTest, ChangeHeartbeatFrequency) {
  IgnoreUpstreamNotificationMsg();

  EXPECT_CALL(gcm_driver_, RegisterImpl(kHeartbeatGCMAppID, _));
  scoped_testing_cros_settings_.device_settings()->SetBoolean(
      chromeos::kHeartbeatEnabled, true);
  gcm_driver_.CompleteRegistration(
      kHeartbeatGCMAppID, gcm::GCMClient::SUCCESS);

  EXPECT_EQ(1U, task_runner_->NumPendingTasks());
  // Should have a heartbeat task posted with zero delay on startup.
  EXPECT_EQ(base::TimeDelta(), task_runner_->NextPendingTaskDelay());
  testing::Mock::VerifyAndClearExpectations(&gcm_driver_);

  IgnoreUpstreamNotificationMsg();
  gcm_driver_.IgnoreDefaultHeartbeatsInterval();

  const int new_delay = 1234*1000;  // 1234 seconds.
  EXPECT_CALL(gcm_driver_, AddHeartbeatInterval(_, new_delay));
  scoped_testing_cros_settings_.device_settings()->SetInteger(
      chromeos::kHeartbeatFrequency, new_delay);
  // Now run pending heartbeat task, should send a heartbeat.
  gcm::OutgoingMessage message;
  EXPECT_CALL(gcm_driver_, SendImpl(kHeartbeatGCMAppID, _, IsHeartbeatMsg()))
      .WillOnce(SaveArg<2>(&message));
  task_runner_->RunPendingTasks();
  EXPECT_FALSE(task_runner_->HasPendingTask());

  // Complete sending a message - we should queue up the next heartbeat
  // even if the previous attempt failed.
  gcm_driver_.CompleteSend(
      kHeartbeatGCMAppID, message.id, gcm::GCMClient::SERVER_ERROR);
  EXPECT_EQ(1U, task_runner_->NumPendingTasks());
  histogram_tester_.ExpectUniqueSample(
      policy::HeartbeatScheduler::kHeartbeatSignalHistogram, /*failure*/ false,
      /*amount*/ 1);
  CheckPendingTaskDelay(scheduler_.last_heartbeat(),
                        base::TimeDelta::FromMilliseconds(new_delay));
}

TEST_F(HeartbeatSchedulerTest, DisableHeartbeats) {
  IgnoreUpstreamNotificationMsg();

  // Makes sure that we can disable heartbeats on the fly.
  EXPECT_CALL(gcm_driver_, RegisterImpl(kHeartbeatGCMAppID, _));
  scoped_testing_cros_settings_.device_settings()->SetBoolean(
      chromeos::kHeartbeatEnabled, true);
  gcm::OutgoingMessage message;
  EXPECT_CALL(gcm_driver_, SendImpl(kHeartbeatGCMAppID, _, IsHeartbeatMsg()))
      .WillOnce(SaveArg<2>(&message));
  gcm_driver_.CompleteRegistration(
      kHeartbeatGCMAppID, gcm::GCMClient::SUCCESS);
  // Should have a heartbeat task posted.
  EXPECT_EQ(1U, task_runner_->NumPendingTasks());
  task_runner_->RunPendingTasks();

  // Complete sending a message - we should queue up the next heartbeat.
  gcm_driver_.CompleteSend(
      kHeartbeatGCMAppID, message.id, gcm::GCMClient::SUCCESS);
  histogram_tester_.ExpectUniqueSample(
      policy::HeartbeatScheduler::kHeartbeatSignalHistogram, /*success*/ true,
      /*amount*/ 1);

  // Should have a new heartbeat task posted.
  ASSERT_EQ(1U, task_runner_->NumPendingTasks());
  CheckPendingTaskDelay(scheduler_.last_heartbeat(),
                        policy::HeartbeatScheduler::kDefaultHeartbeatInterval);
  testing::Mock::VerifyAndClearExpectations(&gcm_driver_);

  IgnoreUpstreamNotificationMsg();
  gcm_driver_.IgnoreDefaultHeartbeatsInterval();

  // Now disable heartbeats. Should get no more heartbeats sent.
  scoped_testing_cros_settings_.device_settings()->SetBoolean(
      chromeos::kHeartbeatEnabled, false);
  task_runner_->RunPendingTasks();
  EXPECT_FALSE(task_runner_->HasPendingTask());
}

TEST_F(HeartbeatSchedulerTest, CheckMessageContents) {
  IgnoreUpstreamNotificationMsg();

  gcm::OutgoingMessage message;
  EXPECT_CALL(gcm_driver_, RegisterImpl(kHeartbeatGCMAppID, _));
  EXPECT_CALL(gcm_driver_, SendImpl(kHeartbeatGCMAppID, _, IsHeartbeatMsg()))
      .WillOnce(SaveArg<2>(&message));
  scoped_testing_cros_settings_.device_settings()->SetBoolean(
      chromeos::kHeartbeatEnabled, true);
  gcm_driver_.CompleteRegistration(
      kHeartbeatGCMAppID, gcm::GCMClient::SUCCESS);
  task_runner_->RunPendingTasks();

  // Heartbeats should have a time-to-live equivalent to the heartbeat frequency
  // so we don't have more than one heartbeat queued at a time.
  EXPECT_EQ(policy::HeartbeatScheduler::kDefaultHeartbeatInterval,
            base::TimeDelta::FromSeconds(message.time_to_live));

  // Check the values in the message payload.
  EXPECT_EQ("hb", message.data["type"]);
  int64_t timestamp;
  EXPECT_TRUE(base::StringToInt64(message.data["timestamp"], &timestamp));
  EXPECT_EQ(kFakeCustomerId, message.data["customer_id"]);
  EXPECT_EQ(kFakeDeviceId, message.data["device_id"]);
}

TEST_F(HeartbeatSchedulerTest, SendGcmIdUpdate) {
  // Verifies that GCM id update request was sent after GCM registration.
  cloud_policy_client_.SetDMToken(kDMToken);
  policy::CloudPolicyClient::StatusCallback callback;
  EXPECT_CALL(cloud_policy_client_, UpdateGcmId_(kRegistrationId, _))
      .WillOnce(MoveArg<1>(&callback));

  // Enable heartbeats.
  EXPECT_CALL(gcm_driver_, RegisterImpl(kHeartbeatGCMAppID, _));
  EXPECT_CALL(gcm_driver_, SendImpl(kHeartbeatGCMAppID, _, _))
      .Times(AtLeast(1));
  scoped_testing_cros_settings_.device_settings()->SetBoolean(
      chromeos::kHeartbeatEnabled, true);
  gcm_driver_.CompleteRegistration(kHeartbeatGCMAppID, gcm::GCMClient::SUCCESS);
  task_runner_->RunPendingTasks();

  // Verifies that CloudPolicyClient got the update request, with a valid
  // callback.
  testing::Mock::VerifyAndClearExpectations(&cloud_policy_client_);
  EXPECT_FALSE(callback.is_null());
  std::move(callback).Run(true);
}

TEST_F(HeartbeatSchedulerTest, GcmUpstreamNotificationSignup) {
  // Verifies that upstream notification works as expected.
  cloud_policy_client_.SetDMToken(kDMToken);
  EXPECT_CALL(gcm_driver_, RegisterImpl(kHeartbeatGCMAppID, _))
      .Times(AnyNumber());
  EXPECT_CALL(cloud_policy_client_, UpdateGcmId_(kRegistrationId, _));

  // GCM connected event before the registration should be ignored.
  scoped_testing_cros_settings_.device_settings()->SetBoolean(
      chromeos::kHeartbeatEnabled, true);
  gcm_driver_.NotifyConnected();
  task_runner_->RunPendingTasks();
  testing::Mock::VerifyAndClearExpectations(&gcm_driver_);

  // Ignore unintested calls.
  EXPECT_CALL(gcm_driver_, RegisterImpl(kHeartbeatGCMAppID, _))
      .Times(AnyNumber());
  EXPECT_CALL(gcm_driver_, SendImpl(kHeartbeatGCMAppID, _, IsHeartbeatMsg()))
      .Times(AnyNumber());
  gcm_driver_.IgnoreDefaultHeartbeatsInterval();

  EXPECT_CALL(gcm_driver_,
              SendImpl(kHeartbeatGCMAppID, _, IsUpstreamNotificationMsg()))
      .Times(AtLeast(1));
  gcm_driver_.CompleteRegistration(kHeartbeatGCMAppID, gcm::GCMClient::SUCCESS);

  gcm_driver_.NotifyConnected();
}

}  // namespace
