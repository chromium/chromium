// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/webui/eche_app_ui/eche_connection_status_handler.h"

#include "ash/constants/ash_features.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/task_environment.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace ash::eche_app {

namespace {

class FakeObserver : public EcheConnectionStatusHandler::Observer {
 public:
  FakeObserver() = default;
  ~FakeObserver() override = default;

  size_t num_connection_status_changed_calls() const {
    return num_connection_status_changed_calls_;
  }

  size_t num_connection_status_for_ui_changed_calls() const {
    return num_connection_status_for_ui_changed_calls_;
  }

  size_t num_request_background_connection_attempt_calls() const {
    return num_request_background_connection_attempt_calls_;
  }

  size_t num_request_close_connection_calls() const {
    return num_request_close_connection_calls_;
  }

  mojom::ConnectionStatus last_connection_changed_status() const {
    return last_connection_changed_status_;
  }

  mojom::ConnectionStatus last_connection_for_ui_changed_status() const {
    return last_connection_for_ui_changed_status_;
  }

  // EcheConnectionStatusHandler::Observer:
  void OnConnectionStatusChanged(
      mojom::ConnectionStatus connection_status) override {
    ++num_connection_status_changed_calls_;
    last_connection_changed_status_ = connection_status;
  }

  void OnConnectionStatusForUiChanged(
      mojom::ConnectionStatus connection_status) override {
    ++num_connection_status_for_ui_changed_calls_;
    last_connection_for_ui_changed_status_ = connection_status;
  }

  void OnRequestBackgroundConnectionAttempt() override {
    ++num_request_background_connection_attempt_calls_;
  }

  void OnRequestCloseConnection() override {
    ++num_request_close_connection_calls_;
  }

 private:
  size_t num_connection_status_changed_calls_ = 0;
  size_t num_connection_status_for_ui_changed_calls_ = 0;
  size_t num_request_background_connection_attempt_calls_ = 0;
  size_t num_request_close_connection_calls_ = 0;
  mojom::ConnectionStatus last_connection_changed_status_ =
      mojom::ConnectionStatus::kConnectionStatusDisconnected;
  mojom::ConnectionStatus last_connection_for_ui_changed_status_ =
      mojom::ConnectionStatus::kConnectionStatusDisconnected;
};

}  // namespace

class EcheConnectionStatusHandlerTest : public testing::Test {
 public:
  EcheConnectionStatusHandlerTest() = default;
  EcheConnectionStatusHandlerTest(const EcheConnectionStatusHandlerTest&) =
      delete;
  EcheConnectionStatusHandlerTest& operator=(
      const EcheConnectionStatusHandlerTest&) = delete;
  ~EcheConnectionStatusHandlerTest() override = default;

  // testing::Test:
  void SetUp() override {
    scoped_feature_list_.InitWithFeatures(
        /*enabled_features=*/{features::kEcheSWA,
                              features::kEcheNetworkConnectionState},
        /*disabled_features=*/{});

    handler_ = std::make_unique<EcheConnectionStatusHandler>();
    handler_->AddObserver(&fake_observer_);
  }

  void TearDown() override {
    handler_->RemoveObserver(&fake_observer_);
    handler_.reset();
  }

  EcheConnectionStatusHandler& handler() { return *handler_; }

  void NotifyConnectionStatusChanged(
      mojom::ConnectionStatus connection_status) {
    handler_->OnConnectionStatusChanged(connection_status);
  }

  void SetFeatureStatus(FeatureStatus feature_status) {
    handler_->set_feature_status_for_test(feature_status);
  }

  bool GetIsConnectingOrConnectedStatus() const {
    return handler_->is_connecting_or_connected_for_test();
  }

  size_t GetNumConnectionStatusChangedCalls() const {
    return fake_observer_.num_connection_status_changed_calls();
  }

  size_t GetNumConnectionStatusForUiChangedCalls() const {
    return fake_observer_.num_connection_status_for_ui_changed_calls();
  }

  size_t GetNumRequestBackgroundConnectionAttemptCalls() const {
    return fake_observer_.num_request_background_connection_attempt_calls();
  }

  size_t GetNumRequestCloceConnectionCalls() const {
    return fake_observer_.num_request_close_connection_calls();
  }

  mojom::ConnectionStatus GetLastConnectionChangedStatus() const {
    return fake_observer_.last_connection_changed_status();
  }

  mojom::ConnectionStatus GetLastConnectionForUiChangedStatus() const {
    return fake_observer_.last_connection_for_ui_changed_status();
  }

  mojom::ConnectionStatus GetConnectionStatusForUi() const {
    return handler_->connection_status_for_ui();
  }

  base::test::ScopedFeatureList scoped_feature_list_;

  base::test::TaskEnvironment task_environment_{
      base::test::TaskEnvironment::TimeSource::MOCK_TIME};

 private:
  FakeObserver fake_observer_;
  std::unique_ptr<EcheConnectionStatusHandler> handler_;
};

TEST_F(EcheConnectionStatusHandlerTest, OnConnectionStatusChanged) {
  EXPECT_EQ(GetLastConnectionChangedStatus(),
            mojom::ConnectionStatus::kConnectionStatusDisconnected);
  EXPECT_EQ(GetNumConnectionStatusChangedCalls(), 0u);
  EXPECT_FALSE(GetIsConnectingOrConnectedStatus());

  NotifyConnectionStatusChanged(
      mojom::ConnectionStatus::kConnectionStatusConnecting);

  EXPECT_EQ(GetLastConnectionChangedStatus(),
            mojom::ConnectionStatus::kConnectionStatusConnecting);
  EXPECT_EQ(GetNumConnectionStatusChangedCalls(), 1u);
  EXPECT_TRUE(GetIsConnectingOrConnectedStatus());

  NotifyConnectionStatusChanged(
      mojom::ConnectionStatus::kConnectionStatusConnected);

  EXPECT_EQ(GetLastConnectionChangedStatus(),
            mojom::ConnectionStatus::kConnectionStatusConnected);
  EXPECT_EQ(GetNumConnectionStatusChangedCalls(), 2u);
  EXPECT_TRUE(GetIsConnectingOrConnectedStatus());

  NotifyConnectionStatusChanged(
      mojom::ConnectionStatus::kConnectionStatusFailed);

  EXPECT_EQ(GetLastConnectionChangedStatus(),
            mojom::ConnectionStatus::kConnectionStatusFailed);
  EXPECT_EQ(GetNumConnectionStatusChangedCalls(), 3u);
  EXPECT_FALSE(GetIsConnectingOrConnectedStatus());

  NotifyConnectionStatusChanged(
      mojom::ConnectionStatus::kConnectionStatusDisconnected);

  EXPECT_EQ(GetLastConnectionChangedStatus(),
            mojom::ConnectionStatus::kConnectionStatusDisconnected);
  EXPECT_EQ(GetNumConnectionStatusChangedCalls(), 4u);
  EXPECT_FALSE(GetIsConnectingOrConnectedStatus());
}

TEST_F(EcheConnectionStatusHandlerTest, OnConnectionStatusChangedFlagDisabled) {
  scoped_feature_list_.Reset();
  scoped_feature_list_.InitWithFeatures(
      /*enabled_features=*/{features::kEcheSWA},
      /*disabled_features=*/{features::kEcheNetworkConnectionState});

  EXPECT_EQ(GetLastConnectionChangedStatus(),
            mojom::ConnectionStatus::kConnectionStatusDisconnected);
  EXPECT_EQ(GetNumConnectionStatusChangedCalls(), 0u);
  EXPECT_FALSE(GetIsConnectingOrConnectedStatus());

  NotifyConnectionStatusChanged(
      mojom::ConnectionStatus::kConnectionStatusConnecting);

  EXPECT_EQ(GetLastConnectionChangedStatus(),
            mojom::ConnectionStatus::kConnectionStatusDisconnected);
  EXPECT_EQ(GetNumConnectionStatusChangedCalls(), 0u);
  EXPECT_FALSE(GetIsConnectingOrConnectedStatus());

  NotifyConnectionStatusChanged(
      mojom::ConnectionStatus::kConnectionStatusConnected);

  EXPECT_EQ(GetLastConnectionChangedStatus(),
            mojom::ConnectionStatus::kConnectionStatusDisconnected);
  EXPECT_EQ(GetNumConnectionStatusChangedCalls(), 0u);
  EXPECT_FALSE(GetIsConnectingOrConnectedStatus());

  NotifyConnectionStatusChanged(
      mojom::ConnectionStatus::kConnectionStatusFailed);

  EXPECT_EQ(GetLastConnectionChangedStatus(),
            mojom::ConnectionStatus::kConnectionStatusDisconnected);
  EXPECT_EQ(GetNumConnectionStatusChangedCalls(), 0u);
  EXPECT_FALSE(GetIsConnectingOrConnectedStatus());
}

TEST_F(EcheConnectionStatusHandlerTest, CheckConnectionStatusForUi) {
  SetFeatureStatus(FeatureStatus::kDisconnected);

  EXPECT_EQ(GetLastConnectionForUiChangedStatus(),
            mojom::ConnectionStatus::kConnectionStatusDisconnected);
  EXPECT_EQ(GetNumConnectionStatusForUiChangedCalls(), 0u);
  EXPECT_FALSE(GetIsConnectingOrConnectedStatus());

  NotifyConnectionStatusChanged(
      mojom::ConnectionStatus::kConnectionStatusConnecting);

  EXPECT_EQ(GetLastConnectionForUiChangedStatus(),
            mojom::ConnectionStatus::kConnectionStatusDisconnected);
  EXPECT_EQ(GetNumConnectionStatusForUiChangedCalls(), 0u);
  EXPECT_EQ(GetNumConnectionStatusChangedCalls(), 1u);
  EXPECT_TRUE(GetIsConnectingOrConnectedStatus());

  handler().CheckConnectionStatusForUi();

  EXPECT_EQ(GetLastConnectionForUiChangedStatus(),
            mojom::ConnectionStatus::kConnectionStatusDisconnected);
  EXPECT_EQ(GetNumConnectionStatusForUiChangedCalls(), 0u);
  EXPECT_TRUE(GetIsConnectingOrConnectedStatus());

  NotifyConnectionStatusChanged(
      mojom::ConnectionStatus::kConnectionStatusConnected);
  EXPECT_EQ(GetLastConnectionForUiChangedStatus(),
            mojom::ConnectionStatus::kConnectionStatusConnected);
  EXPECT_EQ(GetNumConnectionStatusForUiChangedCalls(), 1u);
  EXPECT_TRUE(GetIsConnectingOrConnectedStatus());

  SetFeatureStatus(FeatureStatus::kConnected);
  handler().CheckConnectionStatusForUi();

  EXPECT_EQ(GetLastConnectionChangedStatus(),
            mojom::ConnectionStatus::kConnectionStatusConnected);
  EXPECT_EQ(GetNumConnectionStatusChangedCalls(), 2u);
  EXPECT_TRUE(GetIsConnectingOrConnectedStatus());
}

TEST_F(EcheConnectionStatusHandlerTest,
       CheckConnectionStatusForUi_TimeSinceLastCheckIncreases) {
  SetFeatureStatus(FeatureStatus::kDisconnected);
  handler().CheckConnectionStatusForUi();

  EXPECT_EQ(GetLastConnectionForUiChangedStatus(),
            mojom::ConnectionStatus::kConnectionStatusDisconnected);
  EXPECT_EQ(GetNumConnectionStatusForUiChangedCalls(), 0u);
  EXPECT_EQ(GetNumRequestBackgroundConnectionAttemptCalls(), 0u);
  EXPECT_FALSE(GetIsConnectingOrConnectedStatus());

  NotifyConnectionStatusChanged(
      mojom::ConnectionStatus::kConnectionStatusConnected);
  EXPECT_EQ(GetLastConnectionForUiChangedStatus(),
            mojom::ConnectionStatus::kConnectionStatusConnected);
  EXPECT_EQ(GetNumConnectionStatusForUiChangedCalls(), 1u);
  EXPECT_TRUE(GetIsConnectingOrConnectedStatus());

  // After more than 10 seconds pass, extra calls should happen when there is no
  // active stream.
  NotifyConnectionStatusChanged(
      mojom::ConnectionStatus::kConnectionStatusDisconnected);
  SetFeatureStatus(FeatureStatus::kConnected);
  task_environment_.FastForwardBy(base::Seconds(11));
  handler().CheckConnectionStatusForUi();

  EXPECT_EQ(GetLastConnectionForUiChangedStatus(),
            mojom::ConnectionStatus::kConnectionStatusConnected);
  EXPECT_EQ(GetNumConnectionStatusForUiChangedCalls(), 2u);
  EXPECT_EQ(GetNumRequestBackgroundConnectionAttemptCalls(), 1u);
  EXPECT_FALSE(GetIsConnectingOrConnectedStatus());

  // After more than 10 seconds pass, no extra calls should happen if there's an
  // active stream.
  NotifyConnectionStatusChanged(
      mojom::ConnectionStatus::kConnectionStatusConnected);
  task_environment_.FastForwardBy(base::Seconds(11));
  handler().CheckConnectionStatusForUi();

  EXPECT_EQ(GetLastConnectionForUiChangedStatus(),
            mojom::ConnectionStatus::kConnectionStatusConnected);
  EXPECT_EQ(GetNumConnectionStatusForUiChangedCalls(), 2u);        // no change
  EXPECT_EQ(GetNumRequestBackgroundConnectionAttemptCalls(), 1u);  // no change
  EXPECT_TRUE(GetIsConnectingOrConnectedStatus());                 // no change

  // Reset to Disconnected
  handler().SetConnectionStatusForUi(
      mojom::ConnectionStatus::kConnectionStatusDisconnected);
  EXPECT_EQ(GetNumConnectionStatusForUiChangedCalls(), 3u);
  EXPECT_EQ(GetNumRequestBackgroundConnectionAttemptCalls(), 1u);
  EXPECT_EQ(GetLastConnectionForUiChangedStatus(),
            mojom::ConnectionStatus::kConnectionStatusDisconnected);
  EXPECT_TRUE(GetIsConnectingOrConnectedStatus());

  // After more than 10 minutes pass, state should go back to Connecting.
  handler().SetConnectionStatusForUi(
      mojom::ConnectionStatus::kConnectionStatusConnected);
  EXPECT_EQ(GetNumConnectionStatusForUiChangedCalls(), 4u);
  EXPECT_TRUE(GetIsConnectingOrConnectedStatus());

  NotifyConnectionStatusChanged(
      mojom::ConnectionStatus::kConnectionStatusDisconnected);
  task_environment_.FastForwardBy(base::Minutes(11));
  handler().CheckConnectionStatusForUi();

  EXPECT_EQ(GetLastConnectionForUiChangedStatus(),
            mojom::ConnectionStatus::kConnectionStatusConnecting);
  EXPECT_EQ(GetNumConnectionStatusForUiChangedCalls(), 5u);
  EXPECT_EQ(GetNumRequestBackgroundConnectionAttemptCalls(), 2u);
  EXPECT_FALSE(GetIsConnectingOrConnectedStatus());
}

TEST_F(EcheConnectionStatusHandlerTest, SetConnectionStatusForUi) {
  handler().SetConnectionStatusForUi(
      mojom::ConnectionStatus::kConnectionStatusDisconnected);
  EXPECT_EQ(GetLastConnectionForUiChangedStatus(),
            mojom::ConnectionStatus::kConnectionStatusDisconnected);
  EXPECT_EQ(GetNumConnectionStatusForUiChangedCalls(), 0u);
  EXPECT_FALSE(GetIsConnectingOrConnectedStatus());

  handler().SetConnectionStatusForUi(
      mojom::ConnectionStatus::kConnectionStatusConnecting);
  EXPECT_EQ(GetLastConnectionForUiChangedStatus(),
            mojom::ConnectionStatus::kConnectionStatusConnecting);
  EXPECT_EQ(GetNumConnectionStatusForUiChangedCalls(), 1u);
  EXPECT_FALSE(GetIsConnectingOrConnectedStatus());

  handler().SetConnectionStatusForUi(
      mojom::ConnectionStatus::kConnectionStatusConnected);
  EXPECT_EQ(GetLastConnectionForUiChangedStatus(),
            mojom::ConnectionStatus::kConnectionStatusConnected);
  EXPECT_EQ(GetNumConnectionStatusForUiChangedCalls(), 2u);
  EXPECT_FALSE(GetIsConnectingOrConnectedStatus());

  handler().SetConnectionStatusForUi(
      mojom::ConnectionStatus::kConnectionStatusFailed);
  EXPECT_EQ(GetLastConnectionForUiChangedStatus(),
            mojom::ConnectionStatus::kConnectionStatusFailed);
  EXPECT_EQ(GetNumConnectionStatusForUiChangedCalls(), 3u);
  EXPECT_FALSE(GetIsConnectingOrConnectedStatus());

  handler().SetConnectionStatusForUi(
      mojom::ConnectionStatus::kConnectionStatusDisconnected);
  EXPECT_EQ(GetLastConnectionForUiChangedStatus(),
            mojom::ConnectionStatus::kConnectionStatusDisconnected);
  EXPECT_EQ(GetNumConnectionStatusForUiChangedCalls(), 4u);
  EXPECT_FALSE(GetIsConnectingOrConnectedStatus());
}

TEST_F(EcheConnectionStatusHandlerTest, OnFeatureStatusChanged) {
  handler().OnFeatureStatusChanged(FeatureStatus::kDisconnected);
  // always resets to "loading" on disconnections.
  EXPECT_EQ(GetLastConnectionForUiChangedStatus(),
            mojom::ConnectionStatus::kConnectionStatusConnecting);
  EXPECT_EQ(GetNumConnectionStatusForUiChangedCalls(), 1u);
  EXPECT_FALSE(GetIsConnectingOrConnectedStatus());

  handler().SetConnectionStatusForUi(
      mojom::ConnectionStatus::kConnectionStatusConnected);

  EXPECT_EQ(GetLastConnectionForUiChangedStatus(),
            mojom::ConnectionStatus::kConnectionStatusConnected);
  EXPECT_EQ(GetNumConnectionStatusForUiChangedCalls(), 2u);
  EXPECT_FALSE(GetIsConnectingOrConnectedStatus());

  handler().OnFeatureStatusChanged(FeatureStatus::kConnected);

  task_environment_.FastForwardBy(base::Seconds(2));

  EXPECT_EQ(GetLastConnectionForUiChangedStatus(),
            mojom::ConnectionStatus::kConnectionStatusConnected);
  EXPECT_EQ(GetNumConnectionStatusForUiChangedCalls(), 3u);
  EXPECT_FALSE(GetIsConnectingOrConnectedStatus());
}

}  // namespace ash::eche_app
