// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/threading/scoped_thread_priority.h"

#include "base/test/bind.h"
#include "base/test/gtest_util.h"
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
    ASSERT_EQ(ThreadType::kDefault,
              PlatformThread::GetCurrentEffectiveThreadTypeForTest());
  }
};

using ScopedThreadPriorityDeathTest = ScopedThreadPriorityTest;

#if BUILDFLAG(IS_WIN)
void FunctionThatBoostsPriorityOnFirstInvoke(ThreadType expected_priority) {
  SCOPED_MAY_LOAD_LIBRARY_AT_BACKGROUND_PRIORITY();
  EXPECT_EQ(expected_priority,
            PlatformThread::GetCurrentEffectiveThreadTypeForTest());
}

void FunctionThatBoostsPriorityOnEveryInvoke() {
  SCOPED_MAY_LOAD_LIBRARY_AT_BACKGROUND_PRIORITY_REPEATEDLY();
  EXPECT_EQ(base::ThreadType::kDefault,
            PlatformThread::GetCurrentEffectiveThreadTypeForTest());
}

#endif  // BUILDFLAG(IS_WIN)

}  // namespace

TEST_F(ScopedThreadPriorityTest, BasicTest) {
  for (auto from : kAllThreadTypes) {
    if (!PlatformThread::CanChangeThreadType(ThreadType::kDefault, from)) {
      continue;
    }
    for (auto to : kAllThreadTypes) {
      // ThreadType::kRealtimeAudio is not a valid |target_thread_type| for
      // ScopedBoostPriority.
      if (to == ThreadType::kRealtimeAudio) {
        continue;
      }
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

void TestPriorityResultingFromBoost(ThreadType initial_thread_type,
                                    ThreadType target_thread_type) {
  Thread thread("ScopedThreadPriorityTest");
  thread.StartWithOptions(Thread::Options(initial_thread_type));
  thread.WaitUntilThreadStarted();

  WaitableEvent thread_ready;
  WaitableEvent thread_boosted;
  raw_ptr<ScopedBoostablePriority> scoped_boostable_priority_ptr;

  bool will_boost_priority =
#if BUILDFLAG(IS_LINUX)
      // Linux doesn't support priority boosting.
      false;
#else
      initial_thread_type < target_thread_type &&
      PlatformThread::CanChangeThreadType(initial_thread_type,
                                          target_thread_type) &&
      PlatformThread::CanChangeThreadType(target_thread_type,
                                          initial_thread_type);
#endif

  thread.task_runner()->PostTask(
      FROM_HERE, BindLambdaForTesting([&]() {
        EXPECT_EQ(PlatformThread::GetCurrentThreadType(), initial_thread_type);

        {
          ScopedBoostablePriority scoped_boostable_priority;
          scoped_boostable_priority_ptr = &scoped_boostable_priority;
          thread_ready.Signal();
          thread_boosted.Wait();
          scoped_boostable_priority_ptr = nullptr;

#if BUILDFLAG(IS_WIN) || BUILDFLAG(IS_ANDROID)
          // Apple priority boost doesn't reflect in the effective ThreadType.
          if (will_boost_priority) {
            EXPECT_EQ(PlatformThread::GetCurrentEffectiveThreadTypeForTest(),
                      target_thread_type);
          }
#endif
        }
        EXPECT_EQ(PlatformThread::GetCurrentThreadType(), initial_thread_type);
        EXPECT_EQ(PlatformThread::GetCurrentEffectiveThreadTypeForTest(),
                  initial_thread_type);
      }));

  thread_ready.Wait();
  bool did_boost_priority =
      scoped_boostable_priority_ptr->BoostPriority(target_thread_type);
  EXPECT_EQ(did_boost_priority, will_boost_priority);
  thread_boosted.Signal();

  thread.FlushForTesting();
}

TEST_F(ScopedThreadPriorityTest, BoostableTest) {
  TestPriorityResultingFromBoost(ThreadType::kBackground, ThreadType::kUtility);
  TestPriorityResultingFromBoost(ThreadType::kBackground, ThreadType::kDefault);
  TestPriorityResultingFromBoost(ThreadType::kBackground,
                                 ThreadType::kDisplayCritical);

  TestPriorityResultingFromBoost(ThreadType::kUtility, ThreadType::kDefault);
  TestPriorityResultingFromBoost(ThreadType::kUtility,
                                 ThreadType::kDisplayCritical);

  TestPriorityResultingFromBoost(ThreadType::kDefault,
                                 ThreadType::kDisplayCritical);
}

TEST_F(ScopedThreadPriorityDeathTest, NoRealTime) {
  EXPECT_CHECK_DEATH({
    ScopedBoostPriority scoped_boost_priority(ThreadType::kRealtimeAudio);
  });
}

TEST_F(ScopedThreadPriorityTest, WithoutPriorityBoost) {
  ASSERT_RUNS_ONCE();

  // Validates that a thread at normal priority keep the same priority.
  {
    SCOPED_MAY_LOAD_LIBRARY_AT_BACKGROUND_PRIORITY();
    EXPECT_EQ(ThreadType::kDefault,
              PlatformThread::GetCurrentEffectiveThreadTypeForTest());
  }
  EXPECT_EQ(ThreadType::kDefault,
            PlatformThread::GetCurrentEffectiveThreadTypeForTest());
}

#if BUILDFLAG(IS_WIN)
TEST_F(ScopedThreadPriorityTest, WithPriorityBoost) {
  ASSERT_RUNS_ONCE();

  // Validates that a thread at background priority is boosted to normal
  // priority.
  PlatformThread::SetCurrentThreadType(ThreadType::kBackground);
  {
    SCOPED_MAY_LOAD_LIBRARY_AT_BACKGROUND_PRIORITY();
    EXPECT_EQ(ThreadType::kDefault,
              PlatformThread::GetCurrentEffectiveThreadTypeForTest());
  }
  EXPECT_EQ(ThreadType::kBackground,
            PlatformThread::GetCurrentEffectiveThreadTypeForTest());

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
    EXPECT_EQ(ThreadType::kDefault,
              PlatformThread::GetCurrentEffectiveThreadTypeForTest());
    {
      SCOPED_MAY_LOAD_LIBRARY_AT_BACKGROUND_PRIORITY();
      EXPECT_EQ(ThreadType::kDefault,
                PlatformThread::GetCurrentEffectiveThreadTypeForTest());
    }
    EXPECT_EQ(ThreadType::kDefault,
              PlatformThread::GetCurrentEffectiveThreadTypeForTest());
  }

  EXPECT_EQ(ThreadType::kBackground,
            PlatformThread::GetCurrentEffectiveThreadTypeForTest());

  // Put back the default thread priority.
  PlatformThread::SetCurrentThreadType(ThreadType::kDefault);
}
#endif  // BUILDFLAG(IS_WIN)

#if BUILDFLAG(IS_WIN)
TEST_F(ScopedThreadPriorityTest, FunctionThatBoostsPriorityOnFirstInvoke) {
  ASSERT_RUNS_ONCE();

  PlatformThread::SetCurrentThreadType(ThreadType::kBackground);

  FunctionThatBoostsPriorityOnFirstInvoke(base::ThreadType::kDefault);
  FunctionThatBoostsPriorityOnFirstInvoke(base::ThreadType::kBackground);

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

TEST_F(ScopedThreadPriorityTest, TaskMonitoringBoost) {
  ASSERT_EQ(ThreadType::kDefault,
            PlatformThread::GetCurrentEffectiveThreadTypeForTest());

  {
    // A `TaskMonitoringScopedBoostPriority` object with a callback that always
    // returns true.
    TaskMonitoringScopedBoostPriority scoped_boost_priority(
        ThreadType::kInteractive, BindRepeating([]() { return true; }));
    // Not boosted before `WillProcessTask` is called.
    ASSERT_EQ(ThreadType::kDefault,
              PlatformThread::GetCurrentEffectiveThreadTypeForTest());

    // After `WillProcessTask` is called, the thread priority should be boosted.
    scoped_boost_priority.WillProcessTask(PendingTask(), false);
    ASSERT_EQ(ThreadType::kInteractive,
              PlatformThread::GetCurrentEffectiveThreadTypeForTest());
  }

  // Back to normal outside the scope.
  ASSERT_EQ(ThreadType::kDefault,
            PlatformThread::GetCurrentEffectiveThreadTypeForTest());
}

#endif  // BUILDFLAG(IS_WIN)

}  // namespace base
