// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/policy/server_backed_state_keys_broker.h"

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/macros.h"
#include "base/run_loop.h"
#include "base/test/scoped_mock_time_message_loop_task_runner.h"
#include "base/test/task_environment.h"
#include "chromeos/dbus/session_manager/fake_session_manager_client.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace policy {

class ServerBackedStateKeysBrokerTest : public testing::Test {
 public:
  ServerBackedStateKeysBrokerTest()
      : broker_(&fake_session_manager_client_),
        updated_(false),
        callback_invoked_(false) {
    state_keys_.push_back("1");
    state_keys_.push_back("2");
    state_keys_.push_back("3");
    fake_session_manager_client_.set_server_backed_state_keys(state_keys_);
  }
  ~ServerBackedStateKeysBrokerTest() override {}

  void StateKeysUpdated() {
    updated_ = true;
  }

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
  base::test::SingleThreadTaskEnvironment task_environment_;
  base::ScopedMockTimeMessageLoopTaskRunner mocked_main_runner_;
  chromeos::FakeSessionManagerClient fake_session_manager_client_;
  ServerBackedStateKeysBroker broker_;
  std::vector<std::string> state_keys_;
  bool updated_;
  std::vector<std::string> callback_state_keys_;
  bool callback_invoked_;

 private:
  DISALLOW_COPY_AND_ASSIGN(ServerBackedStateKeysBrokerTest);
};

TEST_F(ServerBackedStateKeysBrokerTest, Load) {
  EXPECT_FALSE(broker_.available());
  EXPECT_TRUE(broker_.state_keys().empty());
  EXPECT_TRUE(broker_.current_state_key().empty());

  ServerBackedStateKeysBroker::Subscription subscription =
      broker_.RegisterUpdateCallback(
          base::Bind(&ServerBackedStateKeysBrokerTest::StateKeysUpdated,
                     base::Unretained(this)));
  mocked_main_runner_->RunUntilIdle();
  EXPECT_TRUE(updated_);
  ExpectGood();
}

TEST_F(ServerBackedStateKeysBrokerTest, Retry) {
  fake_session_manager_client_.set_server_backed_state_keys(
      std::vector<std::string>());

  ServerBackedStateKeysBroker::Subscription subscription =
      broker_.RegisterUpdateCallback(
          base::Bind(&ServerBackedStateKeysBrokerTest::StateKeysUpdated,
                     base::Unretained(this)));
  mocked_main_runner_->RunUntilIdle();
  EXPECT_TRUE(updated_);

  EXPECT_FALSE(broker_.available());
  EXPECT_TRUE(broker_.state_keys().empty());
  EXPECT_TRUE(broker_.current_state_key().empty());

  fake_session_manager_client_.set_server_backed_state_keys(state_keys_);
  updated_ = false;
  ServerBackedStateKeysBroker::Subscription subscription2 =
      broker_.RegisterUpdateCallback(base::DoNothing());
  mocked_main_runner_->RunUntilIdle();
  EXPECT_TRUE(updated_);
  ExpectGood();
}

TEST_F(ServerBackedStateKeysBrokerTest, Refresh) {
  ServerBackedStateKeysBroker::Subscription subscription =
      broker_.RegisterUpdateCallback(
          base::Bind(&ServerBackedStateKeysBrokerTest::StateKeysUpdated,
                     base::Unretained(this)));
  mocked_main_runner_->RunUntilIdle();
  EXPECT_TRUE(updated_);
  ExpectGood();

  // Update callbacks get fired if the keys change.
  state_keys_.erase(state_keys_.begin());
  state_keys_.push_back("4");
  fake_session_manager_client_.set_server_backed_state_keys(state_keys_);
  updated_ = false;
  mocked_main_runner_->FastForwardBy(
      ServerBackedStateKeysBroker::GetPollIntervalForTesting());
  EXPECT_TRUE(updated_);
  ExpectGood();

  // No update callback if the keys are unchanged.
  updated_ = false;
  mocked_main_runner_->FastForwardBy(
      ServerBackedStateKeysBroker::GetPollIntervalForTesting());
  EXPECT_FALSE(updated_);
  ExpectGood();
}

TEST_F(ServerBackedStateKeysBrokerTest, Request) {
  broker_.RequestStateKeys(
      base::Bind(&ServerBackedStateKeysBrokerTest::HandleStateKeysCallback,
                 base::Unretained(this)));
  mocked_main_runner_->RunUntilIdle();
  ExpectGood();
  EXPECT_TRUE(callback_invoked_);
  EXPECT_EQ(state_keys_, callback_state_keys_);
}

TEST_F(ServerBackedStateKeysBrokerTest, RequestFailure) {
  fake_session_manager_client_.set_server_backed_state_keys(
      std::vector<std::string>());

  broker_.RequestStateKeys(
      base::Bind(&ServerBackedStateKeysBrokerTest::HandleStateKeysCallback,
                 base::Unretained(this)));
  mocked_main_runner_->RunUntilIdle();
  EXPECT_TRUE(callback_invoked_);
  EXPECT_TRUE(callback_state_keys_.empty());
}

}  // namespace policy
