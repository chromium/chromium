// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/policy/server_backed_state/server_backed_state_keys_broker.h"

#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "base/run_loop.h"
#include "base/test/bind.h"
#include "base/test/task_environment.h"
#include "base/test/test_future.h"
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

  void ExpectGoodBroker() {
    EXPECT_TRUE(broker_.available());
    EXPECT_EQ(state_keys_, broker_.state_keys());
    EXPECT_EQ(state_keys_.front(), broker_.current_state_key());
  }

 protected:
  base::test::SingleThreadTaskEnvironment task_environment_{
      base::test::TaskEnvironment::TimeSource::MOCK_TIME};
  ash::FakeSessionManagerClient fake_session_manager_client_;
  ServerBackedStateKeysBroker broker_{&fake_session_manager_client_};
  std::vector<std::string> state_keys_{"1", "2", "3"};
};

TEST_F(ServerBackedStateKeysBrokerTest, Load) {
  EXPECT_FALSE(broker_.available());
  EXPECT_TRUE(broker_.state_keys().empty());
  EXPECT_TRUE(broker_.current_state_key().empty());

  base::RunLoop run_loop;
  base::CallbackListSubscription subscription =
      broker_.RegisterUpdateCallback(run_loop.QuitClosure());
  run_loop.Run();
  EXPECT_TRUE(run_loop.AnyQuitCalled());
  ExpectGoodBroker();
}

TEST_F(ServerBackedStateKeysBrokerTest, RetryAfterFailure) {
  fake_session_manager_client_.set_server_backed_state_keys({});

  {
    base::RunLoop run_loop;
    base::CallbackListSubscription subscription =
        broker_.RegisterUpdateCallback(run_loop.QuitClosure());
    run_loop.Run();
    EXPECT_TRUE(run_loop.AnyQuitCalled());
  }

  EXPECT_FALSE(broker_.available());
  EXPECT_TRUE(broker_.state_keys().empty());
  EXPECT_TRUE(broker_.current_state_key().empty());

  fake_session_manager_client_.set_server_backed_state_keys(state_keys_);
  {
    base::RunLoop run_loop;
    base::CallbackListSubscription subscription =
        broker_.RegisterUpdateCallback(run_loop.QuitClosure());
    task_environment_.FastForwardBy(
        ServerBackedStateKeysBroker::GetRetryIntervalForTesting());
    EXPECT_TRUE(run_loop.AnyQuitCalled());
  }
  ExpectGoodBroker();
}

TEST_F(ServerBackedStateKeysBrokerTest, RetryAfterCommunicationFailure) {
  fake_session_manager_client_.SetServerBackedStateKeyError(
      ServerBackedStateKeysBroker::ErrorType::kCommunicationError);

  {
    base::RunLoop run_loop;
    base::CallbackListSubscription subscription =
        broker_.RegisterUpdateCallback(run_loop.QuitClosure());
    run_loop.Run();
    EXPECT_TRUE(run_loop.AnyQuitCalled());
  }

  EXPECT_FALSE(broker_.available());
  EXPECT_TRUE(broker_.state_keys().empty());
  EXPECT_TRUE(broker_.current_state_key().empty());

  fake_session_manager_client_.set_server_backed_state_keys(state_keys_);

  {
    base::RunLoop run_loop;
    base::CallbackListSubscription subscription =
        broker_.RegisterUpdateCallback(run_loop.QuitClosure());
    task_environment_.FastForwardBy(
        ServerBackedStateKeysBroker::GetRetryIntervalForTesting());
    EXPECT_TRUE(run_loop.AnyQuitCalled());
  }

  ExpectGoodBroker();
}

TEST_F(ServerBackedStateKeysBrokerTest, Refresh) {
  // Use unique_ptr to make `run_loop` resettable.
  auto run_loop = std::make_unique<base::RunLoop>();
  base::CallbackListSubscription subscription =
      broker_.RegisterUpdateCallback(base::BindLambdaForTesting([&run_loop]() {
        ASSERT_FALSE(run_loop->AnyQuitCalled());
        run_loop->Quit();
      }));
  run_loop->Run();
  EXPECT_TRUE(run_loop->AnyQuitCalled());
  ExpectGoodBroker();

  // Update callbacks get fired if the keys change.
  run_loop = std::make_unique<base::RunLoop>();
  ASSERT_FALSE(run_loop->AnyQuitCalled());
  state_keys_ = {"2", "3", "4"};
  fake_session_manager_client_.set_server_backed_state_keys(state_keys_);
  task_environment_.FastForwardBy(
      ServerBackedStateKeysBroker::GetPollIntervalForTesting());
  EXPECT_TRUE(run_loop->AnyQuitCalled());
  ExpectGoodBroker();

  // No update callback if the keys are unchanged.
  run_loop = std::make_unique<base::RunLoop>();
  ASSERT_FALSE(run_loop->AnyQuitCalled());
  task_environment_.FastForwardBy(
      ServerBackedStateKeysBroker::GetPollIntervalForTesting());
  EXPECT_FALSE(run_loop->AnyQuitCalled());
  ExpectGoodBroker();
}

TEST_F(ServerBackedStateKeysBrokerTest, Request) {
  base::test::TestFuture<const std::vector<std::string>&> state_keys_future;
  broker_.RequestStateKeys(state_keys_future.GetCallback());
  EXPECT_TRUE(state_keys_future.Wait());
  ExpectGoodBroker();
  EXPECT_EQ(state_keys_, state_keys_future.Get());
}

TEST_F(ServerBackedStateKeysBrokerTest, RequestFailure) {
  fake_session_manager_client_.SetServerBackedStateKeyError(
      ServerBackedStateKeysBroker::ErrorType::kInvalidResponse);

  base::test::TestFuture<const std::vector<std::string>&> state_keys_future;
  broker_.RequestStateKeys(state_keys_future.GetCallback());
  EXPECT_TRUE(state_keys_future.Wait());
  EXPECT_TRUE(state_keys_future.Get().empty());
  EXPECT_EQ(broker_.error_type(),
            ServerBackedStateKeysBroker::ErrorType::kInvalidResponse);
}

}  // namespace policy
