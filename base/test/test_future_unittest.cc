// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/test/test_future.h"

#include "base/dcheck_is_on.h"
#include "base/logging.h"
#include "base/macros.h"
#include "base/run_loop.h"
#include "base/synchronization/waitable_event.h"
#include "base/task/post_task.h"
#include "base/task/thread_pool.h"
#include "base/test/bind.h"
#include "base/test/gtest_util.h"
#include "base/test/task_environment.h"
#include "base/threading/sequenced_task_runner_handle.h"
#include "base/threading/thread_task_runner_handle.h"
#include "testing/gtest/include/gtest/gtest-spi.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace base {
namespace test {

namespace {

using AnyType = int;
constexpr int kAnyValue = 5;
constexpr int kOtherValue = 10;

struct MoveOnlyValue {
 public:
  MoveOnlyValue() = default;
  MoveOnlyValue(const MoveOnlyValue&) = delete;
  auto& operator=(const MoveOnlyValue&) = delete;
  MoveOnlyValue(MoveOnlyValue&&) = default;
  MoveOnlyValue& operator=(MoveOnlyValue&&) = default;
  ~MoveOnlyValue() = default;

  int data;
};

}  // namespace

class TestFutureTest : public ::testing::Test {
 public:
  TestFutureTest() = default;
  TestFutureTest(const TestFutureTest&) = delete;
  TestFutureTest& operator=(const TestFutureTest&) = delete;
  ~TestFutureTest() override = default;

  template <typename Lambda>
  void RunLater(Lambda lambda) {
    base::ThreadTaskRunnerHandle::Get()->PostTask(
        FROM_HERE, base::BindLambdaForTesting(lambda));
  }

  void RunLater(base::OnceClosure callable) {
    base::ThreadTaskRunnerHandle::Get()->PostTask(FROM_HERE,
                                                  std::move(callable));
  }

  void PostDelayedTask(base::OnceClosure callable, base::TimeDelta delay) {
    base::ThreadTaskRunnerHandle::Get()->PostDelayedTask(
        FROM_HERE, std::move(callable), delay);
  }

 private:
  base::test::SingleThreadTaskEnvironment environment_{
      base::test::TaskEnvironment::TimeSource::MOCK_TIME};
};

TEST_F(TestFutureTest, WaitShouldBlockUntilValueArrives) {
  const int expected_value = 42;
  TestFuture<int> future;

  PostDelayedTask(base::BindOnce(future.GetCallback(), expected_value),
                  base::TimeDelta::FromMilliseconds(1));

  future.Wait();

  EXPECT_EQ(expected_value, future.Get());
}

TEST_F(TestFutureTest, WaitShouldReturnTrueWhenValueArrives) {
  TestFuture<int> future;

  PostDelayedTask(base::BindOnce(future.GetCallback(), kAnyValue),
                  base::TimeDelta::FromMilliseconds(1));

  bool success = future.Wait();
  EXPECT_TRUE(success);
}

TEST_F(TestFutureTest, WaitShouldReturnFalseIfTimeoutHappens) {
  base::test::ScopedRunLoopTimeout timeout(
      FROM_HERE, base::TimeDelta::FromMilliseconds(1));

  // |ScopedRunLoopTimeout| will automatically fail the test when a timeout
  // happens, so we use EXPECT_FATAL_FAILURE to handle this failure.
  // EXPECT_FATAL_FAILURE only works on static objects.
  static bool success;
  static TestFuture<AnyType> future;

  EXPECT_FATAL_FAILURE({ success = future.Wait(); }, "timed out");

  EXPECT_FALSE(success);
}

TEST_F(TestFutureTest, GetShouldBlockUntilValueArrives) {
  const int expected_value = 42;
  TestFuture<int> future;

  PostDelayedTask(base::BindOnce(future.GetCallback(), expected_value),
                  base::TimeDelta::FromMilliseconds(1));

  int actual_value = future.Get();

  EXPECT_EQ(expected_value, actual_value);
}

TEST_F(TestFutureTest, GetShouldDcheckIfTimeoutHappens) {
  base::test::ScopedRunLoopTimeout timeout(
      FROM_HERE, base::TimeDelta::FromMilliseconds(1));

  TestFuture<AnyType> future;

  EXPECT_DCHECK_DEATH_WITH((void)future.Get(), "timed out");
}

TEST_F(TestFutureTest, TakeShouldWorkWithMoveOnlyValue) {
  const int expected_data = 99;
  TestFuture<MoveOnlyValue> future;

  RunLater(base::BindOnce(future.GetCallback(), MoveOnlyValue{expected_data}));

  MoveOnlyValue actual_value = future.Take();

  EXPECT_EQ(expected_data, actual_value.data);
}

TEST_F(TestFutureTest, TakeShouldDcheckIfTimeoutHappens) {
  base::test::ScopedRunLoopTimeout timeout(
      FROM_HERE, base::TimeDelta::FromMilliseconds(1));

  TestFuture<AnyType> future;

  EXPECT_DCHECK_DEATH_WITH((void)future.Take(), "timed out");
}

TEST_F(TestFutureTest, IsReadyShouldBeTrueWhenValueIsSet) {
  TestFuture<AnyType> future;

  EXPECT_FALSE(future.IsReady());

  future.SetValue(kAnyValue);

  EXPECT_TRUE(future.IsReady());
}

TEST_F(TestFutureTest, ShouldOnlyAllowSetValueToBeCalledOnce) {
  TestFuture<AnyType> future;

  future.SetValue(kAnyValue);

  EXPECT_DCHECK_DEATH_WITH(future.SetValue(kOtherValue),
                           "The value of a TestFuture can only be set once.");
}

TEST_F(TestFutureTest, ShouldUnblockWhenSetValueIsInvoked) {
  const int expected_value = 111;
  TestFuture<int> future;

  RunLater([&future]() { future.SetValue(expected_value); });

  int actual_value = future.Get();

  EXPECT_EQ(expected_value, actual_value);
}

TEST_F(TestFutureTest, ShouldAllowReferenceArgumentsForCallback) {
  const int expected_value = 222;
  TestFuture<int> future;

  base::OnceCallback<void(const int&)> callback =
      future.GetCallback<const int&>();
  RunLater(base::BindOnce(std::move(callback), expected_value));

  int actual_value = future.Get();

  EXPECT_EQ(expected_value, actual_value);
}

TEST_F(TestFutureTest, ShouldAllowInvokingCallbackAfterFutureIsDestroyed) {
  base::OnceCallback<void(int)> callback;

  {
    TestFuture<int> future;
    callback = future.GetCallback();
  }

  std::move(callback).Run(1);
}

TEST_F(TestFutureTest, ShouldReturnTupleValue) {
  const int expected_int_value = 5;
  const std::string expected_string_value = "value";

  TestFuture<int, std::string> future;

  RunLater(base::BindOnce(future.GetCallback(), expected_int_value,
                          expected_string_value));

  const std::tuple<int, std::string>& actual = future.Get();

  EXPECT_EQ(expected_int_value, std::get<0>(actual));
  EXPECT_EQ(expected_string_value, std::get<1>(actual));
}

TEST_F(TestFutureTest, ShouldAllowAccessingTupleValueThroughGetMethod) {
  const int expected_int_value = 5;
  const std::string expected_string_value = "value";

  TestFuture<int, std::string> future;

  RunLater(base::BindOnce(future.GetCallback(), expected_int_value,
                          expected_string_value));

  ignore_result(future.Get());

  EXPECT_EQ(expected_int_value, future.Get<0>());
  EXPECT_EQ(expected_string_value, future.Get<1>());
}

TEST_F(TestFutureTest, ShouldAllowReferenceArgumentsForMultiArgumentCallback) {
  const int expected_int_value = 5;
  const std::string expected_string_value = "value";

  TestFuture<int, std::string> future;

  base::OnceCallback<void(int, const std::string&)> callback =
      future.GetCallback<int, const std::string&>();
  RunLater(base::BindOnce(std::move(callback), expected_int_value,
                          expected_string_value));

  std::tuple<int, std::string> actual = future.Get();

  EXPECT_EQ(expected_int_value, std::get<0>(actual));
  EXPECT_EQ(expected_string_value, std::get<1>(actual));
}

TEST_F(TestFutureTest, SetValueShouldAllowMultipleArguments) {
  const int expected_int_value = 5;
  const std::string expected_string_value = "value";

  TestFuture<int, std::string> future;

  RunLater([&future, expected_string_value]() {
    future.SetValue(expected_int_value, expected_string_value);
  });

  const std::tuple<int, std::string>& actual = future.Get();

  EXPECT_EQ(expected_int_value, std::get<0>(actual));
  EXPECT_EQ(expected_string_value, std::get<1>(actual));
}

}  // namespace test
}  // namespace base
