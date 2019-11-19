// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/cancelable_callback.h"

#include <memory>

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/location.h"
#include "base/memory/ref_counted.h"
#include "base/run_loop.h"
#include "base/single_thread_task_runner.h"
#include "base/test/task_environment.h"
#include "base/threading/thread_task_runner_handle.h"
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
      base::BindRepeating(&Increment, base::Unretained(&count)));

  base::RepeatingClosure callback = cancelable.callback();
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
      base::BindRepeating(&Increment, base::Unretained(&count)));

  base::RepeatingClosure callback1 = cancelable.callback();
  base::RepeatingClosure callback2 = cancelable.callback();
  cancelable.Cancel();

  callback1.Run();
  EXPECT_EQ(0, count);

  callback2.Run();
  EXPECT_EQ(0, count);

  // Calling Cancel() again has no effect.
  cancelable.Cancel();

  // callback() of a cancelled callback is null.
  base::RepeatingClosure callback3 = cancelable.callback();
  EXPECT_TRUE(callback3.is_null());
}

// CancelableCallback destroyed before callback is run.
//  - Destruction of CancelableCallback cancels outstanding callbacks.
TEST(CancelableCallbackTest, CallbackCanceledOnDestruction) {
  int count = 0;
  base::RepeatingClosure callback;

  {
    CancelableRepeatingClosure cancelable(
        base::BindRepeating(&Increment, base::Unretained(&count)));

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

  CancelableOnceClosure cancelable(
      base::BindOnce(RefCountedParam, ref_counted));
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
      base::BindRepeating(&Increment, base::Unretained(&count)));

  base::RepeatingClosure callback = cancelable.callback();
  callback.Run();
  EXPECT_EQ(1, count);

  callback.Run();
  EXPECT_EQ(2, count);

  cancelable.Reset(
      base::BindRepeating(&IncrementBy, base::Unretained(&count), 3));
  EXPECT_FALSE(cancelable.IsCancelled());

  // The stale copy of the cancelable callback is non-null.
  ASSERT_FALSE(callback.is_null());

  // The stale copy of the cancelable callback is no longer active.
  callback.Run();
  EXPECT_EQ(2, count);

  base::RepeatingClosure callback2 = cancelable.callback();
  ASSERT_FALSE(callback2.is_null());

  callback2.Run();
  EXPECT_EQ(5, count);
}

// IsCanceled().
//  - Cancel() transforms the CancelableCallback into a cancelled state.
TEST(CancelableCallbackTest, IsNull) {
  CancelableOnceClosure cancelable;
  EXPECT_TRUE(cancelable.IsCancelled());

  int count = 0;
  cancelable.Reset(base::BindOnce(&Increment, base::Unretained(&count)));
  EXPECT_FALSE(cancelable.IsCancelled());

  cancelable.Cancel();
  EXPECT_TRUE(cancelable.IsCancelled());
}

// CancelableCallback posted to a task environment with PostTask.
//  - Posted callbacks can be cancelled.
TEST(CancelableCallbackTest, PostTask) {
  test::TaskEnvironment task_environment;

  int count = 0;
  CancelableRepeatingClosure cancelable(
      base::BindRepeating(&Increment, base::Unretained(&count)));

  ThreadTaskRunnerHandle::Get()->PostTask(FROM_HERE, cancelable.callback());
  RunLoop().RunUntilIdle();

  EXPECT_EQ(1, count);

  ThreadTaskRunnerHandle::Get()->PostTask(FROM_HERE, cancelable.callback());

  // Cancel before running the tasks.
  cancelable.Cancel();
  RunLoop().RunUntilIdle();

  // Callback never ran due to cancellation; count is the same.
  EXPECT_EQ(1, count);
}

// CancelableCallback can be used with move-only types.
TEST(CancelableCallbackTest, MoveOnlyType) {
  const int kExpectedResult = 42;

  int result = 0;
  CancelableRepeatingCallback<void(std::unique_ptr<int>)> cb(
      base::BindRepeating(&OnMoveOnlyReceived, base::Unretained(&result)));
  cb.callback().Run(std::make_unique<int>(kExpectedResult));

  EXPECT_EQ(kExpectedResult, result);
}

}  // namespace
}  // namespace base
