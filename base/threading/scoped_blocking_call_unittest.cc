// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/threading/scoped_blocking_call.h"

#include <memory>
#include <utility>
#include <vector>

#include "base/barrier_closure.h"
#include "base/bind.h"
#include "base/callback.h"
#include "base/macros.h"
#include "base/task/thread_pool.h"
#include "base/task/thread_pool/environment_config.h"
#include "base/task/thread_pool/thread_pool_impl.h"
#include "base/test/bind.h"
#include "base/test/gtest_util.h"
#include "base/test/task_environment.h"
#include "base/test/test_waitable_event.h"
#include "base/threading/scoped_blocking_call_internal.h"
#include "base/threading/thread_restrictions.h"
#include "base/time/time_override.h"
#include "build/build_config.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using testing::ElementsAre;

namespace base {

namespace {

class MockBlockingObserver : public internal::BlockingObserver {
 public:
  MockBlockingObserver() = default;

  MOCK_METHOD1(BlockingStarted, void(BlockingType));
  MOCK_METHOD0(BlockingTypeUpgraded, void());
  MOCK_METHOD0(BlockingEnded, void());

 private:
  DISALLOW_COPY_AND_ASSIGN(MockBlockingObserver);
};

class ScopedBlockingCallTest : public testing::Test {
 protected:
  ScopedBlockingCallTest() {
    internal::SetBlockingObserverForCurrentThread(&observer_);
  }

  ~ScopedBlockingCallTest() override {
    internal::ClearBlockingObserverForCurrentThread();
  }

  testing::StrictMock<MockBlockingObserver> observer_;

 private:
  DISALLOW_COPY_AND_ASSIGN(ScopedBlockingCallTest);
};

}  // namespace

TEST_F(ScopedBlockingCallTest, MayBlock) {
  EXPECT_CALL(observer_, BlockingStarted(BlockingType::MAY_BLOCK));
  ScopedBlockingCall scoped_blocking_call(FROM_HERE, BlockingType::MAY_BLOCK);
  testing::Mock::VerifyAndClear(&observer_);
  EXPECT_CALL(observer_, BlockingEnded());
}

TEST_F(ScopedBlockingCallTest, WillBlock) {
  EXPECT_CALL(observer_, BlockingStarted(BlockingType::WILL_BLOCK));
  ScopedBlockingCall scoped_blocking_call(FROM_HERE, BlockingType::WILL_BLOCK);
  testing::Mock::VerifyAndClear(&observer_);
  EXPECT_CALL(observer_, BlockingEnded());
}

TEST_F(ScopedBlockingCallTest, MayBlockWillBlock) {
  EXPECT_CALL(observer_, BlockingStarted(BlockingType::MAY_BLOCK));
  ScopedBlockingCall scoped_blocking_call_a(FROM_HERE, BlockingType::MAY_BLOCK);
  testing::Mock::VerifyAndClear(&observer_);

  {
    EXPECT_CALL(observer_, BlockingTypeUpgraded());
    ScopedBlockingCall scoped_blocking_call_b(FROM_HERE,
                                              BlockingType::WILL_BLOCK);
    testing::Mock::VerifyAndClear(&observer_);
  }

  EXPECT_CALL(observer_, BlockingEnded());
}

TEST_F(ScopedBlockingCallTest, WillBlockMayBlock) {
  EXPECT_CALL(observer_, BlockingStarted(BlockingType::WILL_BLOCK));
  ScopedBlockingCall scoped_blocking_call_a(FROM_HERE,
                                            BlockingType::WILL_BLOCK);
  testing::Mock::VerifyAndClear(&observer_);

  {
    ScopedBlockingCall scoped_blocking_call_b(FROM_HERE,
                                              BlockingType::MAY_BLOCK);
  }

  EXPECT_CALL(observer_, BlockingEnded());
}

TEST_F(ScopedBlockingCallTest, MayBlockMayBlock) {
  EXPECT_CALL(observer_, BlockingStarted(BlockingType::MAY_BLOCK));
  ScopedBlockingCall scoped_blocking_call_a(FROM_HERE, BlockingType::MAY_BLOCK);
  testing::Mock::VerifyAndClear(&observer_);

  {
    ScopedBlockingCall scoped_blocking_call_b(FROM_HERE,
                                              BlockingType::MAY_BLOCK);
  }

  EXPECT_CALL(observer_, BlockingEnded());
}

TEST_F(ScopedBlockingCallTest, WillBlockWillBlock) {
  EXPECT_CALL(observer_, BlockingStarted(BlockingType::WILL_BLOCK));
  ScopedBlockingCall scoped_blocking_call_a(FROM_HERE,
                                            BlockingType::WILL_BLOCK);
  testing::Mock::VerifyAndClear(&observer_);

  {
    ScopedBlockingCall scoped_blocking_call_b(FROM_HERE,
                                              BlockingType::WILL_BLOCK);
  }

  EXPECT_CALL(observer_, BlockingEnded());
}

TEST_F(ScopedBlockingCallTest, MayBlockWillBlockTwice) {
  EXPECT_CALL(observer_, BlockingStarted(BlockingType::MAY_BLOCK));
  ScopedBlockingCall scoped_blocking_call_a(FROM_HERE, BlockingType::MAY_BLOCK);
  testing::Mock::VerifyAndClear(&observer_);

  {
    EXPECT_CALL(observer_, BlockingTypeUpgraded());
    ScopedBlockingCall scoped_blocking_call_b(FROM_HERE,
                                              BlockingType::WILL_BLOCK);
    testing::Mock::VerifyAndClear(&observer_);

    {
      ScopedBlockingCall scoped_blocking_call_c(FROM_HERE,
                                                BlockingType::MAY_BLOCK);
      ScopedBlockingCall scoped_blocking_call_d(FROM_HERE,
                                                BlockingType::WILL_BLOCK);
    }
  }

  EXPECT_CALL(observer_, BlockingEnded());
}

TEST(ScopedBlockingCallDestructionOrderTest, InvalidDestructionOrder) {
  auto scoped_blocking_call_a =
      std::make_unique<ScopedBlockingCall>(FROM_HERE, BlockingType::WILL_BLOCK);
  auto scoped_blocking_call_b =
      std::make_unique<ScopedBlockingCall>(FROM_HERE, BlockingType::WILL_BLOCK);

  EXPECT_DCHECK_DEATH({ scoped_blocking_call_a.reset(); });
}

class ScopedBlockingCallIOJankMonitoringTest : public testing::Test {
 public:
  explicit ScopedBlockingCallIOJankMonitoringTest(
      test::TaskEnvironment::TimeSource time_source =
          test::TaskEnvironment::TimeSource::MOCK_TIME)
      : task_environment_(time_source) {}

  void SetUp() override {
    // Note 1: While EnableIOJankMonitoringForProcess() is documented as being
    // only callable once per process. The call to CancelMonitoringForTesting()
    // in TearDown() makes it okay to call this in multiple tests in a row
    // within a single process.
    // Note 2: No need to check TimeTicks::IsConsistentAcrossProcesses() in
    // spite of EnableIOJankMonitoringForProcess()'s requirement as
    // TimeSource::MOCK_TIME avoids usage of the system clock and avoids the
    // issue.
    EnableIOJankMonitoringForProcess(BindLambdaForTesting(
        [&](int janky_intervals_per_minute, int total_janks_per_minute) {
          reports_.push_back(
              {janky_intervals_per_minute, total_janks_per_minute});
        }));
  }

  void TearDown() override {
    internal::IOJankMonitoringWindow::CancelMonitoringForTesting();
  }

 protected:
  // A member initialized before |task_environment_| that forces worker threads
  // to be started synchronously. This avoids a tricky race where Linux invokes
  // SetCurrentThreadPriority() from early main, before invoking ThreadMain and
  // yielding control to the thread pool impl. That causes a ScopedBlockingCall
  // in platform_thread_linux.cc:SetThreadCgroupForThreadPriority and interferes
  // with this test. This solution is quite intrusive but is the simplest we can
  // do for this unique corner case.
  struct SetSynchronousThreadStart {
    SetSynchronousThreadStart() {
      internal::ThreadPoolImpl::SetSynchronousThreadStartForTesting(true);
    }
    ~SetSynchronousThreadStart() {
      internal::ThreadPoolImpl::SetSynchronousThreadStartForTesting(false);
    }
  } set_synchronous_thread_start_;

  // The registered lambda above may report to this from any thread. It is
  // nonetheless safe to read this from the test body as
  // TaskEnvironment+MOCK_TIME advances the test in lock steps.
  std::vector<std::pair<int, int>> reports_;

  test::TaskEnvironment task_environment_;
};

// Manually mocks time to be able to move it backwards. Uses inheritance to
// ensure the clock override outlives the entire lifetime of the main test
// fixture (or weird things happen on destruction).
class ScopedBlockingCallIOJankMonitoringManualMockTimeTest
    : public subtle::ScopedTimeClockOverrides,
      public ScopedBlockingCallIOJankMonitoringTest {
 public:
  // Initialized to Now() when the test starts and manually controlled from
  // there.
  static TimeTicks reported_ticks_;

  ScopedBlockingCallIOJankMonitoringManualMockTimeTest()
      : subtle::ScopedTimeClockOverrides(
            nullptr,
            []() { return reported_ticks_; },
            nullptr),
        ScopedBlockingCallIOJankMonitoringTest(
            test::TaskEnvironment::TimeSource::SYSTEM_TIME) {
    reported_ticks_ = TimeTicks::Now();
  }
};

// static
TimeTicks ScopedBlockingCallIOJankMonitoringManualMockTimeTest::reported_ticks_;

TEST_F(ScopedBlockingCallIOJankMonitoringTest, Basic) {
  constexpr auto kJankTiming =
      internal::IOJankMonitoringWindow::kIOJankInterval * 7;
  {
    ScopedBlockingCall blocked_for_7s(FROM_HERE, BlockingType::MAY_BLOCK);
    task_environment_.FastForwardBy(kJankTiming);
  }

  // No janks reported before the monitoring window completes.
  EXPECT_THAT(reports_, ElementsAre());

  // Advance precisely to the end of this window.
  task_environment_.FastForwardBy(
      internal::IOJankMonitoringWindow::kMonitoringWindow - kJankTiming);

  EXPECT_THAT(reports_, ElementsAre(std::make_pair(7, 7)));
}

TEST_F(ScopedBlockingCallIOJankMonitoringTest, NestedDoesntMatter) {
  constexpr auto kJankTiming =
      internal::IOJankMonitoringWindow::kIOJankInterval * 7;
  {
    ScopedBlockingCall blocked_for_7s(FROM_HERE, BlockingType::MAY_BLOCK);
    ScopedBlockingCall nested(FROM_HERE, BlockingType::MAY_BLOCK);
    task_environment_.FastForwardBy(kJankTiming);
  }

  // No janks reported before the monitoring window completes.
  EXPECT_THAT(reports_, ElementsAre());

  // Jump to the next window.
  task_environment_.FastForwardBy(
      internal::IOJankMonitoringWindow::kMonitoringWindow);

  EXPECT_THAT(reports_, ElementsAre(std::make_pair(7, 7)));
}

TEST_F(ScopedBlockingCallIOJankMonitoringTest, ManyInAWindow) {
  constexpr auto kJankTiming =
      internal::IOJankMonitoringWindow::kIOJankInterval * 7;
  constexpr auto kIdleTiming = TimeDelta::FromSeconds(3);

  for (int i = 0; i < 3; ++i) {
    {
      ScopedBlockingCall blocked_for_7s(FROM_HERE, BlockingType::MAY_BLOCK);
      task_environment_.FastForwardBy(kJankTiming);
    }
    task_environment_.FastForwardBy(kIdleTiming);
  }

  // No janks reported before the monitoring window completes.
  EXPECT_THAT(reports_, ElementsAre());

  // Complete the current window.
  task_environment_.FastForwardBy(
      internal::IOJankMonitoringWindow::kMonitoringWindow -
      (kJankTiming + kIdleTiming) * 3);

  EXPECT_THAT(reports_, ElementsAre(std::make_pair(7 * 3, 7 * 3)));
}

TEST_F(ScopedBlockingCallIOJankMonitoringTest, OverlappingMultipleWindows) {
  constexpr auto kJankTiming =
      internal::IOJankMonitoringWindow::kMonitoringWindow * 3 +
      internal::IOJankMonitoringWindow::kIOJankInterval * 5;

  {
    ScopedBlockingCall blocked_for_3windows(FROM_HERE, BlockingType::MAY_BLOCK);
    task_environment_.FastForwardBy(kJankTiming);
  }

  // Fast-forward by another window with no active blocking calls.
  task_environment_.FastForwardBy(
      internal::IOJankMonitoringWindow::kMonitoringWindow);

  // 3 windows janky for their full breadth and 1 window janky for 5 seconds.
  EXPECT_THAT(reports_,
              ElementsAre(std::make_pair(60, 60), std::make_pair(60, 60),
                          std::make_pair(60, 60), std::make_pair(5, 5)));
}

TEST_F(ScopedBlockingCallIOJankMonitoringTest, InstantUnblockReportsZero) {
  { ScopedBlockingCall instant_unblock(FROM_HERE, BlockingType::MAY_BLOCK); }

  // No janks reported before the monitoring window completes.
  EXPECT_THAT(reports_, ElementsAre());

  task_environment_.FastForwardBy(
      internal::IOJankMonitoringWindow::kMonitoringWindow);

  EXPECT_THAT(reports_, ElementsAre(std::make_pair(0, 0)));

  // No blocking call in next window also reports zero.
  task_environment_.FastForwardBy(
      internal::IOJankMonitoringWindow::kMonitoringWindow);
  EXPECT_THAT(reports_,
              ElementsAre(std::make_pair(0, 0), std::make_pair(0, 0)));
}

// Start the jank mid-interval; that interval should be counted but the last
// incomplete interval won't count.
TEST_F(ScopedBlockingCallIOJankMonitoringTest, Jank7sMidInterval) {
  task_environment_.FastForwardBy(
      internal::IOJankMonitoringWindow::kIOJankInterval / 3);

  constexpr auto kJankTiming =
      internal::IOJankMonitoringWindow::kIOJankInterval * 7;
  {
    ScopedBlockingCall blocked_for_7s(FROM_HERE, BlockingType::MAY_BLOCK);
    task_environment_.FastForwardBy(kJankTiming);
  }

  // No janks reported before the monitoring window completes.
  EXPECT_THAT(reports_, ElementsAre());

  task_environment_.FastForwardBy(
      internal::IOJankMonitoringWindow::kMonitoringWindow);

  EXPECT_THAT(reports_, ElementsAre(std::make_pair(7, 7)));
}

// Start the jank mid-interval; that interval should be counted but the second
// one won't count.
TEST_F(ScopedBlockingCallIOJankMonitoringTest, Jank1sMidInterval) {
  task_environment_.FastForwardBy(
      internal::IOJankMonitoringWindow::kIOJankInterval / 3);

  constexpr auto kJankTiming =
      internal::IOJankMonitoringWindow::kIOJankInterval;
  {
    ScopedBlockingCall blocked_for_1s(FROM_HERE, BlockingType::MAY_BLOCK);
    task_environment_.FastForwardBy(kJankTiming);
  }

  // No janks reported before the monitoring window completes.
  EXPECT_THAT(reports_, ElementsAre());

  task_environment_.FastForwardBy(
      internal::IOJankMonitoringWindow::kMonitoringWindow);

  EXPECT_THAT(reports_, ElementsAre(std::make_pair(1, 1)));
}

// Jank that lasts for 1.3 intervals should be rounded down to 1.
TEST_F(ScopedBlockingCallIOJankMonitoringTest, JankRoundDown) {
  task_environment_.FastForwardBy(
      internal::IOJankMonitoringWindow::kIOJankInterval * 0.9);

  constexpr auto kJankTiming =
      internal::IOJankMonitoringWindow::kIOJankInterval * 1.3;
  {
    ScopedBlockingCall blocked_for_1s(FROM_HERE, BlockingType::MAY_BLOCK);
    task_environment_.FastForwardBy(kJankTiming);
  }

  // No janks reported before the monitoring window completes.
  EXPECT_THAT(reports_, ElementsAre());

  task_environment_.FastForwardBy(
      internal::IOJankMonitoringWindow::kMonitoringWindow);

  EXPECT_THAT(reports_, ElementsAre(std::make_pair(1, 1)));
}

// Jank that lasts for 1.7 intervals should be rounded up to 2.
TEST_F(ScopedBlockingCallIOJankMonitoringTest, JankRoundUp) {
  task_environment_.FastForwardBy(
      internal::IOJankMonitoringWindow::kIOJankInterval * 0.5);

  constexpr auto kJankTiming =
      internal::IOJankMonitoringWindow::kIOJankInterval * 1.7;
  {
    ScopedBlockingCall blocked_for_1s(FROM_HERE, BlockingType::MAY_BLOCK);
    task_environment_.FastForwardBy(kJankTiming);
  }

  // No janks reported before the monitoring window completes.
  EXPECT_THAT(reports_, ElementsAre());

  task_environment_.FastForwardBy(
      internal::IOJankMonitoringWindow::kMonitoringWindow);

  EXPECT_THAT(reports_, ElementsAre(std::make_pair(2, 2)));
}

// Start mid-interval and perform an operation that overlaps into the next one
// but is under the jank timing.
TEST_F(ScopedBlockingCallIOJankMonitoringTest, NoJankMidInterval) {
  task_environment_.FastForwardBy(
      internal::IOJankMonitoringWindow::kIOJankInterval / 3);

  {
    ScopedBlockingCall non_janky(FROM_HERE, BlockingType::MAY_BLOCK);
    task_environment_.FastForwardBy(
        internal::IOJankMonitoringWindow::kIOJankInterval -
        TimeDelta::FromMilliseconds(1));
  }

  // No janks reported before the monitoring window completes.
  EXPECT_THAT(reports_, ElementsAre());

  task_environment_.FastForwardBy(
      internal::IOJankMonitoringWindow::kMonitoringWindow);

  EXPECT_THAT(reports_, ElementsAre(std::make_pair(0, 0)));
}

TEST_F(ScopedBlockingCallIOJankMonitoringTest, MultiThreaded) {
  constexpr auto kJankTiming =
      internal::IOJankMonitoringWindow::kIOJankInterval * 7;

  // Every worker needs to block for precise clock management; hence we can't
  // test beyond the TaskEnvironment's capacity.
  const int kNumJankyTasks =
      test::TaskEnvironment::kNumForegroundThreadPoolThreads;

  TestWaitableEvent all_threads_blocked;
  auto on_thread_blocked = BarrierClosure(
      kNumJankyTasks,
      BindOnce(&TestWaitableEvent::Signal, Unretained(&all_threads_blocked)));

  TestWaitableEvent resume_all_threads;

  for (int i = 0; i < kNumJankyTasks; ++i) {
    base::ThreadPool::PostTask(
        FROM_HERE, {MayBlock()}, BindLambdaForTesting([&]() {
          ScopedBlockingCall blocked_until_signal(FROM_HERE,
                                                  BlockingType::MAY_BLOCK);
          on_thread_blocked.Run();

          ScopedAllowBaseSyncPrimitivesForTesting allow_wait;
          resume_all_threads.Wait();
        }));
  }

  all_threads_blocked.Wait();
  task_environment_.AdvanceClock(kJankTiming);
  resume_all_threads.Signal();
  task_environment_.RunUntilIdle();

  // No janks reported before the monitoring window completes.
  EXPECT_THAT(reports_, ElementsAre());

  task_environment_.FastForwardBy(
      internal::IOJankMonitoringWindow::kMonitoringWindow);

  // Still only 7 janky internals, but more overall janks.
  EXPECT_THAT(reports_, ElementsAre(std::make_pair(7, 7 * kNumJankyTasks)));
}

// 3 janks of 3 seconds; overlapping but starting 1 second apart from each
// other.
TEST_F(ScopedBlockingCallIOJankMonitoringTest, MultiThreadedOverlapped) {
  static const int kNumJankyTasks = 3;
  static_assert(
      kNumJankyTasks <= test::TaskEnvironment::kNumForegroundThreadPoolThreads,
      "");

  TestWaitableEvent next_task_is_blocked(WaitableEvent::ResetPolicy::AUTOMATIC);

  TestWaitableEvent resume_thread[kNumJankyTasks] = {};
  TestWaitableEvent exited_blocking_scope[kNumJankyTasks] = {};

  auto blocking_task = BindLambdaForTesting([&](int task_index) {
    {
      // Simulate jank until |resume_thread[task_index]| is signaled.
      ScopedBlockingCall blocked_until_signal(FROM_HERE,
                                              BlockingType::MAY_BLOCK);
      next_task_is_blocked.Signal();

      ScopedAllowBaseSyncPrimitivesForTesting allow_wait;
      resume_thread[task_index].Wait();
    }
    exited_blocking_scope[task_index].Signal();
  });

  // [0-1]s
  base::ThreadPool::PostTask(FROM_HERE, {MayBlock()},
                             BindOnce(blocking_task, 0));
  next_task_is_blocked.Wait();
  task_environment_.AdvanceClock(
      internal::IOJankMonitoringWindow::kIOJankInterval);

  // [1-2]s
  base::ThreadPool::PostTask(FROM_HERE, {MayBlock()},
                             BindOnce(blocking_task, 1));
  next_task_is_blocked.Wait();
  task_environment_.AdvanceClock(
      internal::IOJankMonitoringWindow::kIOJankInterval);

  // [2-3]s
  base::ThreadPool::PostTask(FROM_HERE, {MayBlock()},
                             BindOnce(blocking_task, 2));
  next_task_is_blocked.Wait();
  task_environment_.AdvanceClock(
      internal::IOJankMonitoringWindow::kIOJankInterval);

  // [3-6]s
  for (int i = 0; i < kNumJankyTasks; ++i) {
    resume_thread[i].Signal();
    exited_blocking_scope[i].Wait();
    task_environment_.AdvanceClock(
        internal::IOJankMonitoringWindow::kIOJankInterval);
  }

  // No janks reported before the monitoring window completes.
  EXPECT_THAT(reports_, ElementsAre());

  task_environment_.FastForwardBy(
      internal::IOJankMonitoringWindow::kMonitoringWindow);

  // 9s of total janks spread across 5 intervals.
  EXPECT_THAT(reports_, ElementsAre(std::make_pair(5, 9)));
}

// 3 janks of 180 seconds; overlapping but starting 60s apart from each other.
// First one starting at 10 seconds (can't start later than that or we'll trip
// the kTimeDiscrepancyTimeout per TaskEnvironment's inability to RunUntilIdle()
// with pending blocked tasks).
#if defined(OS_LINUX) || defined(OS_CHROMEOS)
// https://crbug.com/1071166
#define MAYBE_MultiThreadedOverlappedWindows \
  DISABLED_MultiThreadedOverlappedWindows
#else
#define MAYBE_MultiThreadedOverlappedWindows MultiThreadedOverlappedWindows
#endif
TEST_F(ScopedBlockingCallIOJankMonitoringTest,
       MAYBE_MultiThreadedOverlappedWindows) {
  constexpr int kNumJankyTasks = 3;
  static_assert(
      kNumJankyTasks <= test::TaskEnvironment::kNumForegroundThreadPoolThreads,
      "");

  TestWaitableEvent next_task_is_blocked(WaitableEvent::ResetPolicy::AUTOMATIC);

  TestWaitableEvent resume_thread[kNumJankyTasks] = {};
  TestWaitableEvent exited_blocking_scope[kNumJankyTasks] = {};

  auto blocking_task = BindLambdaForTesting([&](int task_index) {
    {
      // Simulate jank until |resume_thread[task_index]| is signaled.
      ScopedBlockingCall blocked_until_signal(FROM_HERE,
                                              BlockingType::MAY_BLOCK);
      next_task_is_blocked.Signal();

      ScopedAllowBaseSyncPrimitivesForTesting allow_wait;
      resume_thread[task_index].Wait();
    }
    exited_blocking_scope[task_index].Signal();
  });

  // [0-10s] (minus 1 ms to avoid reaching the timeout; this also tests the
  // logic that intervals are rounded down to the starting interval (e.g.
  // interval 9/60 in this case)).
  task_environment_.AdvanceClock(
      internal::IOJankMonitoringWindow::kTimeDiscrepancyTimeout -
      TimeDelta::FromMilliseconds(1));

  // [10-70]s
  base::ThreadPool::PostTask(FROM_HERE, {MayBlock()},
                             BindOnce(blocking_task, 0));
  next_task_is_blocked.Wait();
  task_environment_.AdvanceClock(
      internal::IOJankMonitoringWindow::kMonitoringWindow);

  // [70-130]s
  base::ThreadPool::PostTask(FROM_HERE, {MayBlock()},
                             BindOnce(blocking_task, 1));
  next_task_is_blocked.Wait();
  task_environment_.AdvanceClock(
      internal::IOJankMonitoringWindow::kMonitoringWindow);

  // [130-190]s
  base::ThreadPool::PostTask(FROM_HERE, {MayBlock()},
                             BindOnce(blocking_task, 2));
  next_task_is_blocked.Wait();
  task_environment_.AdvanceClock(
      internal::IOJankMonitoringWindow::kMonitoringWindow);

  // [190-370]s
  for (int i = 0; i < kNumJankyTasks; ++i) {
    resume_thread[i].Signal();
    exited_blocking_scope[i].Wait();
    task_environment_.AdvanceClock(
        internal::IOJankMonitoringWindow::kMonitoringWindow);
  }

  // Already past the last window (relevant events end at 360s); flush the
  // pending ripe delayed task that will complete the last window.
  task_environment_.RunUntilIdle();

  // 540s(180s*3) of total janks spread across 300 intervals in 6 windows.
  // Distributed as such (zoomed out to 6 intervals per window):
  // [011111]
  //        [122222]
  //               [233333]
  //                      [322222]
  //                             [21111]
  //                                   [100000]
  // Starting at the 9th interval per the 10s-1ms offset start.
  EXPECT_THAT(reports_,
              ElementsAre(std::make_pair(51, 51), std::make_pair(60, 111),
                          std::make_pair(60, 171), std::make_pair(60, 129),
                          std::make_pair(60, 69), std::make_pair(9, 9)));
}

TEST_F(ScopedBlockingCallIOJankMonitoringTest, CancellationAcrossSleep) {
  constexpr auto kJankTiming =
      internal::IOJankMonitoringWindow::kIOJankInterval * 7;
  {
    ScopedBlockingCall blocked_for_7s(FROM_HERE, BlockingType::MAY_BLOCK);
    task_environment_.FastForwardBy(kJankTiming);
  }

  // Jump just beyond the kTimeDiscrepancyTimeout for the next window.
  task_environment_.AdvanceClock(
      internal::IOJankMonitoringWindow::kMonitoringWindow +
      internal::IOJankMonitoringWindow::kTimeDiscrepancyTimeout - kJankTiming);
  task_environment_.RunUntilIdle();

  // Window was canceled and previous jank was not reported.
  EXPECT_THAT(reports_, ElementsAre());

  // The second window should be independent and need a full kMonitoringWindow
  // to elapse before reporting.
  task_environment_.FastForwardBy(
      internal::IOJankMonitoringWindow::kMonitoringWindow -
      TimeDelta::FromSeconds(1));
  EXPECT_THAT(reports_, ElementsAre());

  task_environment_.FastForwardBy(TimeDelta::FromSeconds(1));
  EXPECT_THAT(reports_, ElementsAre(std::make_pair(0, 0)));
}

TEST_F(ScopedBlockingCallIOJankMonitoringTest, SleepWithLongJank) {
  {
    ScopedBlockingCall blocked_through_sleep(FROM_HERE,
                                             BlockingType::MAY_BLOCK);

    // Fast-forward 2 full windows and almost to the end of the 3rd.
    task_environment_.FastForwardBy(
        internal::IOJankMonitoringWindow::kMonitoringWindow * 3 -
        TimeDelta::FromSeconds(1));

    // Simulate a "sleep" over the timeout threshold.
    task_environment_.AdvanceClock(
        TimeDelta::FromSeconds(1) +
        internal::IOJankMonitoringWindow::kTimeDiscrepancyTimeout);
  }

  // Two full jank windows are reported when the ScopedBlokcingCall unwinds but
  // the 3rd is canceled.
  EXPECT_THAT(reports_,
              ElementsAre(std::make_pair(60, 60), std::make_pair(60, 60)));

  // The 4th window has a new |start_time| so completing the "remaining delta"
  // doesn't cause a report from the cancelled 3rd window.
  task_environment_.FastForwardBy(
      internal::IOJankMonitoringWindow::kMonitoringWindow -
      TimeDelta::FromSeconds(1));
  EXPECT_THAT(reports_,
              ElementsAre(std::make_pair(60, 60), std::make_pair(60, 60)));

  // Completing the whole 4th window generates a report.
  task_environment_.FastForwardBy(TimeDelta::FromSeconds(1));
  EXPECT_THAT(reports_,
              ElementsAre(std::make_pair(60, 60), std::make_pair(60, 60),
                          std::make_pair(0, 0)));
}

// Verifies that blocking calls on background workers aren't monitored.
// Platforms where !CanUseBackgroundPriorityForWorkerThread() will still monitor
// this jank (as it may interfere with other foreground work).
TEST_F(ScopedBlockingCallIOJankMonitoringTest, BackgroundBlockingCallsIgnored) {
  constexpr auto kJankTiming =
      internal::IOJankMonitoringWindow::kIOJankInterval * 7;

  TestWaitableEvent task_running;
  TestWaitableEvent resume_task;

  base::ThreadPool::PostTask(
      FROM_HERE, {TaskPriority::BEST_EFFORT, MayBlock()},
      BindLambdaForTesting([&]() {
        ScopedBlockingCall blocked_for_7s(FROM_HERE, BlockingType::MAY_BLOCK);
        task_running.Signal();

        ScopedAllowBaseSyncPrimitivesForTesting allow_wait;
        resume_task.Wait();
      }));

  task_running.Wait();
  task_environment_.AdvanceClock(kJankTiming);
  resume_task.Signal();

  // No janks reported before the monitoring window completes.
  EXPECT_THAT(reports_, ElementsAre());

  task_environment_.FastForwardBy(
      internal::IOJankMonitoringWindow::kMonitoringWindow);

  if (internal::CanUseBackgroundPriorityForWorkerThread())
    EXPECT_THAT(reports_, ElementsAre(std::make_pair(0, 0)));
  else
    EXPECT_THAT(reports_, ElementsAre(std::make_pair(7, 7)));
}

TEST_F(ScopedBlockingCallIOJankMonitoringTest,
       BackgroundAndForegroundCallsMixed) {
  constexpr auto kJankTiming =
      internal::IOJankMonitoringWindow::kIOJankInterval * 7;

  TestWaitableEvent tasks_running;
  auto on_task_running = BarrierClosure(
      2, BindOnce(&TestWaitableEvent::Signal, Unretained(&tasks_running)));
  TestWaitableEvent resume_tasks;

  base::ThreadPool::PostTask(
      FROM_HERE, {TaskPriority::BEST_EFFORT, MayBlock()},
      BindLambdaForTesting([&]() {
        ScopedBlockingCall blocked_for_7s(FROM_HERE, BlockingType::MAY_BLOCK);
        on_task_running.Run();

        ScopedAllowBaseSyncPrimitivesForTesting allow_wait;
        resume_tasks.Wait();
      }));

  base::ThreadPool::PostTask(
      FROM_HERE, {TaskPriority::USER_BLOCKING, MayBlock()},
      BindLambdaForTesting([&]() {
        ScopedBlockingCall blocked_for_7s(FROM_HERE, BlockingType::MAY_BLOCK);
        on_task_running.Run();

        ScopedAllowBaseSyncPrimitivesForTesting allow_wait;
        resume_tasks.Wait();
      }));

  tasks_running.Wait();
  task_environment_.AdvanceClock(kJankTiming);
  resume_tasks.Signal();

  // No janks reported before the monitoring window completes.
  EXPECT_THAT(reports_, ElementsAre());

  task_environment_.FastForwardBy(
      internal::IOJankMonitoringWindow::kMonitoringWindow);

  if (internal::CanUseBackgroundPriorityForWorkerThread())
    EXPECT_THAT(reports_, ElementsAre(std::make_pair(7, 7)));
  else
    EXPECT_THAT(reports_, ElementsAre(std::make_pair(7, 14)));
}

TEST_F(ScopedBlockingCallIOJankMonitoringTest, WillBlockNotMonitored) {
  constexpr auto kBlockedTiming =
      internal::IOJankMonitoringWindow::kIOJankInterval * 7;
  {
    ScopedBlockingCall blocked_for_7s(FROM_HERE, BlockingType::WILL_BLOCK);
    task_environment_.FastForwardBy(kBlockedTiming);
  }

  task_environment_.FastForwardBy(
      internal::IOJankMonitoringWindow::kMonitoringWindow);

  EXPECT_THAT(reports_, ElementsAre(std::make_pair(0, 0)));
}

TEST_F(ScopedBlockingCallIOJankMonitoringTest,
       NestedWillBlockCancelsMonitoring) {
  constexpr auto kBlockedTiming =
      internal::IOJankMonitoringWindow::kIOJankInterval * 7;
  {
    ScopedBlockingCall blocked_for_14s(FROM_HERE, BlockingType::MAY_BLOCK);
    task_environment_.FastForwardBy(kBlockedTiming);
    ScopedBlockingCall will_block_for_7s(FROM_HERE, BlockingType::WILL_BLOCK);
    task_environment_.FastForwardBy(kBlockedTiming);
  }

  task_environment_.FastForwardBy(
      internal::IOJankMonitoringWindow::kMonitoringWindow);

  EXPECT_THAT(reports_, ElementsAre(std::make_pair(0, 0)));
}

TEST_F(ScopedBlockingCallIOJankMonitoringTest, NestedMayBlockIgnored) {
  constexpr auto kBlockedTiming =
      internal::IOJankMonitoringWindow::kIOJankInterval * 7;
  {
    ScopedBlockingCall blocked_for_14s(FROM_HERE, BlockingType::MAY_BLOCK);
    task_environment_.FastForwardBy(kBlockedTiming);
    ScopedBlockingCall may_block_for_7s(FROM_HERE, BlockingType::MAY_BLOCK);
    task_environment_.FastForwardBy(kBlockedTiming);
  }

  task_environment_.FastForwardBy(
      internal::IOJankMonitoringWindow::kMonitoringWindow);

  EXPECT_THAT(reports_, ElementsAre(std::make_pair(14, 14)));
}

TEST_F(ScopedBlockingCallIOJankMonitoringTest, BaseSyncPrimitivesNotMonitored) {
  constexpr auto kBlockedTiming =
      internal::IOJankMonitoringWindow::kIOJankInterval * 7;
  {
    // Even with MAY_BLOCK; base-sync-primitives aren't considered I/O jank
    // (base-sync-primitives induced janks/hangs are captured by other tools,
    // like Slow Reports and HangWatcher).
    internal::ScopedBlockingCallWithBaseSyncPrimitives
        base_sync_primitives_for_7s(FROM_HERE, BlockingType::MAY_BLOCK);
    task_environment_.FastForwardBy(kBlockedTiming);
  }

  task_environment_.FastForwardBy(
      internal::IOJankMonitoringWindow::kMonitoringWindow);

  EXPECT_THAT(reports_, ElementsAre(std::make_pair(0, 0)));
}

TEST_F(ScopedBlockingCallIOJankMonitoringTest,
       NestedBaseSyncPrimitivesCancels) {
  constexpr auto kBlockedTiming =
      internal::IOJankMonitoringWindow::kIOJankInterval * 7;
  {
    ScopedBlockingCall blocked_for_14s(FROM_HERE, BlockingType::MAY_BLOCK);
    task_environment_.FastForwardBy(kBlockedTiming);
    internal::ScopedBlockingCallWithBaseSyncPrimitives
        base_sync_primitives_for_7s(FROM_HERE, BlockingType::MAY_BLOCK);
    task_environment_.FastForwardBy(kBlockedTiming);
  }

  task_environment_.FastForwardBy(
      internal::IOJankMonitoringWindow::kMonitoringWindow);

  EXPECT_THAT(reports_, ElementsAre(std::make_pair(0, 0)));
}

// Regression test for crbug.com/1209622
// Disabled due to flakiness on Linux and Fuchsia bots.
// https://crbug.com/1235945
#if defined(OS_LINUX) || defined(OS_FUCHSIA)
#define MAYBE_IgnoreIfTickClockMovesBackwards \
  DISABLED_IgnoreIfTickClockMovesBackwards
#else
#define MAYBE_IgnoreIfTickClockMovesBackwards IgnoreIfTickClockMovesBackwards
#endif
TEST_F(ScopedBlockingCallIOJankMonitoringManualMockTimeTest,
       MAYBE_IgnoreIfTickClockMovesBackwards) {
  // Stomping 4 intervals in the past and 3 janky intervals from there is known
  // to cause havoc (negatively indexing into |intervals_lock_|), per the
  // original repro. Going back only 1 also causes negative indexing but not
  // enough havoc to make this test crash without the fix.
  {
    reported_ticks_ -= internal::IOJankMonitoringWindow::kIOJankInterval * 4;
    ScopedBlockingCall jank_in_past(FROM_HERE, BlockingType::MAY_BLOCK);
    reported_ticks_ += internal::IOJankMonitoringWindow::kIOJankInterval * 3;
  }

  // Force a report immediately, it should have ignored the jank in the past.
  internal::IOJankMonitoringWindow::CancelMonitoringForTesting();
  EXPECT_THAT(reports_, ElementsAre(std::make_pair(0, 0)));
}

}  // namespace base
