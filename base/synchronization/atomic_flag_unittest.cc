// Copyright 2011 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/synchronization/atomic_flag.h"

#include "base/check_op.h"
#include "base/functional/bind.h"
#include "base/synchronization/waitable_event.h"
#include "base/task/single_thread_task_runner.h"
#include "base/test/gtest_util.h"
#include "base/threading/platform_thread.h"
#include "base/threading/thread.h"
#include "build/build_config.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace base {

namespace {

void ExpectSetFlagDeath(AtomicFlag* flag) {
  ASSERT_TRUE(flag);
  EXPECT_DCHECK_DEATH(flag->Set());
}

// Busy waits (to explicitly avoid using synchronization constructs that would
// defeat the purpose of testing atomics) until |tested_flag| is set and then
// verifies that non-atomic |*expected_after_flag| is true and sets |*done_flag|
// before returning if it's non-null.
void BusyWaitUntilFlagIsSet(AtomicFlag* tested_flag, bool* expected_after_flag,
                            AtomicFlag* done_flag) {
  while (!tested_flag->IsSet())
    PlatformThread::YieldCurrentThread();

  EXPECT_TRUE(*expected_after_flag);
  if (done_flag)
    done_flag->Set();
}

}  // namespace

TEST(AtomicFlagTest, SimpleSingleThreadedTest) {
  AtomicFlag flag;
  ASSERT_FALSE(flag.IsSet());
  flag.Set();
  ASSERT_TRUE(flag.IsSet());
}

TEST(AtomicFlagTest, DoubleSetTest) {
  AtomicFlag flag;
  ASSERT_FALSE(flag.IsSet());
  flag.Set();
  ASSERT_TRUE(flag.IsSet());
  flag.Set();
  ASSERT_TRUE(flag.IsSet());
}

TEST(AtomicFlagTest, ReadFromDifferentThread) {
  // |tested_flag| is the one being tested below.
  AtomicFlag tested_flag;
  // |expected_after_flag| is used to confirm that sequential consistency is
  // obtained around |tested_flag|.
  bool expected_after_flag = false;
  // |reset_flag| is used to confirm the test flows as intended without using
  // synchronization constructs which would defeat the purpose of exercising
  // atomics.
  AtomicFlag reset_flag;

  Thread thread("AtomicFlagTest.ReadFromDifferentThread");
  ASSERT_TRUE(thread.Start());
  thread.task_runner()->PostTask(FROM_HERE,
                                 BindOnce(&BusyWaitUntilFlagIsSet, &tested_flag,
                                          &expected_after_flag, &reset_flag));

  // To verify that IsSet() fetches the flag's value from memory every time it
  // is called (not just the first time that it is called on a thread), sleep
  // before setting the flag.
  PlatformThread::Sleep(Milliseconds(20));

  // |expected_after_flag| is used to verify that all memory operations
  // performed before |tested_flag| is Set() are visible to threads that can see
  // IsSet().
  expected_after_flag = true;
  tested_flag.Set();

  // Sleep again to give the busy loop time to observe the flag and verify
  // expectations.
  PlatformThread::Sleep(Milliseconds(20));

  // Use |reset_flag| to confirm that the above completed (which the rest of
  // this test assumes).
  while (!reset_flag.IsSet())
    PlatformThread::YieldCurrentThread();

  tested_flag.UnsafeResetForTesting();
  EXPECT_FALSE(tested_flag.IsSet());
  expected_after_flag = false;

  // Perform the same test again after the controlled UnsafeResetForTesting(),
  // |thread| is guaranteed to be synchronized past the
  // |UnsafeResetForTesting()| call when the task runs per the implicit
  // synchronization in the post task mechanism.
  thread.task_runner()->PostTask(FROM_HERE,
                                 BindOnce(&BusyWaitUntilFlagIsSet, &tested_flag,
                                          &expected_after_flag, nullptr));

  PlatformThread::Sleep(Milliseconds(20));

  expected_after_flag = true;
  tested_flag.Set();

  // The |thread|'s destructor will block until the posted task completes, so
  // the test will time out if it fails to see the flag be set.
}

TEST(AtomicFlagTest, SetOnDifferentSequenceDeathTest) {
  // Checks that Set() can't be called from another sequence after being called
  // on this one. AtomicFlag should die on a DCHECK if Set() is called again
  // from another sequence.

  // Note: flag must be declared before the Thread so that its destructor runs
  // later. Otherwise there's a race between destructing flag and running
  // ExpectSetFlagDeath.
  AtomicFlag flag;

  GTEST_FLAG_SET(death_test_style, "threadsafe");
  Thread t("AtomicFlagTest.SetOnDifferentThreadDeathTest");
  ASSERT_TRUE(t.Start());
  EXPECT_TRUE(t.WaitUntilThreadStarted());

  flag.Set();
  t.task_runner()->PostTask(FROM_HERE, BindOnce(&ExpectSetFlagDeath, &flag));
}

}  // namespace base
