// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/threading/scoped_thread_priority.h"

#include "base/threading/platform_thread.h"
#include "base/threading/thread.h"
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

static ThreadType kAllThreadTypes[] = {
    ThreadType::kRealtimeAudio, ThreadType::kDisplayCritical,
    ThreadType::kDefault, ThreadType::kBackground};

static_assert(static_cast<int>(ThreadType::kBackground) == 0,
              "kBackground isn't lowest");
static_assert(ThreadType::kRealtimeAudio == ThreadType::kMaxValue,
              "kRealtimeAudio isn't highest");

class ScopedThreadPriorityTest : public testing::Test {
 protected:
  void SetUp() override {
    // Ensures the default thread priority is set.
    PlatformThread::SetCurrentThreadType(ThreadType::kDefault);
    ASSERT_EQ(ThreadPriorityForTest::kNormal,
              PlatformThread::GetCurrentThreadPriorityForTest());
  }
};

#if BUILDFLAG(IS_WIN)
void FunctionThatBoostsPriorityOnFirstInvoke(
    ThreadPriorityForTest expected_priority) {
  SCOPED_MAY_LOAD_LIBRARY_AT_BACKGROUND_PRIORITY();
  EXPECT_EQ(expected_priority,
            PlatformThread::GetCurrentThreadPriorityForTest());
}

void FunctionThatBoostsPriorityOnEveryInvoke() {
  SCOPED_MAY_LOAD_LIBRARY_AT_BACKGROUND_PRIORITY_REPEATEDLY();
  EXPECT_EQ(base::ThreadPriorityForTest::kNormal,
            PlatformThread::GetCurrentThreadPriorityForTest());
}

#endif  // BUILDFLAG(IS_WIN)

}  // namespace

TEST_F(ScopedThreadPriorityTest, BasicTest) {
  for (auto from : kAllThreadTypes) {
    if (!PlatformThread::CanChangeThreadType(ThreadType::kDefault, from))
      continue;
    for (auto to : kAllThreadTypes) {
      // ThreadType::kRealtimeAudio is not a valid |target_thread_type| for
      // ScopedBoostPriority.
      if (to == ThreadType::kRealtimeAudio)
        continue;
      Thread thread("ScopedThreadPriorityTest");
      thread.StartWithOptions(Thread::Options(from));
      thread.WaitUntilThreadStarted();
      thread.task_runner()->PostTask(
          FROM_HERE,
          BindOnce(
              [](ThreadType from, ThreadType to) {
                EXPECT_EQ(PlatformThread::GetCurrentThreadType(), from);
                {
                  ScopedBoostPriority scoped_boost_priority(to);
                  bool will_boost_priority =
                      from < to &&
                      PlatformThread::CanChangeThreadType(from, to) &&
                      PlatformThread::CanChangeThreadType(to, from);
                  EXPECT_EQ(PlatformThread::GetCurrentThreadType(),
                            will_boost_priority ? to : from);
                }
                EXPECT_EQ(PlatformThread::GetCurrentThreadType(), from);
              },
              from, to));
    }
  }
}

TEST_F(ScopedThreadPriorityTest, WithoutPriorityBoost) {
  ASSERT_RUNS_ONCE();

  // Validates that a thread at normal priority keep the same priority.
  {
    SCOPED_MAY_LOAD_LIBRARY_AT_BACKGROUND_PRIORITY();
    EXPECT_EQ(ThreadPriorityForTest::kNormal,
              PlatformThread::GetCurrentThreadPriorityForTest());
  }
  EXPECT_EQ(ThreadPriorityForTest::kNormal,
            PlatformThread::GetCurrentThreadPriorityForTest());
}

#if BUILDFLAG(IS_WIN)
TEST_F(ScopedThreadPriorityTest, WithPriorityBoost) {
  ASSERT_RUNS_ONCE();

  // Validates that a thread at background priority is boosted to normal
  // priority.
  PlatformThread::SetCurrentThreadType(ThreadType::kBackground);
  {
    SCOPED_MAY_LOAD_LIBRARY_AT_BACKGROUND_PRIORITY();
    EXPECT_EQ(ThreadPriorityForTest::kNormal,
              PlatformThread::GetCurrentThreadPriorityForTest());
  }
  EXPECT_EQ(ThreadPriorityForTest::kBackground,
            PlatformThread::GetCurrentThreadPriorityForTest());

  // Put back the default thread priority.
  PlatformThread::SetCurrentThreadType(ThreadType::kDefault);
}
#endif  // BUILDFLAG(IS_WIN)

#if BUILDFLAG(IS_WIN)
TEST_F(ScopedThreadPriorityTest, NestedScope) {
  ASSERT_RUNS_ONCE();

  PlatformThread::SetCurrentThreadType(ThreadType::kBackground);

  {
    SCOPED_MAY_LOAD_LIBRARY_AT_BACKGROUND_PRIORITY();
    EXPECT_EQ(ThreadPriorityForTest::kNormal,
              PlatformThread::GetCurrentThreadPriorityForTest());
    {
      SCOPED_MAY_LOAD_LIBRARY_AT_BACKGROUND_PRIORITY();
      EXPECT_EQ(ThreadPriorityForTest::kNormal,
                PlatformThread::GetCurrentThreadPriorityForTest());
    }
    EXPECT_EQ(ThreadPriorityForTest::kNormal,
              PlatformThread::GetCurrentThreadPriorityForTest());
  }

  EXPECT_EQ(ThreadPriorityForTest::kBackground,
            PlatformThread::GetCurrentThreadPriorityForTest());

  // Put back the default thread priority.
  PlatformThread::SetCurrentThreadType(ThreadType::kDefault);
}
#endif  // BUILDFLAG(IS_WIN)

#if BUILDFLAG(IS_WIN)
TEST_F(ScopedThreadPriorityTest, FunctionThatBoostsPriorityOnFirstInvoke) {
  ASSERT_RUNS_ONCE();

  PlatformThread::SetCurrentThreadType(ThreadType::kBackground);

  FunctionThatBoostsPriorityOnFirstInvoke(base::ThreadPriorityForTest::kNormal);
  FunctionThatBoostsPriorityOnFirstInvoke(
      base::ThreadPriorityForTest::kBackground);

  // Put back the default thread priority.
  PlatformThread::SetCurrentThreadType(ThreadType::kDefault);
}

TEST_F(ScopedThreadPriorityTest, FunctionThatBoostsPriorityOnEveryInvoke) {
  PlatformThread::SetCurrentThreadType(ThreadType::kBackground);

  FunctionThatBoostsPriorityOnEveryInvoke();
  FunctionThatBoostsPriorityOnEveryInvoke();

  // Put back the default thread priority.
  PlatformThread::SetCurrentThreadType(ThreadType::kDefault);
}

#endif  // BUILDFLAG(IS_WIN)

}  // namespace base
