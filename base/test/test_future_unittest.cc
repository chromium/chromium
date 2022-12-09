// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/test/test_future.h"

#include <tuple>

#include "base/task/single_thread_task_runner.h"
#include "base/test/bind.h"
#include "base/test/gtest_util.h"
#include "base/test/scoped_run_loop_timeout.h"
#include "base/test/task_environment.h"
#include "testing/gtest/include/gtest/gtest-spi.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace base::test {

namespace {

using AnyType = int;
constexpr int kAnyValue = 5;
constexpr int kOtherValue = 10;

struct MoveOnlyValue {
 public:
  MoveOnlyValue() = default;
  explicit MoveOnlyValue(int data) : data(data) {}
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
    base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
        FROM_HERE, base::BindLambdaForTesting(lambda));
  }

  void RunLater(base::OnceClosure callable) {
    base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
        FROM_HERE, std::move(callable));
  }

  void PostDelayedTask(base::OnceClosure callable, base::TimeDelta delay) {
    base::SingleThreadTaskRunner::GetCurrentDefault()->PostDelayedTask(
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
                  base::Milliseconds(1));

  std::ignore = future.Wait();

  EXPECT_EQ(expected_value, future.Get());
}

TEST_F(TestFutureTest, WaitShouldReturnTrueWhenValueArrives) {
  TestFuture<int> future;

  PostDelayedTask(base::BindOnce(future.GetCallback(), kAnyValue),
                  base::Milliseconds(1));

  bool success = future.Wait();
  EXPECT_TRUE(success);
}

TEST_F(TestFutureTest, WaitShouldReturnFalseIfTimeoutHappens) {
  base::test::ScopedRunLoopTimeout timeout(FROM_HERE, base::Milliseconds(1));

  // `ScopedRunLoopTimeout` will automatically fail the test when a timeout
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
                  base::Milliseconds(1));

  int actual_value = future.Get();

  EXPECT_EQ(expected_value, actual_value);
}

TEST_F(TestFutureTest, GetShouldDcheckIfTimeoutHappens) {
  base::test::ScopedRunLoopTimeout timeout(FROM_HERE, base::Milliseconds(1));

  TestFuture<AnyType> future;

  EXPECT_DCHECK_DEATH_WITH((void)future.Get(), "timed out");
}

TEST_F(TestFutureTest, TakeShouldWorkWithMoveOnlyValue) {
  const int expected_data = 99;
  TestFuture<MoveOnlyValue> future;

  RunLater(base::BindOnce(future.GetCallback(), MoveOnlyValue(expected_data)));

  MoveOnlyValue actual_value = future.Take();

  EXPECT_EQ(expected_data, actual_value.data);
}

TEST_F(TestFutureTest, TakeShouldDcheckIfTimeoutHappens) {
  base::test::ScopedRunLoopTimeout timeout(FROM_HERE, base::Milliseconds(1));

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

TEST_F(TestFutureTest, ShouldSignalWhenSetValueIsInvoked) {
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

TEST_F(TestFutureTest, ShouldAllowAccessingTupleValueUsingGetWithIndex) {
  const int expected_int_value = 5;
  const std::string expected_string_value = "value";

  TestFuture<int, std::string> future;

  RunLater(base::BindOnce(future.GetCallback(), expected_int_value,
                          expected_string_value));

  std::ignore = future.Get();

  EXPECT_EQ(expected_int_value, future.Get<0>());
  EXPECT_EQ(expected_string_value, future.Get<1>());
}

TEST_F(TestFutureTest, ShouldAllowAccessingTupleValueUsingGetWithType) {
  const int expected_int_value = 5;
  const std::string expected_string_value = "value";

  TestFuture<int, std::string> future;

  RunLater(base::BindOnce(future.GetCallback(), expected_int_value,
                          expected_string_value));

  std::ignore = future.Get();

  EXPECT_EQ(expected_int_value, future.Get<int>());
  EXPECT_EQ(expected_string_value, future.Get<std::string>());
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

TEST_F(TestFutureTest, ShouldSupportCvRefType) {
  std::string expected_value = "value";
  TestFuture<const std::string&> future;

  base::OnceCallback<void(const std::string&)> callback = future.GetCallback();
  std::move(callback).Run(expected_value);

  // both get and take should compile, and take should return the decayed value.
  const std::string& get_result = future.Get();
  EXPECT_EQ(expected_value, get_result);

  std::string take_result = future.Take();
  EXPECT_EQ(expected_value, take_result);
}

TEST_F(TestFutureTest, ShouldSupportMultipleCvRefTypes) {
  const int expected_first_value = 5;
  std::string expected_second_value = "value";
  const long expected_third_value = 10;
  TestFuture<const int, std::string&, const long&> future;

  base::OnceCallback<void(const int, std::string&, const long&)> callback =
      future.GetCallback();
  std::move(callback).Run(expected_first_value, expected_second_value,
                          expected_third_value);

  // both get and take should compile, and return the decayed value.
  const std::tuple<int, std::string, long>& get_result = future.Get();
  EXPECT_EQ(expected_first_value, std::get<0>(get_result));
  EXPECT_EQ(expected_second_value, std::get<1>(get_result));
  EXPECT_EQ(expected_third_value, std::get<2>(get_result));

  // Get<i> should also work
  EXPECT_EQ(expected_first_value, future.Get<0>());
  EXPECT_EQ(expected_second_value, future.Get<1>());
  EXPECT_EQ(expected_third_value, future.Get<2>());

  std::tuple<int, std::string, long> take_result = future.Take();
  EXPECT_EQ(expected_first_value, std::get<0>(take_result));
  EXPECT_EQ(expected_second_value, std::get<1>(take_result));
  EXPECT_EQ(expected_third_value, std::get<2>(take_result));
}

using TestFutureWithoutValuesTest = TestFutureTest;

TEST_F(TestFutureWithoutValuesTest, IsReadyShouldBeTrueWhenSetValueIsInvoked) {
  TestFuture<void> future;

  EXPECT_FALSE(future.IsReady());

  future.SetValue();

  EXPECT_TRUE(future.IsReady());
}

TEST_F(TestFutureWithoutValuesTest, WaitShouldUnblockWhenSetValueIsInvoked) {
  TestFuture<void> future;

  RunLater([&future]() { future.SetValue(); });

  ASSERT_FALSE(future.IsReady());
  std::ignore = future.Wait();
  EXPECT_TRUE(future.IsReady());
}

TEST_F(TestFutureWithoutValuesTest, WaitShouldUnblockWhenCallbackIsInvoked) {
  TestFuture<void> future;

  RunLater(future.GetCallback());

  ASSERT_FALSE(future.IsReady());
  std::ignore = future.Wait();
  EXPECT_TRUE(future.IsReady());
}

TEST_F(TestFutureWithoutValuesTest, GetShouldUnblockWhenCallbackIsInvoked) {
  TestFuture<void> future;

  RunLater(future.GetCallback());

  ASSERT_FALSE(future.IsReady());
  future.Get();
  EXPECT_TRUE(future.IsReady());
}

}  // namespace base::test
