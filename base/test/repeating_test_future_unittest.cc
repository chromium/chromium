// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/test/repeating_test_future.h"

#include "base/task/single_thread_task_runner.h"
#include "base/test/bind.h"
#include "base/test/gtest_util.h"
#include "base/test/task_environment.h"
#include "testing/gtest/include/gtest/gtest-spi.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace base::test {

namespace {

struct MoveOnlyValue {
 public:
  MoveOnlyValue() = default;
  explicit MoveOnlyValue(std::string data) : data(std::move(data)) {}
  MoveOnlyValue(const MoveOnlyValue&) = delete;
  auto& operator=(const MoveOnlyValue&) = delete;
  MoveOnlyValue(MoveOnlyValue&&) = default;
  MoveOnlyValue& operator=(MoveOnlyValue&&) = default;
  ~MoveOnlyValue() = default;

  std::string data;
};

}  // namespace

class RepeatingTestFutureTest : public ::testing::Test {
 public:
  RepeatingTestFutureTest() = default;
  RepeatingTestFutureTest(const RepeatingTestFutureTest&) = delete;
  RepeatingTestFutureTest& operator=(const RepeatingTestFutureTest&) = delete;
  ~RepeatingTestFutureTest() override = default;

  void RunLater(OnceClosure callable) {
    SingleThreadTaskRunner::GetCurrentDefault()->PostTask(FROM_HERE,
                                                          std::move(callable));
  }

 private:
  test::SingleThreadTaskEnvironment environment_;
};

TEST_F(RepeatingTestFutureTest, ShouldBeEmptyInitially) {
  RepeatingTestFuture<std::string> future;

  EXPECT_TRUE(future.IsEmpty());
}

TEST_F(RepeatingTestFutureTest, ShouldNotBeEmptyAfterAddingAValue) {
  RepeatingTestFuture<std::string> future;

  future.AddValue("a value");

  EXPECT_FALSE(future.IsEmpty());
}

TEST_F(RepeatingTestFutureTest, ShouldBeEmptyAfterTakingTheOnlyElement) {
  RepeatingTestFuture<std::string> future;

  future.AddValue("a value");
  future.Take();

  EXPECT_TRUE(future.IsEmpty());
}

TEST_F(RepeatingTestFutureTest,
       ShouldNotBeEmptyIfTakingOneElementFromAfutureWith2Elements) {
  RepeatingTestFuture<std::string> future;

  future.AddValue("first value");
  future.AddValue("second value");
  future.Take();

  EXPECT_FALSE(future.IsEmpty());
}

TEST_F(RepeatingTestFutureTest, ShouldTakeElementsFiFo) {
  RepeatingTestFuture<std::string> future;

  future.AddValue("first value");
  future.AddValue("second value");

  EXPECT_EQ(future.Take(), "first value");
  EXPECT_EQ(future.Take(), "second value");
}

TEST_F(RepeatingTestFutureTest, WaitShouldBlockUntilElementArrives) {
  RepeatingTestFuture<std::string> future;

  RunLater(BindLambdaForTesting([&future] { future.AddValue("a value"); }));
  EXPECT_TRUE(future.IsEmpty());

  EXPECT_TRUE(future.Wait());

  EXPECT_FALSE(future.IsEmpty());
}

TEST_F(RepeatingTestFutureTest, WaitShouldReturnTrueWhenValueArrives) {
  RepeatingTestFuture<std::string> future;

  RunLater(BindLambdaForTesting([&future] { future.AddValue("a value"); }));

  EXPECT_TRUE(future.Wait());

  EXPECT_FALSE(future.IsEmpty());
}

TEST_F(RepeatingTestFutureTest,
       WaitShouldReturnTrueImmediatelyWhenValueIsAlreadyPresent) {
  RepeatingTestFuture<std::string> future;

  future.AddValue("value already present");

  EXPECT_TRUE(future.Wait());
}

TEST_F(RepeatingTestFutureTest, WaitShouldReturnFalseIfTimeoutHappens) {
  test::ScopedRunLoopTimeout timeout(FROM_HERE, Milliseconds(1));

  // `ScopedRunLoopTimeout` will automatically fail the test when a timeout
  // happens, so we use EXPECT_NONFATAL_FAILURE to handle this failure.
  // EXPECT_NONFATAL_FAILURE only works on static objects.
  static bool success;
  static RepeatingTestFuture<std::string> future;

  EXPECT_NONFATAL_FAILURE({ success = future.Wait(); }, "timed out");

  EXPECT_FALSE(success);
}

TEST_F(RepeatingTestFutureTest, TakeShouldBlockUntilAnElementArrives) {
  RepeatingTestFuture<std::string> future;

  RunLater(BindLambdaForTesting(
      [&future] { future.AddValue("value pushed delayed"); }));

  EXPECT_EQ(future.Take(), "value pushed delayed");
}

TEST_F(RepeatingTestFutureTest, TakeShouldDcheckIfTimeoutHappens) {
  test::ScopedRunLoopTimeout timeout(FROM_HERE, Milliseconds(1));

  RepeatingTestFuture<std::string> future;

  EXPECT_DCHECK_DEATH_WITH(future.Take(), "timed out");
}

TEST_F(RepeatingTestFutureTest, TakeShouldWorkWithMoveOnlyValue) {
  RepeatingTestFuture<MoveOnlyValue> future;

  RunLater(BindLambdaForTesting(
      [&future] { future.AddValue(MoveOnlyValue("move only value")); }));

  MoveOnlyValue result = future.Take();

  EXPECT_EQ(result.data, "move only value");
}

TEST_F(RepeatingTestFutureTest, ShouldStoreValuePassedToCallback) {
  RepeatingTestFuture<std::string> future;

  RunLater(BindOnce(future.GetCallback(), "value"));

  EXPECT_EQ("value", future.Take());
}

TEST_F(RepeatingTestFutureTest, ShouldAllowInvokingCallbackMultipleTimes) {
  RepeatingTestFuture<std::string> future;

  RunLater(BindLambdaForTesting([callback = future.GetCallback()]() {
    callback.Run("first value");
    callback.Run("second value");
    callback.Run("third value");
  }));

  EXPECT_EQ("first value", future.Take());
  EXPECT_EQ("second value", future.Take());
  EXPECT_EQ("third value", future.Take());
}

TEST_F(RepeatingTestFutureTest, ShouldAllowReferenceArgumentsForCallback) {
  RepeatingTestFuture<std::string> future;

  RepeatingCallback<void(const std::string&)> callback =
      future.GetCallback<const std::string&>();
  RunLater(BindOnce(std::move(callback), "expected value"));

  EXPECT_EQ("expected value", future.Take());
}

TEST_F(RepeatingTestFutureTest, ShouldStoreMultipleValuesInATuple) {
  const int expected_int_value = 5;
  const std::string expected_string_value = "value";

  RepeatingTestFuture<int, std::string> future;

  RunLater(BindLambdaForTesting(
      [&] { future.AddValue(expected_int_value, expected_string_value); }));

  std::tuple<int, std::string> actual = future.Take();
  EXPECT_EQ(expected_int_value, std::get<0>(actual));
  EXPECT_EQ(expected_string_value, std::get<1>(actual));
}

TEST_F(RepeatingTestFutureTest, ShouldAllowCallbackWithMultipleValues) {
  const int expected_int_value = 5;
  const std::string expected_string_value = "value";

  RepeatingTestFuture<int, std::string> future;

  RunLater(BindOnce(future.GetCallback(), expected_int_value,
                    expected_string_value));

  std::tuple<int, std::string> actual = future.Take();
  EXPECT_EQ(expected_int_value, std::get<0>(actual));
  EXPECT_EQ(expected_string_value, std::get<1>(actual));
}

TEST_F(RepeatingTestFutureTest,
       ShouldAllowCallbackWithMultipleReferenceValues) {
  const int expected_int_value = 5;
  const std::string expected_string_value = "value";

  RepeatingTestFuture<int, std::string> future;

  RepeatingCallback<void(const int&, std::string)> callback =
      future.GetCallback<const int&, std::string>();
  RunLater(
      BindOnce(std::move(callback), expected_int_value, expected_string_value));

  std::tuple<int, std::string> actual = future.Take();
  EXPECT_EQ(expected_int_value, std::get<0>(actual));
  EXPECT_EQ(expected_string_value, std::get<1>(actual));
}

TEST_F(RepeatingTestFutureTest, ShouldSupportCvRefType) {
  std::string expected_value = "value";
  RepeatingTestFuture<const std::string&> future;

  base::OnceCallback<void(const std::string&)> callback = future.GetCallback();
  std::move(callback).Run(expected_value);

  std::string actual = future.Take();
  EXPECT_EQ(expected_value, actual);
}

TEST_F(RepeatingTestFutureTest, ShouldSupportMultipleCvRefType) {
  const int expected_first_value = 5;
  std::string expected_second_value = "value";
  const long expected_third_value = 10;
  RepeatingTestFuture<const int, std::string&, const long&> future;

  base::OnceCallback<void(const int, std::string&, const long&)> callback =
      future.GetCallback();
  std::move(callback).Run(expected_first_value, expected_second_value,
                          expected_third_value);

  std::tuple<int, std::string, long> take_result = future.Take();
  EXPECT_EQ(expected_first_value, std::get<0>(take_result));
  EXPECT_EQ(expected_second_value, std::get<1>(take_result));
  EXPECT_EQ(expected_third_value, std::get<2>(take_result));
}

}  // namespace base::test
