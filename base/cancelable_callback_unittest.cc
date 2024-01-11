// Copyright 2011 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/cancelable_callback.h"

#include <memory>
#include <optional>

#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "base/location.h"
#include "base/memory/ref_counted.h"
#include "base/run_loop.h"
#include "base/task/single_thread_task_runner.h"
#include "base/test/task_environment.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace base {
namespace {

class TestRefCounted : public RefCountedThreadSafe<TestRefCounted> {
 private:
  friend class RefCountedThreadSafe<TestRefCounted>;
  ~TestRefCounted() = default;
};

void Increment(int* count) { (*count)++; }
void IncrementBy(int* count, int n) { (*count) += n; }
void RefCountedParam(const scoped_refptr<TestRefCounted>& ref_counted) {}

void OnMoveOnlyReceived(int* value, std::unique_ptr<int> result) {
  *value = *result;
}

// Cancel().
//  - Callback can be run multiple times.
//  - After Cancel(), Run() completes but has no effect.
TEST(CancelableCallbackTest, Cancel) {
  int count = 0;
  CancelableRepeatingClosure cancelable(
      BindRepeating(&Increment, Unretained(&count)));

  RepeatingClosure callback = cancelable.callback();
  callback.Run();
  EXPECT_EQ(1, count);

  callback.Run();
  EXPECT_EQ(2, count);

  cancelable.Cancel();
  callback.Run();
  EXPECT_EQ(2, count);
}

// Cancel() called multiple times.
//  - Cancel() cancels all copies of the wrapped callback.
//  - Calling Cancel() more than once has no effect.
//  - After Cancel(), callback() returns a null callback.
TEST(CancelableCallbackTest, MultipleCancel) {
  int count = 0;
  CancelableRepeatingClosure cancelable(
      BindRepeating(&Increment, Unretained(&count)));

  RepeatingClosure callback1 = cancelable.callback();
  RepeatingClosure callback2 = cancelable.callback();
  cancelable.Cancel();

  callback1.Run();
  EXPECT_EQ(0, count);

  callback2.Run();
  EXPECT_EQ(0, count);

  // Calling Cancel() again has no effect.
  cancelable.Cancel();

  // callback() of a cancelled callback is null.
  RepeatingClosure callback3 = cancelable.callback();
  EXPECT_TRUE(callback3.is_null());
}

// CancelableRepeatingCallback destroyed before callback is run.
//  - Destruction of CancelableRepeatingCallback cancels outstanding callbacks.
TEST(CancelableCallbackTest, CallbackCanceledOnDestruction) {
  int count = 0;
  RepeatingClosure callback;

  {
    CancelableRepeatingClosure cancelable(
        BindRepeating(&Increment, Unretained(&count)));

    callback = cancelable.callback();
    callback.Run();
    EXPECT_EQ(1, count);
  }

  callback.Run();
  EXPECT_EQ(1, count);
}

// Cancel() called on bound closure with a RefCounted parameter.
//  - Cancel drops wrapped callback (and, implicitly, its bound arguments).
TEST(CancelableCallbackTest, CancelDropsCallback) {
  scoped_refptr<TestRefCounted> ref_counted = new TestRefCounted;
  EXPECT_TRUE(ref_counted->HasOneRef());

  CancelableOnceClosure cancelable(BindOnce(RefCountedParam, ref_counted));
  EXPECT_FALSE(cancelable.IsCancelled());
  EXPECT_TRUE(ref_counted.get());
  EXPECT_FALSE(ref_counted->HasOneRef());

  // There is only one reference to |ref_counted| after the Cancel().
  cancelable.Cancel();
  EXPECT_TRUE(cancelable.IsCancelled());
  EXPECT_TRUE(ref_counted.get());
  EXPECT_TRUE(ref_counted->HasOneRef());
}

// Reset().
//  - Reset() replaces the existing wrapped callback with a new callback.
//  - Reset() deactivates outstanding callbacks.
TEST(CancelableCallbackTest, Reset) {
  int count = 0;
  CancelableRepeatingClosure cancelable(
      BindRepeating(&Increment, Unretained(&count)));

  RepeatingClosure callback = cancelable.callback();
  callback.Run();
  EXPECT_EQ(1, count);

  callback.Run();
  EXPECT_EQ(2, count);

  cancelable.Reset(BindRepeating(&IncrementBy, Unretained(&count), 3));
  EXPECT_FALSE(cancelable.IsCancelled());

  // The stale copy of the cancelable callback is non-null.
  ASSERT_FALSE(callback.is_null());

  // The stale copy of the cancelable callback is no longer active.
  callback.Run();
  EXPECT_EQ(2, count);

  RepeatingClosure callback2 = cancelable.callback();
  ASSERT_FALSE(callback2.is_null());

  callback2.Run();
  EXPECT_EQ(5, count);
}

// IsCanceled().
//  - Cancel() transforms the CancelableOnceCallback into a cancelled state.
TEST(CancelableCallbackTest, IsNull) {
  CancelableOnceClosure cancelable;
  EXPECT_TRUE(cancelable.IsCancelled());

  int count = 0;
  cancelable.Reset(BindOnce(&Increment, Unretained(&count)));
  EXPECT_FALSE(cancelable.IsCancelled());

  cancelable.Cancel();
  EXPECT_TRUE(cancelable.IsCancelled());
}

// CancelableRepeatingCallback posted to a task environment with PostTask.
//  - Posted callbacks can be cancelled.
//  - Chained callbacks from `.Then()` still run on cancelled callbacks.
TEST(CancelableCallbackTest, PostTask) {
  test::SingleThreadTaskEnvironment task_environment;

  int count = 0;
  CancelableRepeatingClosure cancelable(
      BindRepeating(&Increment, Unretained(&count)));

  SingleThreadTaskRunner::GetCurrentDefault()->PostTask(FROM_HERE,
                                                        cancelable.callback());
  RunLoop().RunUntilIdle();

  EXPECT_EQ(1, count);

  SingleThreadTaskRunner::GetCurrentDefault()->PostTask(FROM_HERE,
                                                        cancelable.callback());

  // Cancel before running the task.
  cancelable.Cancel();
  RunLoop().RunUntilIdle();

  // Callback never ran due to cancellation; count is the same.
  EXPECT_EQ(1, count);

  // Chain a callback to the cancelable callback.
  cancelable.Reset(BindRepeating(&Increment, Unretained(&count)));
  SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE, cancelable.callback().Then(
                     BindRepeating(&IncrementBy, Unretained(&count), 2)));

  // Cancel before running the task.
  cancelable.Cancel();
  RunLoop().RunUntilIdle();

  // Callback never ran due to cancellation, but chained callback still should
  // have. Count should increase by exactly two.
  EXPECT_EQ(3, count);
}

// CancelableRepeatingCallback posted to a task environment with
// PostTaskAndReply.
//  - Posted callbacks can be cancelled.
TEST(CancelableCallbackTest, PostTaskAndReply) {
  std::optional<test::SingleThreadTaskEnvironment> task_environment;
  task_environment.emplace();

  int count = 0;
  CancelableRepeatingClosure cancelable_reply(
      BindRepeating(&Increment, Unretained(&count)));

  std::optional<RunLoop> loop;
  loop.emplace();
  SingleThreadTaskRunner::GetCurrentDefault()->PostTaskAndReply(
      FROM_HERE, DoNothing(),
      cancelable_reply.callback().Then(loop->QuitClosure()));
  loop->Run();

  EXPECT_EQ(1, count);

  loop.emplace();
  SingleThreadTaskRunner::GetCurrentDefault()->PostTaskAndReply(
      FROM_HERE, DoNothing(),
      cancelable_reply.callback().Then(loop->QuitClosure()));

  // Cancel before running the tasks.
  cancelable_reply.Cancel();
  loop->Run();

  // Callback never ran due to cancellation; count is the same. Note that
  // QuitClosure() is still invoked because chained callbacks via Then() get
  // invoked even if the first callback is cancelled.
  EXPECT_EQ(1, count);

  // Post it again to exercise a shutdown-like scenario.
  cancelable_reply.Reset(BindRepeating(&Increment, Unretained(&count)));

  SingleThreadTaskRunner::GetCurrentDefault()->PostTaskAndReply(
      FROM_HERE, DoNothing(), cancelable_reply.callback());
  task_environment.reset();

  // Callback never ran due to task runner shutdown; count is the same.
  EXPECT_EQ(1, count);
}

// CancelableRepeatingCallback can be used with move-only types.
TEST(CancelableCallbackTest, MoveOnlyType) {
  const int kExpectedResult = 42;

  int result = 0;
  CancelableRepeatingCallback<void(std::unique_ptr<int>)> cb(
      BindRepeating(&OnMoveOnlyReceived, Unretained(&result)));
  cb.callback().Run(std::make_unique<int>(kExpectedResult));

  EXPECT_EQ(kExpectedResult, result);
}

}  // namespace
}  // namespace base
