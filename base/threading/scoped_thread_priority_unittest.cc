// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/threading/scoped_thread_priority.h"

#include "base/threading/platform_thread.h"
#include "build/build_config.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace base {

namespace {

// Tests in this file invoke an API that tracks state in static variable. They
// can therefore only be invoked once per process.
#define ASSERT_RUNS_ONCE()                                              \
  static int num_times_run = 0;                                         \
  ++num_times_run;                                                      \
  if (num_times_run > 1)                                                \
    ADD_FAILURE() << "This test cannot run multiple times in the same " \
                     "process.";

class ScopedThreadPriorityTest : public testing::Test {
 protected:
  void SetUp() override {
    // Ensures the default thread priority is set.
    ASSERT_EQ(ThreadPriority::NORMAL,
              PlatformThread::GetCurrentThreadPriority());
  }
};

#if defined(OS_WIN)
void FunctionThatBoostsPriorityOnFirstInvoke(ThreadPriority expected_priority) {
  SCOPED_MAY_LOAD_LIBRARY_AT_BACKGROUND_PRIORITY();
  EXPECT_EQ(expected_priority, PlatformThread::GetCurrentThreadPriority());
}

void FunctionThatBoostsPriorityOnEveryInvoke() {
  SCOPED_MAY_LOAD_LIBRARY_AT_BACKGROUND_PRIORITY_REPEATEDLY();
  EXPECT_EQ(base::ThreadPriority::NORMAL,
            PlatformThread::GetCurrentThreadPriority());
}

#endif  // OS_WIN

}  // namespace

TEST_F(ScopedThreadPriorityTest, WithoutPriorityBoost) {
  ASSERT_RUNS_ONCE();

  // Validates that a thread at normal priority keep the same priority.
  {
    SCOPED_MAY_LOAD_LIBRARY_AT_BACKGROUND_PRIORITY();
    EXPECT_EQ(ThreadPriority::NORMAL,
              PlatformThread::GetCurrentThreadPriority());
  }
  EXPECT_EQ(ThreadPriority::NORMAL, PlatformThread::GetCurrentThreadPriority());
}

#if defined(OS_WIN)
TEST_F(ScopedThreadPriorityTest, WithPriorityBoost) {
  ASSERT_RUNS_ONCE();

  // Validates that a thread at background priority is boosted to normal
  // priority.
  PlatformThread::SetCurrentThreadPriority(ThreadPriority::BACKGROUND);
  {
    SCOPED_MAY_LOAD_LIBRARY_AT_BACKGROUND_PRIORITY();
    EXPECT_EQ(ThreadPriority::NORMAL,
              PlatformThread::GetCurrentThreadPriority());
  }
  EXPECT_EQ(ThreadPriority::BACKGROUND,
            PlatformThread::GetCurrentThreadPriority());

  // Put back the default thread priority.
  PlatformThread::SetCurrentThreadPriority(ThreadPriority::NORMAL);
}
#endif  // OS_WIN

#if defined(OS_WIN)
TEST_F(ScopedThreadPriorityTest, NestedScope) {
  ASSERT_RUNS_ONCE();

  PlatformThread::SetCurrentThreadPriority(ThreadPriority::BACKGROUND);

  {
    SCOPED_MAY_LOAD_LIBRARY_AT_BACKGROUND_PRIORITY();
    EXPECT_EQ(ThreadPriority::NORMAL,
              PlatformThread::GetCurrentThreadPriority());
    {
      SCOPED_MAY_LOAD_LIBRARY_AT_BACKGROUND_PRIORITY();
      EXPECT_EQ(ThreadPriority::NORMAL,
                PlatformThread::GetCurrentThreadPriority());
    }
    EXPECT_EQ(ThreadPriority::NORMAL,
              PlatformThread::GetCurrentThreadPriority());
  }

  EXPECT_EQ(ThreadPriority::BACKGROUND,
            PlatformThread::GetCurrentThreadPriority());

  // Put back the default thread priority.
  PlatformThread::SetCurrentThreadPriority(ThreadPriority::NORMAL);
}
#endif  // OS_WIN

#if defined(OS_WIN)
TEST_F(ScopedThreadPriorityTest, FunctionThatBoostsPriorityOnFirstInvoke) {
  ASSERT_RUNS_ONCE();

  PlatformThread::SetCurrentThreadPriority(ThreadPriority::BACKGROUND);

  FunctionThatBoostsPriorityOnFirstInvoke(base::ThreadPriority::NORMAL);
  FunctionThatBoostsPriorityOnFirstInvoke(base::ThreadPriority::BACKGROUND);

  // Put back the default thread priority.
  PlatformThread::SetCurrentThreadPriority(ThreadPriority::NORMAL);
}

TEST_F(ScopedThreadPriorityTest, FunctionThatBoostsPriorityOnEveryInvoke) {
  PlatformThread::SetCurrentThreadPriority(ThreadPriority::BACKGROUND);

  FunctionThatBoostsPriorityOnEveryInvoke();
  FunctionThatBoostsPriorityOnEveryInvoke();

  // Put back the default thread priority.
  PlatformThread::SetCurrentThreadPriority(ThreadPriority::NORMAL);
}

#endif  // OS_WIN

}  // namespace base
