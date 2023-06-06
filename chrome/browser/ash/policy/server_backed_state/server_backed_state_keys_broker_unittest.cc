// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/policy/server_backed_state/server_backed_state_keys_broker.h"

#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "base/test/task_environment.h"
#include "chromeos/ash/components/dbus/session_manager/fake_session_manager_client.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace policy {

class ServerBackedStateKeysBrokerTest : public testing::Test {
 public:
  ServerBackedStateKeysBrokerTest() {
    fake_session_manager_client_.set_server_backed_state_keys(state_keys_);
  }

  ServerBackedStateKeysBrokerTest(const ServerBackedStateKeysBrokerTest&) =
      delete;
  ServerBackedStateKeysBrokerTest& operator=(
      const ServerBackedStateKeysBrokerTest&) = delete;

  ~ServerBackedStateKeysBrokerTest() override = default;

  void StateKeysUpdated() { updated_ = true; }

  void ExpectGood() {
    EXPECT_TRUE(broker_.available());
    EXPECT_EQ(state_keys_, broker_.state_keys());
    EXPECT_EQ(state_keys_.front(), broker_.current_state_key());
  }

  void HandleStateKeysCallback(const std::vector<std::string>& state_keys) {
    callback_invoked_ = true;
    callback_state_keys_ = state_keys;
  }

 protected:
  base::test::SingleThreadTaskEnvironment task_environment_{
      base::test::TaskEnvironment::TimeSource::MOCK_TIME};
  ash::FakeSessionManagerClient fake_session_manager_client_;
  ServerBackedStateKeysBroker broker_{&fake_session_manager_client_};
  std::vector<std::string> state_keys_{"1", "2", "3"};
  bool updated_{false};
  std::vector<std::string> callback_state_keys_;
  bool callback_invoked_{false};
};

TEST_F(ServerBackedStateKeysBrokerTest, Load) {
  EXPECT_FALSE(broker_.available());
  EXPECT_TRUE(broker_.state_keys().empty());
  EXPECT_TRUE(broker_.current_state_key().empty());

  base::CallbackListSubscription subscription = broker_.RegisterUpdateCallback(
      base::BindRepeating(&ServerBackedStateKeysBrokerTest::StateKeysUpdated,
                          base::Unretained(this)));
  task_environment_.RunUntilIdle();
  EXPECT_TRUE(updated_);
  ExpectGood();
}

TEST_F(ServerBackedStateKeysBrokerTest, Retry) {
  fake_session_manager_client_.set_server_backed_state_keys(
      std::vector<std::string>());

  base::CallbackListSubscription subscription = broker_.RegisterUpdateCallback(
      base::BindRepeating(&ServerBackedStateKeysBrokerTest::StateKeysUpdated,
                          base::Unretained(this)));
  task_environment_.RunUntilIdle();
  EXPECT_TRUE(updated_);

  EXPECT_FALSE(broker_.available());
  EXPECT_TRUE(broker_.state_keys().empty());
  EXPECT_TRUE(broker_.current_state_key().empty());

  fake_session_manager_client_.set_server_backed_state_keys(state_keys_);
  updated_ = false;
  base::CallbackListSubscription subscription2 =
      broker_.RegisterUpdateCallback(base::DoNothing());
  task_environment_.RunUntilIdle();
  EXPECT_TRUE(updated_);
  ExpectGood();
}

TEST_F(ServerBackedStateKeysBrokerTest, Refresh) {
  base::CallbackListSubscription subscription = broker_.RegisterUpdateCallback(
      base::BindRepeating(&ServerBackedStateKeysBrokerTest::StateKeysUpdated,
                          base::Unretained(this)));
  task_environment_.RunUntilIdle();
  EXPECT_TRUE(updated_);
  ExpectGood();

  // Update callbacks get fired if the keys change.
  state_keys_ = {"2", "3", "4"};
  fake_session_manager_client_.set_server_backed_state_keys(state_keys_);
  updated_ = false;
  task_environment_.FastForwardBy(
      ServerBackedStateKeysBroker::GetPollIntervalForTesting());
  EXPECT_TRUE(updated_);
  ExpectGood();

  // No update callback if the keys are unchanged.
  updated_ = false;
  task_environment_.FastForwardBy(
      ServerBackedStateKeysBroker::GetPollIntervalForTesting());
  EXPECT_FALSE(updated_);
  ExpectGood();
}

TEST_F(ServerBackedStateKeysBrokerTest, Request) {
  broker_.RequestStateKeys(
      base::BindOnce(&ServerBackedStateKeysBrokerTest::HandleStateKeysCallback,
                     base::Unretained(this)));
  task_environment_.RunUntilIdle();
  ExpectGood();
  EXPECT_TRUE(callback_invoked_);
  EXPECT_EQ(state_keys_, callback_state_keys_);
}

TEST_F(ServerBackedStateKeysBrokerTest, RequestFailure) {
  fake_session_manager_client_.set_server_backed_state_keys(
      std::vector<std::string>());

  broker_.RequestStateKeys(
      base::BindOnce(&ServerBackedStateKeysBrokerTest::HandleStateKeysCallback,
                     base::Unretained(this)));
  task_environment_.RunUntilIdle();
  EXPECT_TRUE(callback_invoked_);
  EXPECT_TRUE(callback_state_keys_.empty());
}

TEST_F(ServerBackedStateKeysBrokerTest, RetryAfterFailure) {
  fake_session_manager_client_.set_server_backed_state_keys(
      std::vector<std::string>());

  base::CallbackListSubscription subscription = broker_.RegisterUpdateCallback(
      base::BindRepeating(&ServerBackedStateKeysBrokerTest::StateKeysUpdated,
                          base::Unretained(this)));
  task_environment_.RunUntilIdle();
  EXPECT_TRUE(updated_);

  EXPECT_FALSE(broker_.available());
  EXPECT_TRUE(broker_.state_keys().empty());
  EXPECT_TRUE(broker_.current_state_key().empty());

  fake_session_manager_client_.set_server_backed_state_keys(state_keys_);
  updated_ = false;
  task_environment_.FastForwardBy(
      ServerBackedStateKeysBroker::GetRetryIntervalForTesting());
  EXPECT_TRUE(updated_);
  ExpectGood();
}

}  // namespace policy
