// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/borealis/infra/transition.h"

#include <memory>

#include "base/functional/bind.h"
#include "base/strings/string_number_conversions.h"
#include "base/test/task_environment.h"
#include "chrome/browser/ash/borealis/testing/callback_factory.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace borealis {
namespace {

template <typename F>
using CallbackFactory = StrictCallbackFactory<F>;

class ParseIntTransition : public Transition<std::string, int, bool> {
  void Start(std::unique_ptr<std::string> in) override {
    int val;
    if (base::StringToInt(*in, &val)) {
      Succeed(std::make_unique<int>(val));
    } else {
      Fail(false);
    }
  }
};

TEST(TransitionTest, TransitionCanTransformInputToOutput) {
  base::test::SingleThreadTaskEnvironment task_environment;
  ParseIntTransition transition;
  CallbackFactory<ParseIntTransition::OnCompleteSignature> callback_handler;

  EXPECT_CALL(callback_handler, Call(testing::_))
      .WillOnce(testing::Invoke([](ParseIntTransition::Result result) {
        ASSERT_TRUE(result.has_value());
        EXPECT_EQ(*result.value(), 12345);
      }));

  transition.Begin(std::make_unique<std::string>("12345"),
                   callback_handler.BindOnce());
  task_environment.RunUntilIdle();
}

TEST(TransitionTest, TransitionCanFail) {
  base::test::SingleThreadTaskEnvironment task_environment;
  ParseIntTransition transition;
  CallbackFactory<ParseIntTransition::OnCompleteSignature> callback_handler;

  EXPECT_CALL(callback_handler, Call(testing::_))
      .WillOnce(testing::Invoke([](ParseIntTransition::Result result) {
        EXPECT_FALSE(result.has_value());
      }));

  transition.Begin(std::make_unique<std::string>("not a number"),
                   callback_handler.BindOnce());
  task_environment.RunUntilIdle();
}

class MultiCompletionTransition
    : public Transition<std::string, std::string, std::string> {
  void Start(std::unique_ptr<std::string> in) override {
    Fail("foo");
    Succeed(std::make_unique<std::string>("bar"));
  }
};

TEST(TransitionTest, MultipleCompletionFiresCallbackOnce) {
  base::test::SingleThreadTaskEnvironment task_environment;
  MultiCompletionTransition transition;
  CallbackFactory<MultiCompletionTransition::OnCompleteSignature>
      callback_handler;

  EXPECT_CALL(callback_handler, Call(testing::_))
      .WillOnce(testing::Invoke([](MultiCompletionTransition::Result result) {
        // The transition completes twice but only the first one will be used.
        EXPECT_FALSE(result.has_value());
        EXPECT_EQ(result.error(), "foo");
      }));

  transition.Begin(nullptr, callback_handler.BindOnce());
  task_environment.RunUntilIdle();
}

}  // namespace
}  // namespace borealis
