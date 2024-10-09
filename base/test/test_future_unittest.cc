// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/test/test_future.h"

#include <tuple>

#include "base/functional/callback.h"
#include "base/run_loop.h"
#include "base/task/sequenced_task_runner.h"
#include "base/task/thread_pool.h"
#include "base/test/bind.h"
#include "base/test/gtest_util.h"
#include "base/test/mock_callback.h"
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
  void RunLater(Lambda lambda,
                scoped_refptr<SequencedTaskRunner> task_runner =
                    SequencedTaskRunner::GetCurrentDefault()) {
    task_runner->PostTask(FROM_HERE, BindLambdaForTesting(lambda));
  }

  void RunLater(OnceClosure callable,
                scoped_refptr<SequencedTaskRunner> task_runner =
                    SequencedTaskRunner::GetCurrentDefault()) {
    task_runner->PostTask(FROM_HERE, std::move(callable));
  }

  void RunLater(RepeatingClosure callable,
                scoped_refptr<SequencedTaskRunner> task_runner =
                    SequencedTaskRunner::GetCurrentDefault()) {
    task_runner->PostTask(FROM_HERE, std::move(callable));
  }

  void PostDelayedTask(OnceClosure callable,
                       base::TimeDelta delay,
                       scoped_refptr<SequencedTaskRunner> task_runner =
                           SequencedTaskRunner::GetCurrentDefault()) {
    task_runner->PostDelayedTask(FROM_HERE, std::move(callable), delay);
  }

 private:
  TaskEnvironment environment_{TaskEnvironment::TimeSource::MOCK_TIME};
};

using TestFutureDeathTest = TestFutureTest;

TEST_F(TestFutureTest, WaitShouldBlockUntilValueArrives) {
  const int expected_value = 42;
  TestFuture<int> future;

  PostDelayedTask(BindOnce(future.GetCallback(), expected_value),
                  Milliseconds(1));

  std::ignore = future.Wait();

  EXPECT_EQ(expected_value, future.Get());
}

TEST_F(TestFutureTest, WaitShouldBlockUntilValueArrivesOnOtherSequence) {
  const int expected_value = 42;
  TestFuture<int> future;

  PostDelayedTask(BindOnce(future.GetSequenceBoundCallback(), expected_value),
                  Milliseconds(1), ThreadPool::CreateSequencedTaskRunner({}));

  std::ignore = future.Wait();

  EXPECT_EQ(expected_value, future.Get());
}

TEST_F(TestFutureTest, WaitShouldReturnTrueWhenValueArrives) {
  TestFuture<int> future;

  PostDelayedTask(BindOnce(future.GetCallback(), kAnyValue), Milliseconds(1));

  bool success = future.Wait();
  EXPECT_TRUE(success);
}

TEST_F(TestFutureTest, WaitShouldReturnTrueWhenValueArrivesOnOtherSequence) {
  TestFuture<int> future;

  PostDelayedTask(BindOnce(future.GetSequenceBoundCallback(), kAnyValue),
                  Milliseconds(1), ThreadPool::CreateSequencedTaskRunner({}));

  bool success = future.Wait();
  EXPECT_TRUE(success);
}

TEST_F(TestFutureTest, WaitShouldReturnFalseIfTimeoutHappens) {
  ScopedRunLoopTimeout timeout(FROM_HERE, Milliseconds(1));

  // `ScopedRunLoopTimeout` will automatically fail the test when a timeout
  // happens, so we use EXPECT_FATAL_FAILURE to handle this failure.
  // EXPECT_FATAL_FAILURE only works on static objects.
  static bool success;
  static TestFuture<AnyType> future;

  EXPECT_NONFATAL_FAILURE({ success = future.Wait(); }, "timed out");

  EXPECT_FALSE(success);
}

TEST_F(TestFutureTest, GetShouldBlockUntilValueArrives) {
  const int expected_value = 42;
  TestFuture<int> future;

  PostDelayedTask(BindOnce(future.GetCallback(), expected_value),
                  Milliseconds(1));

  int actual_value = future.Get();

  EXPECT_EQ(expected_value, actual_value);
}

TEST_F(TestFutureTest, GetShouldBlockUntilValueArrivesOnOtherSequence) {
  const int expected_value = 42;
  TestFuture<int> future;

  PostDelayedTask(BindOnce(future.GetSequenceBoundCallback(), expected_value),
                  Milliseconds(1), ThreadPool::CreateSequencedTaskRunner({}));

  int actual_value = future.Get();

  EXPECT_EQ(expected_value, actual_value);
}

TEST_F(TestFutureDeathTest, GetShouldCheckIfTimeoutHappens) {
  ScopedRunLoopTimeout timeout(FROM_HERE, Milliseconds(1));

  TestFuture<AnyType> future;

  EXPECT_CHECK_DEATH_WITH(std::ignore = future.Get(), "timed out");
}

TEST_F(TestFutureTest, TakeShouldWorkWithMoveOnlyValue) {
  const int expected_data = 99;
  TestFuture<MoveOnlyValue> future;

  RunLater(BindOnce(future.GetCallback(), MoveOnlyValue(expected_data)));

  MoveOnlyValue actual_value = future.Take();

  EXPECT_EQ(expected_data, actual_value.data);
}

TEST_F(TestFutureTest, TakeShouldWorkWithMoveOnlyValueOnOtherSequence) {
  const int expected_data = 99;
  TestFuture<MoveOnlyValue> future;

  RunLater(
      BindOnce(future.GetSequenceBoundCallback(), MoveOnlyValue(expected_data)),
      ThreadPool::CreateSequencedTaskRunner({}));

  MoveOnlyValue actual_value = future.Take();

  EXPECT_EQ(expected_data, actual_value.data);
}

TEST_F(TestFutureDeathTest, TakeShouldCheckIfTimeoutHappens) {
  ScopedRunLoopTimeout timeout(FROM_HERE, Milliseconds(1));

  TestFuture<AnyType> future;

  EXPECT_CHECK_DEATH_WITH(std::ignore = future.Take(), "timed out");
}

TEST_F(TestFutureTest, IsReadyShouldBeTrueWhenValueIsSet) {
  TestFuture<AnyType> future;

  EXPECT_FALSE(future.IsReady());

  future.SetValue(kAnyValue);

  EXPECT_TRUE(future.IsReady());
}

TEST_F(TestFutureTest, ClearShouldRemoveStoredValue) {
  TestFuture<AnyType> future;

  future.SetValue(kAnyValue);

  future.Clear();

  EXPECT_FALSE(future.IsReady());
}

TEST_F(TestFutureTest, ShouldNotAllowOverwritingStoredValue) {
  TestFuture<AnyType> future;

  future.SetValue(kAnyValue);

  EXPECT_NONFATAL_FAILURE(future.SetValue(kOtherValue), "Received new value");
}

TEST_F(TestFutureTest, ShouldAllowReuseIfPreviousValueIsFirstConsumed) {
  TestFuture<std::string> future;

  RunLater([&] { future.SetValue("first value"); });
  EXPECT_EQ(future.Take(), "first value");

  ASSERT_FALSE(future.IsReady());

  RunLater([&] { future.SetValue("second value"); });
  EXPECT_EQ(future.Take(), "second value");
}

TEST_F(TestFutureTest, ShouldAllowReusingCallback) {
  TestFuture<std::string> future;

  RepeatingCallback<void(std::string)> callback = future.GetRepeatingCallback();

  RunLater(BindOnce(callback, "first value"));
  EXPECT_EQ(future.Take(), "first value");

  RunLater(BindOnce(callback, "second value"));
  EXPECT_EQ(future.Take(), "second value");

  RepeatingCallback<void(std::string)> sequence_bound_callback =
      future.GetSequenceBoundRepeatingCallback();
  auto other_task_runner = ThreadPool::CreateSequencedTaskRunner({});

  RunLater(BindOnce(sequence_bound_callback, "third value"), other_task_runner);
  EXPECT_EQ(future.Take(), "third value");

  RunLater(BindOnce(sequence_bound_callback, "fourth value"),
           other_task_runner);
  EXPECT_EQ(future.Take(), "fourth value");
}

TEST_F(TestFutureTest, WaitShouldWorkAfterTake) {
  TestFuture<std::string> future;

  future.SetValue("first value");
  std::ignore = future.Take();

  RunLater([&] { future.SetValue("second value"); });

  EXPECT_TRUE(future.Wait());
  EXPECT_EQ(future.Get(), "second value");
}

TEST_F(TestFutureTest, ShouldSignalWhenSetValueIsInvoked) {
  const int expected_value = 111;
  TestFuture<int> future;

  RunLater([&future] { future.SetValue(expected_value); });

  int actual_value = future.Get();

  EXPECT_EQ(expected_value, actual_value);
}

TEST_F(TestFutureTest, ShouldAllowReferenceArgumentsForCallback) {
  const int expected_value = 222;
  TestFuture<int> future;

  OnceCallback<void(const int&)> callback = future.GetCallback<const int&>();
  RunLater(BindOnce(std::move(callback), expected_value));

  int actual_value = future.Get();

  EXPECT_EQ(expected_value, actual_value);
}

TEST_F(TestFutureTest,
       ShouldAllowReferenceArgumentsForCallbackOnOtherSequence) {
  const int expected_value = 222;
  TestFuture<int> future;

  OnceCallback<void(const int&)> callback =
      future.GetSequenceBoundCallback<const int&>();
  RunLater(BindOnce(std::move(callback), expected_value),
           ThreadPool::CreateSequencedTaskRunner({}));

  int actual_value = future.Get();

  EXPECT_EQ(expected_value, actual_value);
}

TEST_F(TestFutureTest, ShouldAllowInvokingCallbackAfterFutureIsDestroyed) {
  OnceCallback<void(int)> callback;

  {
    TestFuture<int> future;
    callback = future.GetCallback();
  }

  std::move(callback).Run(1);
}

TEST_F(TestFutureTest,
       ShouldAllowInvokingCallbackOnOtherSequenceAfterFutureIsDestroyed) {
  OnceCallback<void(int)> callback;

  {
    TestFuture<int> future;
    callback = future.GetSequenceBoundCallback();
  }

  base::RunLoop run_loop;
  ThreadPool::PostTask(
      FROM_HERE, BindOnce(std::move(callback), 1).Then(run_loop.QuitClosure()));
  run_loop.Run();
}

TEST_F(TestFutureTest, ShouldReturnTupleValue) {
  const int expected_int_value = 5;
  const std::string expected_string_value = "value";

  TestFuture<int, std::string> future;

  RunLater(BindOnce(future.GetCallback(), expected_int_value,
                    expected_string_value));

  const std::tuple<int, std::string>& actual = future.Get();

  EXPECT_EQ(expected_int_value, std::get<0>(actual));
  EXPECT_EQ(expected_string_value, std::get<1>(actual));
}

TEST_F(TestFutureTest, ShouldReturnTupleValueOnOtherSequence) {
  const int expected_int_value = 5;
  const std::string expected_string_value = "value";

  TestFuture<int, std::string> future;

  RunLater(BindOnce(future.GetSequenceBoundCallback(), expected_int_value,
                    expected_string_value),
           ThreadPool::CreateSequencedTaskRunner({}));

  const std::tuple<int, std::string>& actual = future.Get();

  EXPECT_EQ(expected_int_value, std::get<0>(actual));
  EXPECT_EQ(expected_string_value, std::get<1>(actual));
}

TEST_F(TestFutureTest, ShouldAllowAccessingTupleValueUsingGetWithIndex) {
  const int expected_int_value = 5;
  const std::string expected_string_value = "value";

  TestFuture<int, std::string> future;

  RunLater(BindOnce(future.GetCallback(), expected_int_value,
                    expected_string_value));

  std::ignore = future.Get();

  EXPECT_EQ(expected_int_value, future.Get<0>());
  EXPECT_EQ(expected_string_value, future.Get<1>());
}

TEST_F(TestFutureTest, ShouldAllowAccessingTupleValueUsingGetWithType) {
  const int expected_int_value = 5;
  const std::string expected_string_value = "value";

  TestFuture<int, std::string> future;

  RunLater(BindOnce(future.GetCallback(), expected_int_value,
                    expected_string_value));

  std::ignore = future.Get();

  EXPECT_EQ(expected_int_value, future.Get<int>());
  EXPECT_EQ(expected_string_value, future.Get<std::string>());
}

TEST_F(TestFutureTest, ShouldAllowReferenceArgumentsForMultiArgumentCallback) {
  const int expected_int_value = 5;
  const std::string expected_string_value = "value";

  TestFuture<int, std::string> future;

  OnceCallback<void(int, const std::string&)> callback =
      future.GetCallback<int, const std::string&>();
  RunLater(
      BindOnce(std::move(callback), expected_int_value, expected_string_value));

  std::tuple<int, std::string> actual = future.Get();

  EXPECT_EQ(expected_int_value, std::get<0>(actual));
  EXPECT_EQ(expected_string_value, std::get<1>(actual));
}

TEST_F(TestFutureTest,
       ShouldAllowReferenceArgumentsForMultiArgumentCallbackOnOtherSequence) {
  const int expected_int_value = 5;
  const std::string expected_string_value = "value";

  TestFuture<int, std::string> future;

  OnceCallback<void(int, const std::string&)> callback =
      future.GetSequenceBoundCallback<int, const std::string&>();
  RunLater(
      BindOnce(std::move(callback), expected_int_value, expected_string_value),
      ThreadPool::CreateSequencedTaskRunner({}));

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

  OnceCallback<void(const std::string&)> callback = future.GetCallback();
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

  OnceCallback<void(const int, std::string&, const long&)> callback =
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

TEST_F(TestFutureTest, ShouldAllowReuseIfPreviousTupleValueIsFirstConsumed) {
  TestFuture<std::string, int> future;

  future.SetValue("first value", 1);
  std::ignore = future.Take();

  ASSERT_FALSE(future.IsReady());

  future.SetValue("second value", 2);
  EXPECT_EQ(future.Take(), std::make_tuple("second value", 2));
}

TEST_F(TestFutureTest, ShouldPrintCurrentValueIfItIsOverwritten) {
  using UnprintableValue = MoveOnlyValue;

  TestFuture<const char*, int, UnprintableValue> future;

  future.SetValue("first-value", 1111, UnprintableValue());

  EXPECT_NONFATAL_FAILURE(
      future.SetValue("second-value", 2222, UnprintableValue()),
      "old value <first-value, 1111, [4-byte object at 0x");
}

TEST_F(TestFutureTest, ShouldPrintNewValueIfItOverwritesOldValue) {
  using UnprintableValue = MoveOnlyValue;

  TestFuture<const char*, int, UnprintableValue> future;

  future.SetValue("first-value", 1111, UnprintableValue());

  EXPECT_NONFATAL_FAILURE(
      future.SetValue("second-value", 2222, UnprintableValue()),
      "new value <second-value, 2222, [4-byte object at 0x");
}

TEST_F(TestFutureTest, InvokeFutureSingleValue) {
  TestFuture<int> future;

  MockCallback<OnceCallback<void(int)>> cb;

  EXPECT_CALL(cb, Run).WillOnce(InvokeFuture(future));

  RunLater(BindOnce(cb.Get(), 7));

  EXPECT_EQ(7, future.Take());
}

TEST_F(TestFutureTest, InvokeFutureMoveOnlyValue) {
  TestFuture<MoveOnlyValue> future;

  MockCallback<OnceCallback<void(MoveOnlyValue)>> cb;

  EXPECT_CALL(cb, Run).WillOnce(InvokeFuture(future));

  RunLater(BindOnce(cb.Get(), MoveOnlyValue(10)));

  EXPECT_EQ(10, future.Take().data);
}

TEST_F(TestFutureTest, InvokeFutureMultipleValues) {
  TestFuture<int, std::string> future;

  MockCallback<OnceCallback<void(int, std::string)>> cb;

  EXPECT_CALL(cb, Run).WillOnce(InvokeFuture(future));

  RunLater(BindOnce(cb.Get(), 19, "Nineteen"));

  EXPECT_THAT(future.Take(), std::tuple(19, "Nineteen"));
}

TEST_F(TestFutureTest, InvokeFutureMultipleTimes) {
  TestFuture<std::string> future;

  MockCallback<RepeatingCallback<void(std::string)>> cb;

  EXPECT_CALL(cb, Run).WillRepeatedly(InvokeFuture(future));

  cb.Get().Run("first time");
  EXPECT_EQ("first time", future.Take());

  cb.Get().Run("second time");
  EXPECT_EQ("second time", future.Take());
}

TEST_F(TestFutureTest, InvokeFutureDestroyedFuture) {
  std::optional<TestFuture<int>> maybe_future;
  maybe_future.emplace();

  MockCallback<OnceCallback<void(int)>> cb;

  EXPECT_CALL(cb, Run).WillOnce(InvokeFuture(*maybe_future));

  maybe_future = std::nullopt;

  // If this doesn't crash it worked.
  cb.Get().Run(42);
}

TEST_F(TestFutureDeathTest, CallbackShouldDcheckOnOtherSequence) {
  TestFuture<int> future;

  // Sequence-bound callback may run any time between RunLater() and Wait(),
  // should succeed.
  auto other_task_runner = ThreadPool::CreateSequencedTaskRunner({});
  RunLater(BindOnce(future.GetSequenceBoundCallback(), 1), other_task_runner);
  EXPECT_TRUE(future.Wait());

  future.Clear();

  // Callback may run any time between RunLater() and Wait(), should DCHECK.
  EXPECT_DCHECK_DEATH_WITH(
      {
        RunLater(BindOnce(future.GetCallback(), 2), other_task_runner);
        EXPECT_TRUE(future.Wait());
      },
      "CalledOnValidSequence");
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

  RunLater([&future] { future.SetValue(); });

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

TEST_F(TestFutureWithoutValuesTest,
       WaitShouldUnblockWhenCallbackIsInvokedOnOtherSequence) {
  TestFuture<void> future;

  RunLater(future.GetSequenceBoundCallback(),
           ThreadPool::CreateSequencedTaskRunner({}));

  ASSERT_FALSE(future.IsReady());
  std::ignore = future.Wait();
  EXPECT_TRUE(future.IsReady());
}

TEST_F(TestFutureWithoutValuesTest, WaitAndClearShouldAllowFutureReusing) {
  TestFuture<void> future;

  RunLater(future.GetCallback());
  EXPECT_TRUE(future.WaitAndClear());

  ASSERT_FALSE(future.IsReady());

  RunLater(future.GetCallback());
  EXPECT_TRUE(future.Wait());

  auto other_task_runner = ThreadPool::CreateSequencedTaskRunner({});
  RunLater(future.GetSequenceBoundCallback(), other_task_runner);
  EXPECT_TRUE(future.WaitAndClear());

  ASSERT_FALSE(future.IsReady());

  RunLater(future.GetSequenceBoundCallback(), other_task_runner);
  EXPECT_TRUE(future.Wait());
}

TEST_F(TestFutureWithoutValuesTest, GetShouldUnblockWhenCallbackIsInvoked) {
  TestFuture<void> future;

  RunLater(future.GetCallback());

  ASSERT_FALSE(future.IsReady());
  future.Get();
  EXPECT_TRUE(future.IsReady());
}

TEST_F(TestFutureWithoutValuesTest,
       GetShouldUnblockWhenCallbackIsInvokedOnOtherSequence) {
  TestFuture<void> future;

  RunLater(future.GetSequenceBoundCallback(),
           ThreadPool::CreateSequencedTaskRunner({}));

  ASSERT_FALSE(future.IsReady());
  future.Get();
  EXPECT_TRUE(future.IsReady());
}

TEST(TestFutureWithoutTaskEnvironmentTest,
     CanCreateTestFutureBeforeTaskEnvironment) {
  TestFuture<AnyType> future;

  // If we come here the test passes, since it means we can create a
  // `TestFuture` without having a `TaskEnvironment`.
}

TEST(TestFutureWithoutTaskEnvironmentDeathTest,
     WaitShouldDcheckWithoutTaskEnvironment) {
  TestFuture<AnyType> future;

  EXPECT_CHECK_DEATH_WITH((void)future.Wait(),
                          "requires a single-threaded context");
}

TEST_F(TestFutureWithoutValuesTest, InvokeFuture) {
  TestFuture<void> future;

  MockCallback<OnceClosure> cb;

  EXPECT_CALL(cb, Run).WillOnce(InvokeFuture(future));

  RunLater(cb.Get());

  EXPECT_TRUE(future.Wait());
}

}  // namespace base::test
