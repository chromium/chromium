// Copyright 2019 The Chromium Authors. All rights reserved.
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

static ThreadPriority kAllThreadTypes[] = {
    ThreadPriority::REALTIME_AUDIO, ThreadPriority::DISPLAY,
    ThreadPriority::NORMAL, ThreadPriority::BACKGROUND};

static_assert(static_cast<int>(ThreadPriority::BACKGROUND) == 0,
              "kBackground isn't lowest");
static_assert(ThreadPriority::REALTIME_AUDIO == ThreadPriority::kMaxValue,
              "kRealtimeAudio isn't highest");

class ScopedThreadPriorityTest : public testing::Test {
 protected:
  void SetUp() override {
    // Ensures the default thread priority is set.
    ASSERT_EQ(ThreadPriority::NORMAL,
              PlatformThread::GetCurrentThreadPriority());
  }
};

#if BUILDFLAG(IS_WIN)
void FunctionThatBoostsPriorityOnFirstInvoke(ThreadPriority expected_priority) {
  SCOPED_MAY_LOAD_LIBRARY_AT_BACKGROUND_PRIORITY();
  EXPECT_EQ(expected_priority, PlatformThread::GetCurrentThreadPriority());
}

void FunctionThatBoostsPriorityOnEveryInvoke() {
  SCOPED_MAY_LOAD_LIBRARY_AT_BACKGROUND_PRIORITY_REPEATEDLY();
  EXPECT_EQ(base::ThreadPriority::NORMAL,
            PlatformThread::GetCurrentThreadPriority());
}

#endif  // BUILDFLAG(IS_WIN)

}  // namespace

TEST_F(ScopedThreadPriorityTest, BasicTest) {
  for (auto from : kAllThreadTypes) {
    if (!PlatformThread::CanChangeThreadPriority(ThreadPriority::NORMAL, from))
      continue;
    for (auto to : kAllThreadTypes) {
      // ThreadType::kRealtimeAudio is not a valid |target_thread_type| for
      // ScopedBoostPriority.
      if (to == ThreadPriority::REALTIME_AUDIO)
        continue;
      Thread::Options options;
      options.priority = from;
      Thread thread("ScopedThreadPriorityTest");
      thread.StartWithOptions(std::move(options));
      thread.WaitUntilThreadStarted();
      thread.task_runner()->PostTask(
          FROM_HERE,
          BindOnce(
              [](ThreadPriority from, ThreadPriority to) {
                EXPECT_EQ(PlatformThread::GetCurrentThreadPriority(), from);
                {
                  ScopedBoostPriority scoped_boost_priority(to);
                  bool will_boost_priority =
                      from < to &&
                      PlatformThread::CanChangeThreadPriority(from, to) &&
                      PlatformThread::CanChangeThreadPriority(to, from);
                  EXPECT_EQ(PlatformThread::GetCurrentThreadPriority(),
                            will_boost_priority ? to : from);
                }
                EXPECT_EQ(PlatformThread::GetCurrentThreadPriority(), from);
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
    EXPECT_EQ(ThreadPriority::NORMAL,
              PlatformThread::GetCurrentThreadPriority());
  }
  EXPECT_EQ(ThreadPriority::NORMAL, PlatformThread::GetCurrentThreadPriority());
}

#if BUILDFLAG(IS_WIN)
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
#endif  // BUILDFLAG(IS_WIN)

#if BUILDFLAG(IS_WIN)
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
#endif  // BUILDFLAG(IS_WIN)

#if BUILDFLAG(IS_WIN)
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

#endif  // BUILDFLAG(IS_WIN)

}  // namespace base
