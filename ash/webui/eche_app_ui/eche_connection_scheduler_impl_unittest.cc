// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/webui/eche_app_ui/eche_connection_scheduler_impl.h"

#include <memory>

#include "ash/webui/eche_app_ui/fake_feature_status_provider.h"
#include "ash/webui/eche_app_ui/feature_status.h"
#include "base/test/task_environment.h"
#include "base/time/time.h"
#include "chromeos/ash/services/secure_channel/public/cpp/client/fake_connection_manager.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace ash {
namespace eche_app {

using ConnectionStatus = secure_channel::ConnectionManager::Status;

class EcheConnectionSchedulerImplTest : public testing::Test {
 protected:
  EcheConnectionSchedulerImplTest() = default;
  EcheConnectionSchedulerImplTest(const EcheConnectionSchedulerImplTest&) =
      delete;
  EcheConnectionSchedulerImplTest& operator=(
      const EcheConnectionSchedulerImplTest&) = delete;
  ~EcheConnectionSchedulerImplTest() override = default;

  void SetUp() override {
    fake_connection_manager_ =
        std::make_unique<secure_channel::FakeConnectionManager>();
    fake_feature_status_provider_ =
        std::make_unique<FakeFeatureStatusProvider>();
  }

  void CreateConnectionScheduler() {
    connection_scheduler_ = std::make_unique<EcheConnectionSchedulerImpl>(
        fake_connection_manager_.get(), fake_feature_status_provider_.get());
  }

  base::TimeDelta GetCurrentBackoffDelay() {
    return connection_scheduler_->GetCurrentBackoffDelayTimeForTesting();
  }

  int GetBackoffFailureCount() {
    return connection_scheduler_->GetBackoffFailureCountForTesting();
  }

  base::test::TaskEnvironment task_environment_{
      base::test::TaskEnvironment::TimeSource::MOCK_TIME};
  std::unique_ptr<secure_channel::FakeConnectionManager>
      fake_connection_manager_;
  std::unique_ptr<FakeFeatureStatusProvider> fake_feature_status_provider_;
  std::unique_ptr<EcheConnectionSchedulerImpl> connection_scheduler_;
};

TEST_F(EcheConnectionSchedulerImplTest, SuccesssfullyAttemptConnection) {
  fake_connection_manager_->SetStatus(ConnectionStatus::kDisconnected);
  fake_feature_status_provider_->SetStatus(FeatureStatus::kDisconnected);
  CreateConnectionScheduler();

  connection_scheduler_->ScheduleConnectionNow();
  // Verify that the ConnectionManager has attempted to connect.
  EXPECT_EQ(1u, fake_connection_manager_->num_attempt_connection_calls());

  // Simulate state changes with AttemptConnection().
  fake_feature_status_provider_->SetStatus(FeatureStatus::kConnecting);
  fake_feature_status_provider_->SetStatus(FeatureStatus::kConnected);
  // Verify only 1 call to AttemptConnection() was ever made.
  EXPECT_EQ(fake_connection_manager_->num_attempt_connection_calls(), 1u);
  // Verify that we did not attempt a backoff retry.
  EXPECT_EQ(GetBackoffFailureCount(), 0);
}

TEST_F(EcheConnectionSchedulerImplTest,
       FeatureDisabledCanEstablishConnectionProperly) {
  fake_connection_manager_->SetStatus(ConnectionStatus::kConnecting);
  fake_feature_status_provider_->SetStatus(FeatureStatus::kDisabled);
  CreateConnectionScheduler();

  connection_scheduler_->ScheduleConnectionNow();
  // Verify that the ConnectionManager did not attempt connection.
  EXPECT_EQ(0u, fake_connection_manager_->num_attempt_connection_calls());
  // Verify that we did not attempt a backoff retry.
  EXPECT_EQ(0, GetBackoffFailureCount());

  fake_connection_manager_->SetStatus(ConnectionStatus::kConnected);
  CreateConnectionScheduler();
  // Verify that the ConnectionManager did not attempt connection.
  EXPECT_EQ(0u, fake_connection_manager_->num_attempt_connection_calls());
  // Verify that we did not attempt a backoff retry.
  EXPECT_EQ(0, GetBackoffFailureCount());

  fake_connection_manager_->SetStatus(ConnectionStatus::kDisconnected);
  connection_scheduler_->ScheduleConnectionNow();
  // Verify that the ConnectionManager has attempted to connect.
  EXPECT_EQ(fake_connection_manager_->num_attempt_connection_calls(), 1u);
  // Verify that we did not attempt a backoff retry.
  EXPECT_EQ(GetBackoffFailureCount(), 0);

  // Simulate state changes with AttemptConnection().
  fake_connection_manager_->SetStatus(ConnectionStatus::kConnecting);
  fake_feature_status_provider_->SetStatus(FeatureStatus::kConnected);
  // Verify only 1 call to AttemptConnection() was ever made.
  EXPECT_EQ(fake_connection_manager_->num_attempt_connection_calls(), 1u);
  // Verify that we did not attempt a backoff retry.
  EXPECT_EQ(GetBackoffFailureCount(), 0);
}

TEST_F(EcheConnectionSchedulerImplTest, ShouldNotEstablishConnection) {
  fake_connection_manager_->SetStatus(ConnectionStatus::kDisconnected);
  fake_feature_status_provider_->SetStatus(FeatureStatus::kIneligible);
  CreateConnectionScheduler();

  connection_scheduler_->ScheduleConnectionNow();
  EXPECT_EQ(fake_connection_manager_->num_attempt_connection_calls(), 0u);
  EXPECT_EQ(GetBackoffFailureCount(), 0);

  fake_feature_status_provider_->SetStatus(FeatureStatus::kDependentFeature);
  connection_scheduler_->ScheduleConnectionNow();
  EXPECT_EQ(fake_connection_manager_->num_attempt_connection_calls(), 0u);
  EXPECT_EQ(GetBackoffFailureCount(), 0);

  fake_feature_status_provider_->SetStatus(
      FeatureStatus::kDependentFeaturePending);
  connection_scheduler_->ScheduleConnectionNow();
  EXPECT_EQ(fake_connection_manager_->num_attempt_connection_calls(), 0u);
  EXPECT_EQ(GetBackoffFailureCount(), 0);
}

TEST_F(EcheConnectionSchedulerImplTest, BackoffRetryWithUpdatedConnection) {
  fake_connection_manager_->SetStatus(ConnectionStatus::kDisconnected);
  fake_feature_status_provider_->SetStatus(FeatureStatus::kDisconnected);
  CreateConnectionScheduler();

  connection_scheduler_->ScheduleConnectionNow();
  EXPECT_EQ(fake_connection_manager_->num_attempt_connection_calls(), 1u);
  // Simulate state changes with AttemptConnection().
  fake_connection_manager_->SetStatus(ConnectionStatus::kConnecting);
  fake_feature_status_provider_->SetStatus(FeatureStatus::kConnecting);
  fake_connection_manager_->SetStatus(ConnectionStatus::kDisconnected);
  fake_feature_status_provider_->SetStatus(FeatureStatus::kDisconnected);
  EXPECT_EQ(GetBackoffFailureCount(), 1);

  // Move forward time to the next backoff retry with disconnected status.
  task_environment_.FastForwardBy(GetCurrentBackoffDelay());
  EXPECT_EQ(fake_connection_manager_->num_attempt_connection_calls(), 2u);
  fake_connection_manager_->SetStatus(ConnectionStatus::kConnecting);
  fake_feature_status_provider_->SetStatus(FeatureStatus::kConnecting);
  fake_connection_manager_->SetStatus(ConnectionStatus::kDisconnected);
  fake_feature_status_provider_->SetStatus(FeatureStatus::kDisconnected);
  EXPECT_EQ(GetBackoffFailureCount(), 2);

  // Move forward time to the next backoff retry, this time with connected
  // status.
  task_environment_.FastForwardBy(GetCurrentBackoffDelay());
  EXPECT_EQ(fake_connection_manager_->num_attempt_connection_calls(), 3u);
  fake_connection_manager_->SetStatus(ConnectionStatus::kConnecting);
  fake_feature_status_provider_->SetStatus(FeatureStatus::kConnecting);
  fake_connection_manager_->SetStatus(ConnectionStatus::kConnected);
  fake_feature_status_provider_->SetStatus(FeatureStatus::kConnected);
  // Expected no more backoff failures since connection is now established.
  EXPECT_EQ(GetBackoffFailureCount(), 0);

  // Fast forward time and confirm no other retries have been made.
  task_environment_.FastForwardBy(base::Seconds(100));
  EXPECT_EQ(fake_connection_manager_->num_attempt_connection_calls(), 3u);
  EXPECT_EQ(GetBackoffFailureCount(), 0);
  EXPECT_EQ(fake_feature_status_provider_->GetStatus(),
            FeatureStatus::kConnected);
}

TEST_F(EcheConnectionSchedulerImplTest,
       BackoffRetryWithUpdatedFeaturesAndConnection) {
  fake_connection_manager_->SetStatus(ConnectionStatus::kDisconnected);
  fake_feature_status_provider_->SetStatus(FeatureStatus::kDisconnected);
  CreateConnectionScheduler();

  connection_scheduler_->ScheduleConnectionNow();
  EXPECT_EQ(fake_connection_manager_->num_attempt_connection_calls(), 1u);
  // Simulate state changes with AttemptConnection().
  fake_connection_manager_->SetStatus(ConnectionStatus::kConnecting);
  fake_feature_status_provider_->SetStatus(FeatureStatus::kConnecting);
  fake_connection_manager_->SetStatus(ConnectionStatus::kDisconnected);
  fake_feature_status_provider_->SetStatus(FeatureStatus::kDisconnected);
  EXPECT_EQ(1, GetBackoffFailureCount());

  // Simulate the feature status switched to kIneligible.
  fake_feature_status_provider_->SetStatus(FeatureStatus::kIneligible);
  // Expect the backoff to reset and never attempt to kickoff another
  // connection.
  EXPECT_EQ(GetBackoffFailureCount(), 0);
  EXPECT_EQ(fake_connection_manager_->num_attempt_connection_calls(), 1u);
  // Expect that connection has been disconnected.
  EXPECT_EQ(fake_connection_manager_->num_disconnect_calls(), 1u);

  // Fast forward time and confirm no other retries have been made.
  task_environment_.FastForwardBy(base::Seconds(100));
  EXPECT_EQ(GetBackoffFailureCount(), 0);
  EXPECT_EQ(fake_connection_manager_->num_attempt_connection_calls(), 1u);

  // Simulate the feature re-enabled and the connection kickoff should start.
  fake_feature_status_provider_->SetStatus(FeatureStatus::kDisconnected);
  // The next ScheduleConnection() was not caused by a previous failure, expect
  // backoff failure count to not increase.
  EXPECT_EQ(GetBackoffFailureCount(), 0);

  // Move forward in time and confirm backoff attempted another retry.
  task_environment_.FastForwardBy(GetCurrentBackoffDelay());
  EXPECT_EQ(fake_connection_manager_->num_attempt_connection_calls(), 2u);
  fake_connection_manager_->SetStatus(ConnectionStatus::kConnecting);
  fake_feature_status_provider_->SetStatus(FeatureStatus::kConnecting);
  fake_connection_manager_->SetStatus(ConnectionStatus::kDisconnected);
  fake_feature_status_provider_->SetStatus(FeatureStatus::kDisconnected);
  // The next ScheduleConnection() was caused by a previous failure, expect 1
  // failure count.
  EXPECT_EQ(GetBackoffFailureCount(), 1);
}

TEST_F(EcheConnectionSchedulerImplTest,
       DisconnectAndClearBackoffAttemptsSuccesssfully) {
  fake_connection_manager_->SetStatus(ConnectionStatus::kDisconnected);
  fake_feature_status_provider_->SetStatus(FeatureStatus::kDisconnected);
  CreateConnectionScheduler();

  connection_scheduler_->ScheduleConnectionNow();
  // Verify that the ConnectionManager has attempted to connect.
  EXPECT_EQ(fake_connection_manager_->num_attempt_connection_calls(), 1u);

  // Simulate state changes with AttemptConnection().
  fake_connection_manager_->SetStatus(ConnectionStatus::kConnecting);
  fake_feature_status_provider_->SetStatus(FeatureStatus::kConnecting);
  fake_connection_manager_->SetStatus(ConnectionStatus::kDisconnected);
  fake_feature_status_provider_->SetStatus(FeatureStatus::kDisconnected);
  EXPECT_EQ(1, GetBackoffFailureCount());

  connection_scheduler_->DisconnectAndClearBackoffAttempts();

  // Expect the backoff to reset and never attempt to kickoff another
  // connection.
  EXPECT_EQ(GetBackoffFailureCount(), 0);
  EXPECT_EQ(fake_connection_manager_->num_attempt_connection_calls(), 1u);
  // Expect that connection has been disconnected.
  EXPECT_EQ(fake_connection_manager_->num_disconnect_calls(), 1u);
}

TEST_F(EcheConnectionSchedulerImplTest, SimulateIneligibleToDisconnected) {
  fake_connection_manager_->SetStatus(ConnectionStatus::kDisconnected);
  fake_feature_status_provider_->SetStatus(FeatureStatus::kIneligible);
  CreateConnectionScheduler();

  EXPECT_EQ(GetBackoffFailureCount(), 0);
  EXPECT_EQ(fake_connection_manager_->num_attempt_connection_calls(), 0u);

  // Flip to feature available. Expect a scheduled connection.
  fake_feature_status_provider_->SetStatus(FeatureStatus::kDisconnected);
  EXPECT_EQ(GetBackoffFailureCount(), 0);
  EXPECT_EQ(fake_connection_manager_->num_attempt_connection_calls(), 1u);
}

TEST_F(EcheConnectionSchedulerImplTest,
       SimulateDependentFeatureToDisconnected) {
  fake_connection_manager_->SetStatus(ConnectionStatus::kDisconnected);
  fake_feature_status_provider_->SetStatus(FeatureStatus::kDependentFeature);
  CreateConnectionScheduler();

  EXPECT_EQ(GetBackoffFailureCount(), 0);
  EXPECT_EQ(fake_connection_manager_->num_attempt_connection_calls(), 0u);

  // Flip to feature available. Expect a scheduled connection.
  fake_feature_status_provider_->SetStatus(FeatureStatus::kDisconnected);
  EXPECT_EQ(GetBackoffFailureCount(), 0);
  EXPECT_EQ(fake_connection_manager_->num_attempt_connection_calls(), 1u);
}

TEST_F(EcheConnectionSchedulerImplTest,
       SimulatekDependentFeaturePendingtoDisconnected) {
  fake_connection_manager_->SetStatus(ConnectionStatus::kDisconnected);
  fake_feature_status_provider_->SetStatus(
      FeatureStatus::kDependentFeaturePending);
  CreateConnectionScheduler();

  EXPECT_EQ(GetBackoffFailureCount(), 0);
  EXPECT_EQ(fake_connection_manager_->num_attempt_connection_calls(), 0u);

  // Flip to feature available. Expect a scheduled connection.
  fake_feature_status_provider_->SetStatus(FeatureStatus::kDisconnected);
  EXPECT_EQ(GetBackoffFailureCount(), 0);
  EXPECT_EQ(fake_connection_manager_->num_attempt_connection_calls(), 1u);
}

}  // namespace eche_app
}  // namespace ash
