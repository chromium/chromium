// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/borealis/infra/state_manager.h"

#include <memory>

#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "base/test/task_environment.h"
#include "chrome/browser/ash/borealis/infra/expected.h"
#include "chrome/browser/ash/borealis/testing/callback_factory.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

namespace borealis {
namespace {

template <typename F>
using CallbackFactory = StrictCallbackFactory<F>;

struct Foo {
  std::string msg;
};
struct Bar {
  std::string msg;
};
struct Baz {
  std::string msg;
};

class MockStateManager : public BorealisStateManager<Foo, Bar, Baz> {
 public:
  MOCK_METHOD(std::unique_ptr<OnTransition>, GetOnTransition, (), ());
  MOCK_METHOD(std::unique_ptr<OffTransition>, GetOffTransition, (), ());
  MOCK_METHOD(Bar, GetIsTurningOffError, (), ());
  MOCK_METHOD(Baz, GetIsTurningOnError, (), ());
};

class NonCompletingOnTransition : public MockStateManager::OnTransition {
  void Start(std::unique_ptr<MockStateManager::OffState> in) override {}
};

class SucceedingOnTransition : public MockStateManager::OnTransition {
  void Start(std::unique_ptr<MockStateManager::OffState> in) override {
    Succeed(std::make_unique<Foo>());
  }
};

class FailingOnTransition : public MockStateManager::OnTransition {
  void Start(std::unique_ptr<MockStateManager::OffState> in) override {
    Fail(Bar{.msg = "failure turning on"});
  }
};

class NonCompletingOffTransition : public MockStateManager::OffTransition {
  void Start(std::unique_ptr<Foo> in) override {}
};

class SucceedingOffTransition : public MockStateManager::OffTransition {
  void Start(std::unique_ptr<Foo> in) override { Succeed(nullptr); }
};

class FailingOffTransition : public MockStateManager::OffTransition {
  void Start(std::unique_ptr<Foo> in) override {
    Fail(Baz{.msg = "failure turning off"});
  }
};

TEST(BorealisStateManagerTest, DefaultStateIsOff) {
  testing::StrictMock<MockStateManager> state_manager;
  CallbackFactory<MockStateManager::WhenOff> on_callback_handler;
  // State managers are created in the "Off" state, so we don't need to
  // transition there.
  EXPECT_CALL(state_manager, GetOffTransition).Times(0);
  EXPECT_CALL(on_callback_handler, Call(testing::Eq(absl::nullopt)));
  state_manager.TurnOff(on_callback_handler.BindOnce());
}

TEST(BorealisStateManagerTest, CanBeTurnedOnAndOff) {
  base::test::SingleThreadTaskEnvironment task_environment;
  testing::StrictMock<MockStateManager> state_manager;
  CallbackFactory<MockStateManager::WhenOn> on_callback_handler;
  CallbackFactory<MockStateManager::WhenOff> off_callback_handler;

  EXPECT_CALL(state_manager, GetOnTransition).WillOnce(testing::Invoke([]() {
    return std::make_unique<SucceedingOnTransition>();
  }));
  EXPECT_CALL(state_manager, GetOffTransition).WillOnce(testing::Invoke([]() {
    return std::make_unique<SucceedingOffTransition>();
  }));
  EXPECT_CALL(on_callback_handler, Call(testing::_))
      .WillOnce(testing::Invoke(
          [](Expected<Foo*, Bar> result) { EXPECT_TRUE(result); }));
  EXPECT_CALL(off_callback_handler, Call(testing::Eq(absl::nullopt)));

  state_manager.TurnOn(on_callback_handler.BindOnce());
  task_environment.RunUntilIdle();
  state_manager.TurnOff(off_callback_handler.BindOnce());
  task_environment.RunUntilIdle();
}

TEST(BorealisStateManagerTest, CanHandleMultipleCallbacks) {
  base::test::SingleThreadTaskEnvironment task_environment;
  testing::StrictMock<MockStateManager> state_manager;
  CallbackFactory<MockStateManager::WhenOn> on_callback_handler;

  EXPECT_CALL(state_manager, GetOnTransition).WillOnce(testing::Invoke([]() {
    return std::make_unique<SucceedingOnTransition>();
  }));

  state_manager.TurnOn(on_callback_handler.BindOnce());
  state_manager.TurnOn(on_callback_handler.BindOnce());

  // The above two callbacks will not be run until the sequence gets a chance to
  // execute. We assure this by making the expectations after them.
  EXPECT_CALL(on_callback_handler, Call(testing::_))
      .Times(3)
      .WillRepeatedly(testing::Invoke(
          [](Expected<Foo*, Bar> result) { EXPECT_TRUE(result); }));

  // The two callbacks will have a chance to run now.
  task_environment.RunUntilIdle();
  state_manager.TurnOn(on_callback_handler.BindOnce());
}

TEST(BorealisStateManagerTest, TurnOffRejectedWhileTurningOn) {
  testing::StrictMock<MockStateManager> state_manager;
  CallbackFactory<MockStateManager::WhenOff> off_callback_handler;

  EXPECT_CALL(state_manager, GetOnTransition).WillOnce(testing::Invoke([]() {
    return std::make_unique<NonCompletingOnTransition>();
  }));
  EXPECT_CALL(state_manager, GetIsTurningOnError)
      .WillOnce(testing::Return(Baz{.msg = "rejected"}));
  EXPECT_CALL(off_callback_handler, Call(testing::_))
      .WillOnce(testing::Invoke([](absl::optional<Baz> err) {
        ASSERT_TRUE(err.has_value());
        EXPECT_EQ(err->msg, "rejected");
      }));

  state_manager.TurnOn(base::DoNothing());
  state_manager.TurnOff(off_callback_handler.BindOnce());
}

TEST(BorealisStateManagerTest, TurnOnRejectedWhileTurningOff) {
  base::test::SingleThreadTaskEnvironment task_environment;
  testing::StrictMock<MockStateManager> state_manager;
  CallbackFactory<MockStateManager::WhenOn> on_callback_handler;

  EXPECT_CALL(state_manager, GetOnTransition).WillOnce(testing::Invoke([]() {
    return std::make_unique<SucceedingOnTransition>();
  }));
  EXPECT_CALL(state_manager, GetOffTransition).WillOnce(testing::Invoke([]() {
    return std::make_unique<NonCompletingOffTransition>();
  }));
  EXPECT_CALL(state_manager, GetIsTurningOffError)
      .WillOnce(testing::Return(Bar{.msg = "blocked"}));
  EXPECT_CALL(on_callback_handler, Call(testing::_))
      .WillOnce(testing::Invoke([](Expected<Foo*, Bar> result) {
        EXPECT_TRUE(result.Unexpected());
        EXPECT_EQ(result.Error().msg, "blocked");
      }));

  state_manager.TurnOn(base::DoNothing());
  task_environment.RunUntilIdle();
  state_manager.TurnOff(base::DoNothing());
  state_manager.TurnOn(on_callback_handler.BindOnce());
}

TEST(BorealisStateManagerTest, FailureToTurnOnProducesAnErrorAndResultsInOff) {
  base::test::SingleThreadTaskEnvironment task_environment;
  testing::StrictMock<MockStateManager> state_manager;
  CallbackFactory<MockStateManager::WhenOn> on_callback_handler;

  EXPECT_CALL(state_manager, GetOnTransition).WillOnce(testing::Invoke([]() {
    return std::make_unique<FailingOnTransition>();
  }));
  EXPECT_CALL(on_callback_handler, Call(testing::_))
      .WillOnce(testing::Invoke(
          [](Expected<Foo*, Bar> result) { EXPECT_FALSE(result); }));

  state_manager.TurnOn(on_callback_handler.BindOnce());
  task_environment.RunUntilIdle();

  // Additional call to turn off requires no transition, because the state is
  // off.
  CallbackFactory<MockStateManager::WhenOff> off_callback_handler;
  EXPECT_CALL(off_callback_handler, Call(testing::Eq(absl::nullopt)));
  state_manager.TurnOff(off_callback_handler.BindOnce());
}

TEST(BorealisStateManagerTest, FailureToTurnOffProducesErrorButDoesTurnOff) {
  base::test::SingleThreadTaskEnvironment task_environment;
  testing::StrictMock<MockStateManager> state_manager;
  CallbackFactory<MockStateManager::WhenOn> on_callback_handler;
  CallbackFactory<MockStateManager::WhenOff> off_callback_handler;

  EXPECT_CALL(state_manager, GetOnTransition).WillOnce(testing::Invoke([]() {
    return std::make_unique<SucceedingOnTransition>();
  }));
  EXPECT_CALL(state_manager, GetOffTransition).WillOnce(testing::Invoke([]() {
    return std::make_unique<FailingOffTransition>();
  }));
  EXPECT_CALL(on_callback_handler, Call(testing::_))
      .WillOnce(testing::Invoke(
          [](Expected<Foo*, Bar> result) { EXPECT_TRUE(result); }));
  EXPECT_CALL(off_callback_handler,
              Call(testing::Not(testing::Eq(absl::nullopt))));

  state_manager.TurnOn(on_callback_handler.BindOnce());
  task_environment.RunUntilIdle();
  state_manager.TurnOff(off_callback_handler.BindOnce());
  task_environment.RunUntilIdle();

  // Additional call to turn off requires no transition, because the state is
  // off.
  EXPECT_CALL(off_callback_handler, Call(testing::Eq(absl::nullopt)));
  state_manager.TurnOff(off_callback_handler.BindOnce());
}

}  // namespace
}  // namespace borealis
