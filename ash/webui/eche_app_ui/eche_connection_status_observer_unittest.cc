// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/webui/eche_app_ui/eche_connection_status_observer.h"

#include "ash/constants/ash_features.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/task_environment.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace ash::eche_app {

namespace {

class FakeObserver : public EcheConnectionStatusObserver::Observer {
 public:
  FakeObserver() = default;
  ~FakeObserver() override = default;

  size_t num_connection_status_changed_calls() const {
    return num_connection_status_changed_calls_;
  }

  mojom::ConnectionStatus last_connection_changed_status() const {
    return last_connection_changed_status_;
  }

  // EcheConnectionStatusObserver::Observer:
  void OnConnectionStatusChanged(
      mojom::ConnectionStatus connection_status) override {
    ++num_connection_status_changed_calls_;
    last_connection_changed_status_ = connection_status;
  }

 private:
  size_t num_connection_status_changed_calls_ = 0;
  mojom::ConnectionStatus last_connection_changed_status_ =
      mojom::ConnectionStatus::kConnectionStatusDisconnected;
};

}  // namespace

class EcheConnectionStatusObserverTest : public testing::Test {
 public:
  EcheConnectionStatusObserverTest() = default;
  EcheConnectionStatusObserverTest(const EcheConnectionStatusObserverTest&) =
      delete;
  EcheConnectionStatusObserverTest& operator=(
      const EcheConnectionStatusObserverTest&) = delete;
  ~EcheConnectionStatusObserverTest() override = default;

  // testing::Test:
  void SetUp() override {
    scoped_feature_list_.InitWithFeatures(
        /*enabled_features=*/{features::kEcheSWA,
                              features::kEcheNetworkConnectionState},
        /*disabled_features=*/{});

    observer_ = std::make_unique<EcheConnectionStatusObserver>();
    observer_->AddObserver(&fake_observer_);
  }

  void TearDown() override {
    observer_->RemoveObserver(&fake_observer_);
    observer_.reset();
  }

  void NotifyConnectionStatusChanged(
      mojom::ConnectionStatus connection_status) {
    observer_->OnConnectionStatusChanged(connection_status);
  }

  size_t GetNumConnectionStatusChangedCalls() const {
    return fake_observer_.num_connection_status_changed_calls();
  }

  mojom::ConnectionStatus GetLastConnectionChangedStatus() const {
    return fake_observer_.last_connection_changed_status();
  }

  base::test::ScopedFeatureList scoped_feature_list_;

 private:
  FakeObserver fake_observer_;
  std::unique_ptr<EcheConnectionStatusObserver> observer_;
};

TEST_F(EcheConnectionStatusObserverTest, OnConnectionStatusChanged) {
  EXPECT_EQ(GetLastConnectionChangedStatus(),
            mojom::ConnectionStatus::kConnectionStatusDisconnected);
  EXPECT_EQ(GetNumConnectionStatusChangedCalls(), 0u);

  NotifyConnectionStatusChanged(
      mojom::ConnectionStatus::kConnectionStatusConnecting);

  EXPECT_EQ(GetLastConnectionChangedStatus(),
            mojom::ConnectionStatus::kConnectionStatusConnecting);
  EXPECT_EQ(GetNumConnectionStatusChangedCalls(), 1u);

  NotifyConnectionStatusChanged(
      mojom::ConnectionStatus::kConnectionStatusConnected);

  EXPECT_EQ(GetLastConnectionChangedStatus(),
            mojom::ConnectionStatus::kConnectionStatusConnected);
  EXPECT_EQ(GetNumConnectionStatusChangedCalls(), 2u);

  NotifyConnectionStatusChanged(
      mojom::ConnectionStatus::kConnectionStatusFailed);

  EXPECT_EQ(GetLastConnectionChangedStatus(),
            mojom::ConnectionStatus::kConnectionStatusFailed);
  EXPECT_EQ(GetNumConnectionStatusChangedCalls(), 3u);

  NotifyConnectionStatusChanged(
      mojom::ConnectionStatus::kConnectionStatusDisconnected);

  EXPECT_EQ(GetLastConnectionChangedStatus(),
            mojom::ConnectionStatus::kConnectionStatusDisconnected);
  EXPECT_EQ(GetNumConnectionStatusChangedCalls(), 4u);
}

TEST_F(EcheConnectionStatusObserverTest,
       OnConnectionStatusChangedFlagDisabled) {
  scoped_feature_list_.Reset();
  scoped_feature_list_.InitWithFeatures(
      /*enabled_features=*/{features::kEcheSWA},
      /*disabled_features=*/{features::kEcheNetworkConnectionState});

  EXPECT_EQ(GetLastConnectionChangedStatus(),
            mojom::ConnectionStatus::kConnectionStatusDisconnected);
  EXPECT_EQ(GetNumConnectionStatusChangedCalls(), 0u);

  NotifyConnectionStatusChanged(
      mojom::ConnectionStatus::kConnectionStatusConnecting);

  EXPECT_EQ(GetLastConnectionChangedStatus(),
            mojom::ConnectionStatus::kConnectionStatusDisconnected);
  EXPECT_EQ(GetNumConnectionStatusChangedCalls(), 0u);

  NotifyConnectionStatusChanged(
      mojom::ConnectionStatus::kConnectionStatusConnected);

  EXPECT_EQ(GetLastConnectionChangedStatus(),
            mojom::ConnectionStatus::kConnectionStatusDisconnected);
  EXPECT_EQ(GetNumConnectionStatusChangedCalls(), 0u);

  NotifyConnectionStatusChanged(
      mojom::ConnectionStatus::kConnectionStatusFailed);

  EXPECT_EQ(GetLastConnectionChangedStatus(),
            mojom::ConnectionStatus::kConnectionStatusDisconnected);
  EXPECT_EQ(GetNumConnectionStatusChangedCalls(), 0u);
}

}  // namespace ash::eche_app
