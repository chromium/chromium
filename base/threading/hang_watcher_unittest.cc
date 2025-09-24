// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/threading/hang_watcher.h"

#include <memory>
#include <optional>

#include "base/barrier_closure.h"
#include "base/containers/fixed_flat_map.h"
#include "base/feature_list.h"
#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/functional/callback_helpers.h"
#include "base/memory/raw_ptr.h"
#include "base/metrics/field_trial_params.h"
#include "base/run_loop.h"
#include "base/strings/string_number_conversions.h"
#include "base/synchronization/lock.h"
#include "base/synchronization/waitable_event.h"
#include "base/test/bind.h"
#include "base/test/manual_hang_watcher.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/power_monitor_test.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/simple_test_tick_clock.h"
#include "base/test/task_environment.h"
#include "base/test/test_timeouts.h"
#include "base/threading/platform_thread.h"
#include "base/threading/thread_checker.h"
#include "base/threading/threading_features.h"
#include "base/time/tick_clock.h"
#include "base/time/time.h"
#include "build/build_config.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace base {
namespace {

using ::base::test::FeatureRefAndParams;
using ::base::test::ManualHangWatcher;
using ::base::test::ScopedFeatureList;
using ::base::test::SingleThreadTaskEnvironment;
using ::base::test::TaskEnvironment;
using ::testing::ElementsAre;
using ::testing::IsEmpty;
using ::testing::Pair;
using ::testing::TestWithParam;
using ::testing::UnorderedElementsAre;
using ::testing::Values;
using ::testing::ValuesIn;

// Use this value to mark things very far off in the future. Adding this
// to TimeTicks::Now() gives a point that will never be reached during the
// normal execution of a test.
constexpr TimeDelta kVeryLongDelta{base::Days(365)};

// A relatively small time delta to ensure ordering of hung threads list.
constexpr TimeDelta kSmallCPUQuantum{base::Milliseconds(1)};

constexpr uint64_t kArbitraryDeadline = 0x0000C0FFEEC0FFEEu;
constexpr uint64_t kAllOnes = 0xFFFFFFFFFFFFFFFFu;
constexpr uint64_t kAllZeros = 0x0000000000000000u;
constexpr uint64_t kOnesThenZeroes = 0xAAAAAAAAAAAAAAAAu;
constexpr uint64_t kZeroesThenOnes = 0x5555555555555555u;

// Waits on provided WaitableEvent before executing and signals when done.
class BlockedThread : public DelegateSimpleThread::Delegate {
 public:
  BlockedThread(HangWatcher::ThreadType thread_type, TimeDelta timeout)
      : thread_(this, "BlockedThread"),
        thread_type_(thread_type),
        timeout_(timeout) {
    StartAndWaitForScopeEntered();
  }

  ~BlockedThread() override {
    Unblock();
    thread_.Join();
  }

  void Run() override {
    // Open a scope so that `unregister_closure` is destroyed before signaling
    // `run_event_`.
    {
      // (Un)Register the thread here instead of in ctor/dtor so that the action
      // happens on the right thread.
      base::ScopedClosureRunner unregister_closure =
          base::HangWatcher::RegisterThread(thread_type_);

      WatchHangsInScope scope(timeout_);
      wait_until_entered_scope_.Signal();

      unblock_thread_.Wait();
    }
    run_event_.Signal();
  }

  bool IsDone() { return run_event_.IsSignaled(); }

  void StartAndWaitForScopeEntered() {
    thread_.Start();
    // Block until this thread registered itself for hang watching and has
    // entered a WatchHangsInScope.
    wait_until_entered_scope_.Wait();
  }

  void Unblock() { unblock_thread_.Signal(); }

  void WaitDone() { run_event_.Wait(); }

  PlatformThreadId GetId() { return thread_.tid(); }

 private:
  base::DelegateSimpleThread thread_;

  // Will be signaled once the thread is properly registered for watching and
  // the WatchHangsInScope has been entered.
  WaitableEvent wait_until_entered_scope_;

  // Will be signaled once ThreadMain has run.
  WaitableEvent run_event_;

  // Used to unblock the monitored thread. Signaled from the test main thread.
  base::WaitableEvent unblock_thread_;

  const HangWatcher::ThreadType thread_type_;
  const base::TimeDelta timeout_;
};

// Scope object starting a BlockedThread for all thread types monitored by the
// HangWatcher. Threads are started by the constructor and joined in the
// destructor.
class BlockedThreadsForAllTypes {
 public:
  explicit BlockedThreadsForAllTypes(base::TimeDelta timeout)
      : main_(HangWatcher::ThreadType::kMainThread, timeout),
        compositor_(HangWatcher::ThreadType::kCompositorThread, timeout),
        io_(HangWatcher::ThreadType::kIOThread, timeout),
        pool_(HangWatcher::ThreadType::kThreadPoolThread, timeout) {}

 private:
  BlockedThread main_;
  BlockedThread compositor_;
  BlockedThread io_;
  BlockedThread pool_;
};

class HangWatcherTest : public testing::Test {
 protected:
  // Used exclusively for MOCK_TIME. No tasks will be run on the environment.
  // Single threaded to avoid ThreadPool WorkerThreads registering.
  test::SingleThreadTaskEnvironment task_environment_{
      test::TaskEnvironment::TimeSource::MOCK_TIME};
};

using HangWatcherEnabledTest = TestWithParam<HangWatcher::ProcessType>;
INSTANTIATE_TEST_SUITE_P(AllEnabledProcessTypes,
                         HangWatcherEnabledTest,
                         Values(HangWatcher::ProcessType::kBrowserProcess,
                                HangWatcher::ProcessType::kRendererProcess,
                                HangWatcher::ProcessType::kUtilityProcess));
TEST_P(HangWatcherEnabledTest, HangWatcherEnabled) {
  ScopedFeatureList enable_hang_watcher(kEnableHangWatcher);
  ManualHangWatcher hang_watcher(GetParam());
  EXPECT_TRUE(hang_watcher.IsEnabled());
}

TEST(HangWatcherGpuEnabledTest, HangWatcherDisabledOnGpuProcessByDefault) {
  ScopedFeatureList enable_hang_watcher(kEnableHangWatcher);
  ManualHangWatcher hang_watcher(HangWatcher::ProcessType::kGPUProcess);
  EXPECT_FALSE(hang_watcher.IsEnabled());
}

TEST(HangWatcherGpuEnabledTest, HangWatcherEnabledOnGpuProcessViaFeature) {
  ScopedFeatureList enable_gpu_watcher(kEnableHangWatcherOnGpuProcess);
  ManualHangWatcher hang_watcher(HangWatcher::ProcessType::kGPUProcess);
  EXPECT_TRUE(hang_watcher.IsEnabled());
}

TEST_F(HangWatcherTest, InvalidatingExpectationsPreventsCapture) {
  ScopedFeatureList enable_hang_watcher(kEnableHangWatcher);
  ManualHangWatcher hang_watcher(HangWatcher::ProcessType::kBrowserProcess);

  // Register the main test thread for hang watching.
  auto unregister_thread_closure =
      HangWatcher::RegisterThread(base::HangWatcher::ThreadType::kMainThread);

  // Create a hang.
  WatchHangsInScope expires_instantly(base::TimeDelta{});
  task_environment_.FastForwardBy(base::Seconds(1));

  // de-activate hang watching,
  base::HangWatcher::InvalidateActiveExpectations();

  // Trigger a monitoring on HangWatcher thread and verify results.
  // Hang is not detected.
  hang_watcher.TriggerSynchronousMonitoring();
  EXPECT_EQ(hang_watcher.GetHangCount(), 0);
}

TEST_F(HangWatcherTest, MultipleInvalidateExpectationsDoNotCancelOut) {
  ScopedFeatureList enable_hang_watcher(kEnableHangWatcher);
  ManualHangWatcher hang_watcher(HangWatcher::ProcessType::kBrowserProcess);

  // Register the main test thread for hang watching.
  auto unregister_thread_closure =
      HangWatcher::RegisterThread(base::HangWatcher::ThreadType::kMainThread);

  // Create a hang.
  WatchHangsInScope expires_instantly(base::TimeDelta{});
  task_environment_.FastForwardBy(base::Seconds(1));

  // de-activate hang watching,
  base::HangWatcher::InvalidateActiveExpectations();

  // Redundantly de-activate hang watching.
  base::HangWatcher::InvalidateActiveExpectations();

  // Trigger a monitoring on HangWatcher thread and verify results.
  // Hang is not detected.
  hang_watcher.TriggerSynchronousMonitoring();
  EXPECT_EQ(hang_watcher.GetHangCount(), 0);
}

// TODO(crbug.com/385732561): Test is flaky.
TEST_F(HangWatcherTest,
       DISABLED_NewInnerWatchHangsInScopeAfterInvalidationDetectsHang) {
  ScopedFeatureList enable_hang_watcher(kEnableHangWatcher);
  ManualHangWatcher hang_watcher(HangWatcher::ProcessType::kBrowserProcess);

  // Register the main test thread for hang watching.
  auto unregister_thread_closure =
      HangWatcher::RegisterThread(base::HangWatcher::ThreadType::kMainThread);

  WatchHangsInScope expires_instantly(base::TimeDelta{});
  task_environment_.FastForwardBy(base::Seconds(1));

  // De-activate hang watching.
  base::HangWatcher::InvalidateActiveExpectations();

  {
    WatchHangsInScope also_expires_instantly(base::TimeDelta{});
    task_environment_.FastForwardBy(base::Seconds(1));

    // Trigger a monitoring on HangWatcher thread and verify results.
    hang_watcher.TriggerSynchronousMonitoring();

    // Hang is detected since the new WatchHangsInScope temporarily
    // re-activated hang_watching.
    EXPECT_EQ(hang_watcher.GetHangCount(), 1);
  }

  // Trigger a monitoring on HangWatcher thread and verify results.
  hang_watcher.TriggerSynchronousMonitoring();

  // No new hang is detected since execution is back to being covered by
  // |expires_instantly| for which expectations were invalidated.
  EXPECT_EQ(hang_watcher.GetHangCount(), 1);
}

TEST_F(HangWatcherTest,
       NewSeparateWatchHangsInScopeAfterInvalidationDetectsHang) {
  ScopedFeatureList enable_hang_watcher(kEnableHangWatcher);
  ManualHangWatcher hang_watcher(HangWatcher::ProcessType::kBrowserProcess);

  // Register the main test thread for hang watching.
  auto unregister_thread_closure =
      HangWatcher::RegisterThread(base::HangWatcher::ThreadType::kMainThread);

  {
    WatchHangsInScope expires_instantly(base::TimeDelta{});
    task_environment_.FastForwardBy(base::Seconds(1));

    // De-activate hang watching.
    base::HangWatcher::InvalidateActiveExpectations();
  }

  WatchHangsInScope also_expires_instantly(base::TimeDelta{});
  task_environment_.FastForwardBy(base::Seconds(1));

  // Trigger a monitoring on HangWatcher thread and verify results.
  hang_watcher.TriggerSynchronousMonitoring();

  // Hang is detected since the new WatchHangsInScope did not have its
  // expectations invalidated.
  EXPECT_EQ(hang_watcher.GetHangCount(), 1);
}

// Test that invalidating expectations from inner WatchHangsInScope will also
// prevent hang detection in outer scopes.
TEST_F(HangWatcherTest, ScopeDisabledObjectInnerScope) {
  ScopedFeatureList enable_hang_watcher(kEnableHangWatcher);
  ManualHangWatcher hang_watcher(HangWatcher::ProcessType::kBrowserProcess);

  // Register the main test thread for hang watching.
  auto unregister_thread_closure =
      HangWatcher::RegisterThread(base::HangWatcher::ThreadType::kMainThread);

  // Start a WatchHangsInScope that expires right away. Then advance
  // time to make sure no hang is detected.
  WatchHangsInScope expires_instantly(base::TimeDelta{});
  task_environment_.FastForwardBy(base::Seconds(1));
  {
    WatchHangsInScope also_expires_instantly(base::TimeDelta{});

    // De-activate hang watching.
    base::HangWatcher::InvalidateActiveExpectations();
    task_environment_.FastForwardBy(base::Seconds(1));
  }

  // Trigger a monitoring on HangWatcher thread and verify results.
  hang_watcher.TriggerSynchronousMonitoring();

  // Hang is ignored since it concerns a scope for which one of the inner scope
  // was ignored.
  EXPECT_EQ(hang_watcher.GetHangCount(), 0);
}

TEST_F(HangWatcherTest, NewScopeAfterDisabling) {
  ScopedFeatureList enable_hang_watcher(kEnableHangWatcher);
  ManualHangWatcher hang_watcher(HangWatcher::ProcessType::kBrowserProcess);

  // Register the main test thread for hang watching.
  auto unregister_thread_closure =
      HangWatcher::RegisterThread(base::HangWatcher::ThreadType::kMainThread);

  // Start a WatchHangsInScope that expires right away. Then advance
  // time to make sure no hang is detected.
  WatchHangsInScope expires_instantly(base::TimeDelta{});
  task_environment_.FastForwardBy(base::Seconds(1));
  {
    WatchHangsInScope also_expires_instantly(base::TimeDelta{});

    // De-activate hang watching.
    base::HangWatcher::InvalidateActiveExpectations();
    task_environment_.FastForwardBy(base::Seconds(1));
  }

  // New scope for which expectations are never invalidated.
  WatchHangsInScope also_expires_instantly(base::TimeDelta{});
  task_environment_.FastForwardBy(base::Seconds(1));

  // Trigger a monitoring on HangWatcher thread and verify results.
  hang_watcher.TriggerSynchronousMonitoring();

  // Hang is detected because it's unrelated to the hangs that were disabled.
  EXPECT_EQ(hang_watcher.GetHangCount(), 1);
}

TEST_F(HangWatcherTest, NestedScopes) {
  ScopedFeatureList enable_hang_watcher(kEnableHangWatcher);
  ManualHangWatcher hang_watcher(HangWatcher::ProcessType::kBrowserProcess);

  // Create a state object for the test thread since this test is single
  // threaded.
  auto current_hang_watch_state =
      base::internal::HangWatchState::CreateHangWatchStateForCurrentThread(
          HangWatcher::ThreadType::kMainThread);

  ASSERT_FALSE(current_hang_watch_state->IsOverDeadline());
  base::TimeTicks original_deadline = current_hang_watch_state->GetDeadline();

  constexpr base::TimeDelta kFirstTimeout(base::Milliseconds(500));
  base::TimeTicks first_deadline = base::TimeTicks::Now() + kFirstTimeout;

  constexpr base::TimeDelta kSecondTimeout(base::Milliseconds(250));
  base::TimeTicks second_deadline = base::TimeTicks::Now() + kSecondTimeout;

  // At this point we have not set any timeouts.
  {
    // Create a first timeout which is more restrictive than the default.
    WatchHangsInScope first_scope(kFirstTimeout);

    // We are on mock time. There is no time advancement and as such no hangs.
    EXPECT_FALSE(current_hang_watch_state->IsOverDeadline());
    EXPECT_EQ(current_hang_watch_state->GetDeadline(), first_deadline);
    {
      // Set a yet more restrictive deadline. Still no hang.
      WatchHangsInScope second_scope(kSecondTimeout);
      EXPECT_FALSE(current_hang_watch_state->IsOverDeadline());
      EXPECT_EQ(current_hang_watch_state->GetDeadline(), second_deadline);
    }
    // First deadline we set should be restored.
    EXPECT_FALSE(current_hang_watch_state->IsOverDeadline());
    EXPECT_EQ(current_hang_watch_state->GetDeadline(), first_deadline);
  }

  // Original deadline should now be restored.
  EXPECT_FALSE(current_hang_watch_state->IsOverDeadline());
  EXPECT_EQ(current_hang_watch_state->GetDeadline(), original_deadline);
}

// Checks that histograms are recorded on the right threads for the browser
// process.
TEST_F(HangWatcherTest, HistogramsLoggedOnBrowserProcessHang) {
  ScopedFeatureList enable_hang_watcher(kEnableHangWatcher);
  base::HistogramTester histogram_tester;
  ManualHangWatcher hang_watcher(HangWatcher::ProcessType::kBrowserProcess);

  // Start blocked threads for all thread types and simulate hangs.
  BlockedThreadsForAllTypes threads(/*timeout=*/base::Seconds(10));
  task_environment_.FastForwardBy(base::Seconds(11));

  // Check that histograms are only recorded for the expected threads.
  hang_watcher.TriggerSynchronousMonitoring();
  EXPECT_THAT(
      histogram_tester.GetAllSamplesForPrefix(
          "HangWatcher.IsThreadHung.BrowserProcess"),
      UnorderedElementsAre(
          Pair("HangWatcher.IsThreadHung.BrowserProcess.UIThread.Normal",
               BucketsAre(Bucket(true, /*count=*/1))),
          Pair("HangWatcher.IsThreadHung.BrowserProcess.IOThread.Normal",
               BucketsAre(Bucket(true, /*count=*/1)))));
}

TEST_F(HangWatcherTest, HistogramsLoggedOnGpuProcessHang) {
  ScopedFeatureList enable_gpu_watcher(kEnableHangWatcherOnGpuProcess);
  HistogramTester histogram_tester;
  ManualHangWatcher hang_watcher(HangWatcher::ProcessType::kGPUProcess);

  // Start blocked threads for all thread types and simulate hangs.
  BlockedThreadsForAllTypes threads(/*timeout=*/base::Seconds(10));
  task_environment_.FastForwardBy(base::Seconds(11));

  // Check that histograms are only recorded for the expected threads.
  hang_watcher.TriggerSynchronousMonitoring();
  EXPECT_THAT(histogram_tester.GetAllSamplesForPrefix(
                  "HangWatcher.IsThreadHung.GpuProcess"),
              UnorderedElementsAre(
                  Pair("HangWatcher.IsThreadHung.GpuProcess.MainThread",
                       BucketsAre(Bucket(true, /*count=*/1))),
                  Pair("HangWatcher.IsThreadHung.GpuProcess.IOThread",
                       BucketsAre(Bucket(true, /*count=*/1))),
                  Pair("HangWatcher.IsThreadHung.GpuProcess.CompositorThread",
                       BucketsAre(Bucket(true, /*count=*/1)))));
}

struct AnyCriticalTestParam {
  std::string test_name;
  HangWatcher::ProcessType process_type;
  HangWatcher::ThreadType thread_type;
  bool is_critical;
};

// Spot check critical and non-critical process and thread types. We can't do a
// full cross product because some processes types don't support some thread
// types.
using HangWatcherAnyCriticalThreadTests = TestWithParam<AnyCriticalTestParam>;
INSTANTIATE_TEST_SUITE_P(
    CriticalProcessAndThreadSpotChecks,
    HangWatcherAnyCriticalThreadTests,
    ValuesIn<AnyCriticalTestParam>({
        // Test at least one critical thread per process types:
        {.test_name = "BrowserProcessIsCritical",
         .process_type = HangWatcher::ProcessType::kBrowserProcess,
         .thread_type = HangWatcher::ThreadType::kMainThread,
         .is_critical = true},
        {.test_name = "RendererProcessIsCritical",
         .process_type = HangWatcher::ProcessType::kRendererProcess,
         .thread_type = HangWatcher::ThreadType::kMainThread,
         .is_critical = true},
        {.test_name = "UtilityProcessIsCritical",
         .process_type = HangWatcher::ProcessType::kUtilityProcess,
         .thread_type = HangWatcher::ThreadType::kMainThread,
         .is_critical = true},
        {.test_name = "GpuProcessIsCritical",
         .process_type = HangWatcher::ProcessType::kGPUProcess,
         .thread_type = HangWatcher::ThreadType::kMainThread,
         .is_critical = true},
        // Test each critical thread types for one process type:
        {.test_name = "MainThreadIsCritical",
         .process_type = HangWatcher::ProcessType::kBrowserProcess,
         .thread_type = HangWatcher::ThreadType::kMainThread,
         .is_critical = true},
        {.test_name = "IOThreadIsCritical",
         .process_type = HangWatcher::ProcessType::kBrowserProcess,
         .thread_type = HangWatcher::ThreadType::kIOThread,
         .is_critical = true},
        {.test_name = "CompositorThreadIsCritical",
         .process_type = HangWatcher::ProcessType::kRendererProcess,
         .thread_type = HangWatcher::ThreadType::kCompositorThread,
         .is_critical = true},
        // Test non critical threads:
        {.test_name = "ThreadPoolIsNotCritical",
         .process_type = HangWatcher::ProcessType::kBrowserProcess,
         .thread_type = HangWatcher::ThreadType::kThreadPoolThread,
         .is_critical = false},
    }),
    [](const auto& info) { return info.param.test_name; });

// Checks that Any and AnyCritical are correctly recorded for different process
// and thread types.
// TODO(crbug.com/446160865): Re-enable this test.
#if BUILDFLAG(IS_IOS)
#define MAYBE_AnyCriticalThreadHung DISABLED_AnyCriticalThreadHung
#else
#define MAYBE_AnyCriticalThreadHung AnyCriticalThreadHung
#endif
TEST_P(HangWatcherAnyCriticalThreadTests, MAYBE_AnyCriticalThreadHung) {
  ScopedFeatureList enable_hang_watcher(kEnableHangWatcher);
  ScopedFeatureList enable_gpu_hang_watcher(kEnableHangWatcherOnGpuProcess);
  SingleThreadTaskEnvironment task_env(TaskEnvironment::TimeSource::MOCK_TIME);
  base::HistogramTester histogram_tester;
  ManualHangWatcher hang_watcher(GetParam().process_type);

  // Start a blocked thread and simulate a hang.
  BlockedThread thread(GetParam().thread_type, base::Seconds(10));
  task_env.FastForwardBy(base::Seconds(11));

  hang_watcher.TriggerSynchronousMonitoring();
  EXPECT_THAT(
      histogram_tester.GetAllSamplesForPrefix("HangWatcher.IsThreadHung.Any"),
      UnorderedElementsAre(
          Pair("HangWatcher.IsThreadHung.Any",
               BucketsAre(Bucket(true, /*count=*/1))),
          Pair("HangWatcher.IsThreadHung.AnyCritical",
               BucketsAre(Bucket(GetParam().is_critical, /*count=*/1)))));
}

// Checks that only a single Any/AnyCritical histogram is recorded even if
// multiple threads hang.
TEST_F(HangWatcherTest, AnyRecordedOnlyOnceEvenIfMultipleThreadsHang) {
  ScopedFeatureList enable_hang_watcher(kEnableHangWatcher);
  ManualHangWatcher hang_watcher(HangWatcher::ProcessType::kBrowserProcess);
  base::HistogramTester histogram_tester;

  // Start and hang multiple threads.
  BlockedThread main(HangWatcher::ThreadType::kMainThread, base::Seconds(10));
  BlockedThread io(HangWatcher::ThreadType::kIOThread, base::Seconds(10));
  task_environment_.FastForwardBy(base::Seconds(11));

  // A single Any/AnyCritical should be recorded, even if multiple threads hung.
  hang_watcher.TriggerSynchronousMonitoring();
  EXPECT_THAT(
      histogram_tester.GetAllSamplesForPrefix("HangWatcher.IsThreadHung.Any"),
      UnorderedElementsAre(Pair("HangWatcher.IsThreadHung.Any",
                                BucketsAre(Bucket(true, /*count=*/1))),
                           Pair("HangWatcher.IsThreadHung.AnyCritical",
                                BucketsAre(Bucket(true, /*count=*/1)))));
}

// Checks that histograms with `false` buckets are recorded if there's no hang.
TEST_F(HangWatcherTest, HistogramsLoggedWithoutHangs) {
  ScopedFeatureList enable_hang_watcher(kEnableHangWatcher);
  base::HistogramTester histogram_tester;
  ManualHangWatcher hang_watcher(HangWatcher::ProcessType::kBrowserProcess);

  // Start a blocked thread with a 10 seconds hang limit, but don't fastforward
  // time.
  BlockedThread thread(HangWatcher::ThreadType::kMainThread, base::Seconds(10));

  // No hang to catch so nothing is recorded.
  hang_watcher.TriggerSynchronousMonitoring();
  EXPECT_EQ(hang_watcher.GetHangCount(), 0);

  // A thread of type ThreadForTesting was monitored but didn't hang. This is
  // logged.
  EXPECT_THAT(
      histogram_tester.GetAllSamplesForPrefix("HangWatcher.IsThreadHung"),
      UnorderedElementsAre(
          Pair("HangWatcher.IsThreadHung.BrowserProcess.UIThread.Normal",
               BucketsAre(Bucket(false, /*count=*/1))),
          Pair("HangWatcher.IsThreadHung.Any",
               BucketsAre(Bucket(false, /*count=*/1))),
          Pair("HangWatcher.IsThreadHung.AnyCritical",
               BucketsAre(Bucket(false, /*count=*/1)))));
}

// Histograms should be recorded on each monitoring.
TEST_F(HangWatcherTest, HistogramsLoggedOnEachHang) {
  ScopedFeatureList enable_hang_watcher(kEnableHangWatcher);
  base::HistogramTester histogram_tester;
  ManualHangWatcher hang_watcher(HangWatcher::ProcessType::kBrowserProcess);

  // Start a blocked thread and simulate a hang.
  BlockedThread thread(HangWatcher::ThreadType::kMainThread, base::Seconds(10));
  task_environment_.FastForwardBy(base::Seconds(11));

  // First monitoring catches the hang and emits the histogram.
  hang_watcher.TriggerSynchronousMonitoring();
  EXPECT_THAT(
      histogram_tester.GetAllSamplesForPrefix("HangWatcher.IsThreadHung"),
      UnorderedElementsAre(
          Pair("HangWatcher.IsThreadHung.BrowserProcess.UIThread.Normal",
               BucketsAre(Bucket(true, /*count=*/1))),
          Pair("HangWatcher.IsThreadHung.Any",
               BucketsAre(Bucket(true, /*count=*/1))),
          Pair("HangWatcher.IsThreadHung.AnyCritical",
               BucketsAre(Bucket(true, /*count=*/1)))));

  // Hang is logged again even if it would not trigger a crash dump.
  hang_watcher.TriggerSynchronousMonitoring();
  EXPECT_THAT(
      histogram_tester.GetAllSamplesForPrefix("HangWatcher.IsThreadHung"),
      UnorderedElementsAre(
          Pair("HangWatcher.IsThreadHung.BrowserProcess.UIThread.Normal",
               BucketsAre(Bucket(true, /*count=*/2))),
          Pair("HangWatcher.IsThreadHung.Any",
               BucketsAre(Bucket(true, /*count=*/2))),
          Pair("HangWatcher.IsThreadHung.AnyCritical",
               BucketsAre(Bucket(true, /*count=*/2)))));
}

// Checks that the browser process emits Shutdown histograms on shutdown.
TEST_F(HangWatcherTest, HistogramsLoggedWithShutdownFlag) {
  ScopedFeatureList enable_hang_watcher(kEnableHangWatcher);
  base::HistogramTester histogram_tester;
  ManualHangWatcher hang_watcher(HangWatcher::ProcessType::kBrowserProcess);

  // Start blocked threads for all thread types and simulate hangs.
  BlockedThreadsForAllTypes threads(/*timeout=*/base::Seconds(10));
  task_environment_.FastForwardBy(base::Seconds(11));

  // Make this process emit *.Shutdown instead of *.Normal histograms.
  base::HangWatcher::SetShuttingDown();

  // Check that histograms are only recorded for the expected threads.
  hang_watcher.TriggerSynchronousMonitoring();
  EXPECT_THAT(
      histogram_tester.GetAllSamplesForPrefix(
          "HangWatcher.IsThreadHung.BrowserProcess"),
      UnorderedElementsAre(
          Pair("HangWatcher.IsThreadHung.BrowserProcess.UIThread.Shutdown",
               BucketsAre(Bucket(true, /*count=*/1))),
          Pair("HangWatcher.IsThreadHung.BrowserProcess.IOThread.Shutdown",
               BucketsAre(Bucket(true, /*count=*/1)))));
}

// Parameterized test for validating log-level feature params.
struct HangWatcherLogLevelTestParam {
  std::string test_name;
  HangWatcher::ProcessType process_type;
  std::vector<FeatureRefAndParams> enabled_features;
  bool emit_crashes = false;
  int expected_hang_count;
};
using HangWatcherLogLevelTest = TestWithParam<HangWatcherLogLevelTestParam>;
INSTANTIATE_TEST_SUITE_P(
    LogLevels,
    HangWatcherLogLevelTest,
    ValuesIn<HangWatcherLogLevelTestParam>({
        // Browser process.
        {.test_name = "BrowserCrashReportsEnabledByDefaultIfEmitCrashTrue",
         .process_type = HangWatcher::ProcessType::kBrowserProcess,
         .enabled_features = {FeatureRefAndParams(kEnableHangWatcher, {})},
         .emit_crashes = true,
         .expected_hang_count = 1},
        {.test_name = "BrowserCrashReportsDisabledByDefault",
         .process_type = HangWatcher::ProcessType::kBrowserProcess,
         .enabled_features = {FeatureRefAndParams(kEnableHangWatcher, {})},
         .expected_hang_count = 0},
        {.test_name = "BrowserCrashReportsDisabledAtLogLevel1",
         .process_type = HangWatcher::ProcessType::kBrowserProcess,
         .enabled_features = {FeatureRefAndParams(
             kEnableHangWatcher,
             {{kBrowserProcessUiThreadLogLevelParam, "1"}})},
         .expected_hang_count = 0},
        {.test_name = "BrowserCrashReportsEnabledForUiThread",
         .process_type = HangWatcher::ProcessType::kBrowserProcess,
         .enabled_features = {FeatureRefAndParams(
             kEnableHangWatcher,
             {{kBrowserProcessUiThreadLogLevelParam, "2"}})},
         .expected_hang_count = 1},
        {.test_name = "BrowserCrashReportsEnabledForIoThread",
         .process_type = HangWatcher::ProcessType::kBrowserProcess,
         .enabled_features = {FeatureRefAndParams(
             kEnableHangWatcher,
             {{kBrowserProcessIoThreadLogLevelParam, "2"}})},
         .expected_hang_count = 1},
        {.test_name = "BrowserCrashReportsAlwaysDisabledForThreadPoolThreads",
         .process_type = HangWatcher::ProcessType::kBrowserProcess,
         .enabled_features = {FeatureRefAndParams(
             kEnableHangWatcher,
             {{kBrowserProcessThreadPoolLogLevelParam, "2"}})},
         .expected_hang_count = 1},

        // GPU process.
        {.test_name = "GpuCrashReportsDisabledByDefault",
         .process_type = HangWatcher::ProcessType::kGPUProcess,
         .enabled_features =
             {FeatureRefAndParams(kEnableHangWatcherOnGpuProcess, {})},
         .expected_hang_count = 0},
        {.test_name = "GpuCrashReportsDisabledAtLogLevel1",
         .process_type = HangWatcher::ProcessType::kGPUProcess,
         .enabled_features = {FeatureRefAndParams(
             kEnableHangWatcherOnGpuProcess,
             {{kGpuProcessMainThreadLogLevelParam, "1"}})},
         .expected_hang_count = 0},
        {.test_name = "GpuCrashReportsEnabledForMainThread",
         .process_type = HangWatcher::ProcessType::kGPUProcess,
         .enabled_features = {FeatureRefAndParams(
             kEnableHangWatcherOnGpuProcess,
             {{kGpuProcessMainThreadLogLevelParam, "2"}})},
         .expected_hang_count = 1},
        {.test_name = "GpuCrashReportsEnabledForIoThread",
         .process_type = HangWatcher::ProcessType::kGPUProcess,
         .enabled_features = {FeatureRefAndParams(
             kEnableHangWatcherOnGpuProcess,
             {{kGpuProcessIoThreadLogLevelParam, "2"}})},
         .expected_hang_count = 1},
        {.test_name = "GpuCrashReportsEnabledForCompositorThread",
         .process_type = HangWatcher::ProcessType::kGPUProcess,
         .enabled_features = {FeatureRefAndParams(
             kEnableHangWatcherOnGpuProcess,
             {{kGpuProcessCompositorThreadLogLevelParam, "2"}})},
         .expected_hang_count = 1},
        {.test_name = "GpuCrashReportsEnabledForThreadPoolThreads",
         .process_type = HangWatcher::ProcessType::kGPUProcess,
         .enabled_features = {FeatureRefAndParams(
             kEnableHangWatcherOnGpuProcess,
             {{kGpuProcessThreadPoolLogLevelParam, "2"}})},
         .expected_hang_count = 1},

        // Renderer process.
        {.test_name = "RendererCrashReportsDisabledByDefault",
         .process_type = HangWatcher::ProcessType::kRendererProcess,
         .enabled_features = {FeatureRefAndParams(kEnableHangWatcher, {})},
         .expected_hang_count = 0},
        {.test_name = "RendererCrashReportsDisabledAtLogLevel1",
         .process_type = HangWatcher::ProcessType::kRendererProcess,
         .enabled_features = {FeatureRefAndParams(
             kEnableHangWatcher,
             {{kRendererProcessMainThreadLogLevelParam, "1"}})},
         .expected_hang_count = 0},
        {.test_name = "RendererCrashReportsEnabledForMainThread",
         .process_type = HangWatcher::ProcessType::kRendererProcess,
         .enabled_features = {FeatureRefAndParams(
             kEnableHangWatcher,
             {{kRendererProcessMainThreadLogLevelParam, "2"}})},
         .expected_hang_count = 1},
        {.test_name = "RendererCrashReportsEnabledForIoThread",
         .process_type = HangWatcher::ProcessType::kRendererProcess,
         .enabled_features = {FeatureRefAndParams(
             kEnableHangWatcher,
             {{kRendererProcessIoThreadLogLevelParam, "2"}})},
         .expected_hang_count = 1},
        {.test_name = "RendererCrashReportsEnabledForCompositorThread",
         .process_type = HangWatcher::ProcessType::kRendererProcess,
         .enabled_features = {FeatureRefAndParams(
             kEnableHangWatcher,
             {{kRendererProcessCompositorThreadLogLevelParam, "2"}})},
         .expected_hang_count = 1},
        {.test_name = "RendererCrashReportsEnabledForThreadPoolThreads",
         .process_type = HangWatcher::ProcessType::kRendererProcess,
         .enabled_features = {FeatureRefAndParams(
             kEnableHangWatcher,
             {{kRendererProcessThreadPoolLogLevelParam, "2"}})},
         .expected_hang_count = 1},

        // Utility process.
        {.test_name = "UtilityCrashReportsDisabledByDefault",
         .process_type = HangWatcher::ProcessType::kUtilityProcess,
         .enabled_features = {FeatureRefAndParams(kEnableHangWatcher, {})},
         .expected_hang_count = 0},
        {.test_name = "UtilityCrashReportsDisabledAtLogLevel1",
         .process_type = HangWatcher::ProcessType::kUtilityProcess,
         .enabled_features = {FeatureRefAndParams(
             kEnableHangWatcher,
             {{kUtilityProcessMainThreadLogLevelParam, "1"}})},
         .expected_hang_count = 0},
        {.test_name = "UtilityCrashReportsEnabledForMainThread",
         .process_type = HangWatcher::ProcessType::kUtilityProcess,
         .enabled_features = {FeatureRefAndParams(
             kEnableHangWatcher,
             {{kUtilityProcessMainThreadLogLevelParam, "2"}})},
         .expected_hang_count = 1},
        {.test_name = "UtilityCrashReportsEnabledForIoThread",
         .process_type = HangWatcher::ProcessType::kUtilityProcess,
         .enabled_features = {FeatureRefAndParams(
             kEnableHangWatcher,
             {{kUtilityProcessIoThreadLogLevelParam, "2"}})},
         .expected_hang_count = 1},
        {.test_name = "UtilityCrashReportsEnabledForThreadPoolThreads",
         .process_type = HangWatcher::ProcessType::kUtilityProcess,
         .enabled_features = {FeatureRefAndParams(
             kEnableHangWatcher,
             {{kUtilityProcessThreadPoolLogLevelParam, "2"}})},
         .expected_hang_count = 1},
    }),
    [](const auto& info) { return info.param.test_name; });

// Tests that log level can be controlled via feature params.
TEST_P(HangWatcherLogLevelTest, CrashLogLevels) {
  SingleThreadTaskEnvironment task_env(TaskEnvironment::TimeSource::MOCK_TIME);
  ScopedFeatureList enable_hang_watcher;
  enable_hang_watcher.InitWithFeaturesAndParameters(GetParam().enabled_features,
                                                    {});
  ManualHangWatcher hang_watcher(GetParam().process_type,
                                 GetParam().emit_crashes);

  ASSERT_TRUE(hang_watcher.IsEnabled());

  // Start blocked threads for all thread types and simulate hangs.
  BlockedThreadsForAllTypes threads(base::Seconds(10));
  task_env.FastForwardBy(base::Seconds(11));

  // Hang reports are enabled when the log level is set to 2.
  hang_watcher.TriggerSynchronousMonitoring();
  EXPECT_EQ(hang_watcher.GetHangCount(), GetParam().expected_hang_count);
}

// Test that hangs get recorded for the browser process.
TEST_F(HangWatcherTest, Hang) {
  ScopedFeatureList enable_hang_watcher(kEnableHangWatcher);
  ManualHangWatcher hang_watcher(HangWatcher::ProcessType::kBrowserProcess);

  // Start a blocked thread and simulate a hang.
  BlockedThread thread(HangWatcher::ThreadType::kMainThread, base::Seconds(10));
  task_environment_.FastForwardBy(base::Seconds(11));

  // First monitoring catches and records the hang.
  hang_watcher.TriggerSynchronousMonitoring();
  EXPECT_EQ(hang_watcher.GetHangCount(), 1);
}

// Tests that hangs don't get recorded for the GPU process by default.
TEST_F(HangWatcherTest, GpuProcessHangReportingDisabledByDefault) {
  ScopedFeatureList enable_gpu_watcher(kEnableHangWatcherOnGpuProcess);
  ManualHangWatcher hang_watcher(HangWatcher::ProcessType::kGPUProcess);

  // Start a blocked thread and simulate a hang.
  BlockedThread thread(HangWatcher::ThreadType::kMainThread, base::Seconds(10));
  task_environment_.FastForwardBy(base::Seconds(11));

  // Hang reports are disabled by default on the GPU process.
  hang_watcher.TriggerSynchronousMonitoring();
  EXPECT_EQ(hang_watcher.GetHangCount(), 0);
}

// Tests that hang detection can be enabled on the GPU process.
TEST_F(HangWatcherTest, GpuProcessHangReportingCanBeEnabled) {
  ScopedFeatureList enable_hang_watcher;
  enable_hang_watcher.InitWithFeaturesAndParameters(
      {{kEnableHangWatcherOnGpuProcess,
        {{kGpuProcessMainThreadLogLevelParam, "2"}}}},
      {});
  ManualHangWatcher hang_watcher(HangWatcher::ProcessType::kGPUProcess);

  // Start a blocked thread and simulate a hang.
  BlockedThread thread(HangWatcher::ThreadType::kMainThread, base::Seconds(10));
  task_environment_.FastForwardBy(base::Seconds(11));

  // Hang reports are disabled by default on the GPU process.
  hang_watcher.TriggerSynchronousMonitoring();
  EXPECT_EQ(hang_watcher.GetHangCount(), 1);
}

// Test that a single hang gets recorded when multiple threads hung.
TEST_F(HangWatcherTest, SingleHangRecordedForMultipleThreads) {
  ScopedFeatureList enable_hang_watcher(kEnableHangWatcher);
  base::HistogramTester histogram_tester;

  ManualHangWatcher hang_watcher(HangWatcher::ProcessType::kBrowserProcess);

  // Start blocked threads for all thread types and simulate hangs.
  BlockedThreadsForAllTypes threads(base::Seconds(10));
  task_environment_.FastForwardBy(base::Seconds(11));

  // A single hang report should be sent, even though two threads hung.
  hang_watcher.TriggerSynchronousMonitoring();
  EXPECT_EQ(hang_watcher.GetHangCount(), 1);

  EXPECT_THAT(
      histogram_tester.GetAllSamplesForPrefix(
          "HangWatcher.IsThreadHung.BrowserProcess"),
      UnorderedElementsAre(
          Pair("HangWatcher.IsThreadHung.BrowserProcess.UIThread.Normal",
               BucketsAre(Bucket(true, /*count=*/1))),
          Pair("HangWatcher.IsThreadHung.BrowserProcess.IOThread.Normal",
               BucketsAre(Bucket(true, /*count=*/1)))));
}

TEST_F(HangWatcherTest, HangAlreadyRecorded) {
  ScopedFeatureList enable_hang_watcher(kEnableHangWatcher);
  ManualHangWatcher hang_watcher(HangWatcher::ProcessType::kBrowserProcess);

  // Start a blocked thread and simulate a hang.
  BlockedThread thread(HangWatcher::ThreadType::kMainThread, base::Seconds(10));
  task_environment_.FastForwardBy(base::Seconds(11));

  // First monitoring catches and records the hang.
  hang_watcher.TriggerSynchronousMonitoring();
  EXPECT_EQ(hang_watcher.GetHangCount(), 1);

  // Attempt capture again. Second monitoring does not record a new hang because
  // a hang that was already recorded is still live.
  hang_watcher.TriggerSynchronousMonitoring();
  EXPECT_EQ(hang_watcher.GetHangCount(), 1);
}

TEST_F(HangWatcherTest, NoHang) {
  ScopedFeatureList enable_hang_watcher(kEnableHangWatcher);
  ManualHangWatcher hang_watcher(HangWatcher::ProcessType::kBrowserProcess);

  // Start a blocked thread with a 10 seconds hang limit, but don't fastforward
  // time.
  BlockedThread thread(HangWatcher::ThreadType::kMainThread, base::Seconds(10));

  // No hang to catch so nothing is recorded.
  hang_watcher.TriggerSynchronousMonitoring();
  EXPECT_EQ(hang_watcher.GetHangCount(), 0);
}

class HangWatcherSnapshotTest : public testing::Test {
 protected:
  // Verify that a capture takes place and that at the time of the capture the
  // list of hung thread ids is correct.
  void TestIDList(ManualHangWatcher& hang_watcher, const std::string& id_list) {
    list_of_hung_thread_ids_during_capture_ = id_list;
    task_environment_.AdvanceClock(kSmallCPUQuantum);
    hang_watcher.TriggerSynchronousMonitoring();
    EXPECT_EQ(++reference_capture_count_, hang_watcher.GetHangCount());
  }

  // Verify that even if hang monitoring takes place no hangs are detected.
  void ExpectNoCapture(ManualHangWatcher& hang_watcher) {
    int old_capture_count = hang_watcher.GetHangCount();
    task_environment_.AdvanceClock(kSmallCPUQuantum);
    hang_watcher.TriggerSynchronousMonitoring();
    EXPECT_EQ(old_capture_count, hang_watcher.GetHangCount());
  }

  std::string ConcatenateThreadIds(
      const std::vector<base::PlatformThreadId>& ids) const {
    std::string result;
    constexpr char kSeparator{'|'};

    for (PlatformThreadId id : ids) {
      result += base::NumberToString(id.raw()) + kSeparator;
    }

    return result;
  }

  const PlatformThreadId test_thread_id_ = PlatformThread::CurrentId();

  // This is written to by the test main thread and read from the hang watching
  // thread. It does not need to be protected because access to it is
  // synchronized by always setting before triggering the execution of the
  // reading code through HangWatcher::SignalMonitorEventForTesting().
  std::string list_of_hung_thread_ids_during_capture_;

  // Increases at the same time as |hang_capture_count_| to test that capture
  // actually took place.
  int reference_capture_count_ = 0;

  base::test::ScopedFeatureList feature_list_{base::kEnableHangWatcher};

  // Used exclusively for MOCK_TIME.
  test::SingleThreadTaskEnvironment task_environment_{
      test::TaskEnvironment::TimeSource::MOCK_TIME};
};

// Verify that the hang capture fails when marking a thread for blocking fails.
// This simulates a WatchHangsInScope completing between the time the hang
// was detected and the time it is recorded which would create a non-actionable
// report.
TEST_F(HangWatcherSnapshotTest, NonActionableReport) {
  ManualHangWatcher hang_watcher(HangWatcher::ProcessType::kBrowserProcess);

  // Register the main test thread for hang watching.
  auto unregister_thread_closure =
      HangWatcher::RegisterThread(base::HangWatcher::ThreadType::kMainThread);
  {
    // Start a WatchHangsInScope that expires right away. Ensures that
    // the first monitor will detect a hang.
    WatchHangsInScope expires_instantly(base::TimeDelta{});

    internal::HangWatchState* current_hang_watch_state =
        internal::HangWatchState::GetHangWatchStateForCurrentThread();

    // Simulate the deadline changing concurrently during the capture. This
    // makes the capture fail since marking of the deadline fails.
    ASSERT_NE(current_hang_watch_state->GetDeadline(),
              base::TimeTicks::FromInternalValue(kArbitraryDeadline));
    current_hang_watch_state->GetHangWatchDeadlineForTesting()
        ->SetSwitchBitsClosureForTesting(
            base::BindLambdaForTesting([] { return kArbitraryDeadline; }));

    ExpectNoCapture(hang_watcher);

    // Marking failed.
    EXPECT_FALSE(current_hang_watch_state->IsFlagSet(
        internal::HangWatchDeadline::Flag::kShouldBlockOnHang));

    current_hang_watch_state->GetHangWatchDeadlineForTesting()
        ->ResetSwitchBitsClosureForTesting();
  }
}

TEST_F(HangWatcherSnapshotTest, HungThreadIDs) {
  ManualHangWatcher hang_watcher(HangWatcher::ProcessType::kBrowserProcess);

  // During hang capture the list of hung threads should be populated.
  // When hang capture is over the list should be empty.
  hang_watcher.SetOnHangClosure(base::BindLambdaForTesting([&] {
    EXPECT_EQ(hang_watcher.GetHungThreadListCrashKeyForTesting(),
              list_of_hung_thread_ids_during_capture_);
  }));

  // Register the main test thread for hang watching.
  auto unregister_thread_closure =
      HangWatcher::RegisterThread(base::HangWatcher::ThreadType::kMainThread);

  BlockedThread blocked_thread(HangWatcher::ThreadType::kMainThread,
                               /*timeout=*/base::TimeDelta{});
  {
    // Ensure the blocking thread entered the scope before the main thread. This
    // will guarantee an ordering while reporting the list of hung threads.
    task_environment_.AdvanceClock(kSmallCPUQuantum);

    // Start a WatchHangsInScope that expires right away. Ensures that
    // the first monitor will detect a hang. This scope will naturally have a
    // later deadline than the one in |blocked_thread_| since it was created
    // after.
    WatchHangsInScope expires_instantly(base::TimeDelta{});

    // Hung thread list should contain the id the blocking thread and then the
    // id of the test main thread since that is the order of increasing
    // deadline.
    TestIDList(hang_watcher,
               ConcatenateThreadIds({blocked_thread.GetId(), test_thread_id_}));

    // |expires_instantly| and the scope from |blocked_thread| are still live
    // but already recorded so should be ignored.
    ExpectNoCapture(hang_watcher);

    // Unblock and join the thread to close the scope in |blocked_thread|.
    blocked_thread.Unblock();
    blocked_thread.WaitDone();

    // |expires_instantly| is still live but already recorded so should be
    // ignored.
    ExpectNoCapture(hang_watcher);
  }

  // All HangWatchScopeEnables are over. There should be no capture.
  ExpectNoCapture(hang_watcher);

  // Once all recorded scopes are over creating a new one and monitoring will
  // trigger a hang detection.
  WatchHangsInScope expires_instantly(base::TimeDelta{});
  TestIDList(hang_watcher, ConcatenateThreadIds({test_thread_id_}));
}

TEST_F(HangWatcherSnapshotTest, TimeSinceLastSystemPowerResumeCrashKey) {
  ManualHangWatcher hang_watcher(HangWatcher::ProcessType::kBrowserProcess);

  // Override the capture of hangs. Simulate a crash key capture.
  std::string seconds_since_last_power_resume_crash_key;
  hang_watcher.SetOnHangClosure(base::BindLambdaForTesting([&] {
    seconds_since_last_power_resume_crash_key =
        hang_watcher.GetTimeSinceLastSystemPowerResumeCrashKeyValue();
  }));

  // Register the main test thread for hang watching.
  auto unregister_thread_closure =
      HangWatcher::RegisterThread(base::HangWatcher::ThreadType::kMainThread);

  {
    WatchHangsInScope expires_instantly(base::TimeDelta{});
    task_environment_.AdvanceClock(kSmallCPUQuantum);

    hang_watcher.TriggerSynchronousMonitoring();
    EXPECT_EQ(1, hang_watcher.GetHangCount());
    EXPECT_EQ("Never suspended", seconds_since_last_power_resume_crash_key);
  }

  {
    test::ScopedPowerMonitorTestSource power_monitor_source;
    power_monitor_source.Suspend();
    task_environment_.AdvanceClock(kSmallCPUQuantum);

    {
      WatchHangsInScope expires_instantly(base::TimeDelta{});
      task_environment_.AdvanceClock(kSmallCPUQuantum);
      hang_watcher.TriggerSynchronousMonitoring();
      EXPECT_EQ(2, hang_watcher.GetHangCount());
      EXPECT_EQ("Power suspended", seconds_since_last_power_resume_crash_key);
    }

    power_monitor_source.Resume();
    constexpr TimeDelta kAfterResumeTime{base::Seconds(5)};
    task_environment_.AdvanceClock(kAfterResumeTime);

    {
      WatchHangsInScope expires_instantly(base::TimeDelta{});
      hang_watcher.TriggerSynchronousMonitoring();
      EXPECT_EQ(3, hang_watcher.GetHangCount());
      EXPECT_EQ(base::NumberToString(kAfterResumeTime.InSeconds()),
                seconds_since_last_power_resume_crash_key);
    }
  }
}

// Determines how long the HangWatcher will wait between calls to
// Monitor(). Choose a low value so that that successive invocations happens
// fast. This makes tests that wait for monitoring run fast and makes tests that
// expect no monitoring fail fast.
const base::TimeDelta kMonitoringPeriod = base::Milliseconds(1);

// Test if and how often the HangWatcher periodically monitors for hangs.
class HangWatcherPeriodicMonitoringTest : public testing::Test {
 public:
  HangWatcherPeriodicMonitoringTest() {
    hang_watcher_.InitializeOnMainThread(
        HangWatcher::ProcessType::kBrowserProcess, /*emit_crashes=*/true);

    hang_watcher_.SetMonitoringPeriodForTesting(kMonitoringPeriod);
    hang_watcher_.SetOnHangClosureForTesting(base::BindRepeating(
        &WaitableEvent::Signal, base::Unretained(&hang_event_)));

    // HangWatcher uses a TickClock to detect how long it slept in between calls
    // to Monitor(). Override that clock to control its subjective passage of
    // time.
    hang_watcher_.SetTickClockForTesting(&test_clock_);
  }

  void TearDown() override {
    hang_watcher_.UninitializeOnMainThreadForTesting();
  }

 protected:
  // Setup the callback invoked after waiting in HangWatcher to advance the
  // tick clock by the desired time delta.
  void InstallAfterWaitCallback(base::TimeDelta time_delta) {
    hang_watcher_.SetAfterWaitCallbackForTesting(base::BindLambdaForTesting(
        [this, time_delta](base::TimeTicks time_before_wait) {
          test_clock_.Advance(time_delta);
        }));
  }

  base::SimpleTestTickClock test_clock_;

  // Single threaded to avoid ThreadPool WorkerThreads registering. Will run
  // delayed tasks created by the tests.
  test::SingleThreadTaskEnvironment task_environment_;

  HangWatcher hang_watcher_;

  // Signaled when a hang is detected.
  WaitableEvent hang_event_;

  base::ScopedClosureRunner unregister_thread_closure_;
};

// Don't register any threads for hang watching. HangWatcher should not monitor.
TEST_F(HangWatcherPeriodicMonitoringTest,
       NoPeriodicMonitoringWithoutRegisteredThreads) {
  RunLoop run_loop;

  // If a call to HangWatcher::Monitor() takes place the test will instantly
  // fail.
  hang_watcher_.SetAfterMonitorClosureForTesting(
      base::BindLambdaForTesting([&run_loop] {
        ADD_FAILURE() << "Monitoring took place!";
        run_loop.Quit();
      }));

  // Make the HangWatcher tick clock advance by exactly the monitoring period
  // after waiting so it will never detect oversleeping between attempts to call
  // Monitor(). This would inhibit monitoring and make the test pass for the
  // wrong reasons.
  InstallAfterWaitCallback(kMonitoringPeriod);

  hang_watcher_.Start();

  // Unblock the test thread. No thread ever registered after the HangWatcher
  // was created in the test's constructor. No monitoring should have taken
  // place.
  task_environment_.GetMainThreadTaskRunner()->PostDelayedTask(
      FROM_HERE, run_loop.QuitClosure(), TestTimeouts::tiny_timeout());
  run_loop.Run();

  // NOTE:
  // A lack of calls could technically also be caused by the HangWatcher thread
  // executing too slowly / being descheduled. This is a known limitation.
  // It's expected for |TestTimeouts::tiny_timeout()| to be large enough that
  // this is rare.
}

// During normal execution periodic monitoring should take place.
TEST_F(HangWatcherPeriodicMonitoringTest, PeriodicCallsTakePlace) {
  // HangWatcher::Monitor() will run once right away on thread registration.
  // We want to make sure it runs at a couple more times from being scheduled.
  constexpr int kMinimumMonitorCount = 3;

  RunLoop run_loop;

  // Setup the HangWatcher to unblock run_loop when the Monitor() has been
  // invoked enough times.
  hang_watcher_.SetAfterMonitorClosureForTesting(BarrierClosure(
      kMinimumMonitorCount, base::BindLambdaForTesting([&run_loop] {
        // This should only run if there are threads to watch.
        EXPECT_TRUE(base::FeatureList::IsEnabled(kEnableHangWatcher));

        // Test condition are confirmed, stop monitoring.
        HangWatcher::StopMonitoringForTesting();

        // Unblock the test main thread.
        run_loop.Quit();
      })));

  // Make the HangWatcher tick clock advance by exactly the monitoring period
  // after waiting so it will never detect oversleeping between attempts to call
  // Monitor(). This would inhibit monitoring.
  InstallAfterWaitCallback(kMonitoringPeriod);

  hang_watcher_.Start();

  // Register a thread,
  unregister_thread_closure_ =
      HangWatcher::RegisterThread(base::HangWatcher::ThreadType::kMainThread);

  // The "after monitor" closure only runs if there are threads to watch.
  if (base::FeatureList::IsEnabled(kEnableHangWatcher)) {
    run_loop.Run();
  }

  // No monitored scope means no possible hangs.
  EXPECT_FALSE(hang_event_.IsSignaled());
}

// If the HangWatcher detects it slept for longer than expected it will not
// monitor.
TEST_F(HangWatcherPeriodicMonitoringTest, NoMonitorOnOverSleep) {
  RunLoop run_loop;

  // If a call to HangWatcher::Monitor() takes place the test will instantly
  // fail.
  hang_watcher_.SetAfterMonitorClosureForTesting(
      base::BindLambdaForTesting([&run_loop] {
        ADD_FAILURE() << "Monitoring took place!";
        run_loop.Quit();
      }));

  // Make the HangWatcher tick clock advance so much after waiting that it will
  // detect oversleeping every time. This will keep it from monitoring.
  InstallAfterWaitCallback(base::Minutes(1));

  hang_watcher_.Start();

  // Register a thread.
  unregister_thread_closure_ =
      HangWatcher::RegisterThread(base::HangWatcher::ThreadType::kMainThread);

  // Unblock the test thread. All waits were perceived as oversleeping so all
  // monitoring was inhibited.
  task_environment_.GetMainThreadTaskRunner()->PostDelayedTask(
      FROM_HERE, run_loop.QuitClosure(), TestTimeouts::tiny_timeout());
  run_loop.Run();

  // NOTE: A lack of calls could technically also be caused by the HangWatcher
  // thread executing too slowly / being descheduled. This is a known
  // limitation. It's expected for |TestTimeouts::tiny_timeout()| to be large
  // enough that this happens rarely.
}

class WatchHangsInScopeBlockingTest : public testing::Test {
 public:
  WatchHangsInScopeBlockingTest() {
    HangWatcher::InitializeOnMainThread(
        HangWatcher::ProcessType::kBrowserProcess, /*emit_crashes=*/true);

    hang_watcher_.SetOnHangClosureForTesting(base::BindLambdaForTesting([&] {
      capture_started_.Signal();
      // Simulate capturing that takes a long time.
      PlatformThread::Sleep(base::Milliseconds(500));

      continue_capture_.Wait();
      completed_capture_ = true;
    }));

    hang_watcher_.SetAfterMonitorClosureForTesting(
        base::BindLambdaForTesting([&] {
          // Simulate monitoring that takes a long time.
          PlatformThread::Sleep(base::Milliseconds(500));
          completed_monitoring_.Signal();
        }));

    // Make sure no periodic monitoring takes place.
    hang_watcher_.SetMonitoringPeriodForTesting(kVeryLongDelta);

    hang_watcher_.Start();

    // Register the test main thread for hang watching.
    unregister_thread_closure_ =
        HangWatcher::RegisterThread(base::HangWatcher::ThreadType::kMainThread);
  }

  void TearDown() override {
    HangWatcher::UninitializeOnMainThreadForTesting();
  }

  void VerifyScopesDontBlock() {
    // Start a WatchHangsInScope that cannot possibly cause a hang to be
    // detected.
    {
      WatchHangsInScope long_scope(kVeryLongDelta);

      // Manually trigger a monitoring.
      hang_watcher_.SignalMonitorEventForTesting();

      // Execution has to continue freely here as no capture is in progress.
    }

    // Monitoring should not be over yet because the test code should execute
    // faster when not blocked.
    EXPECT_FALSE(completed_monitoring_.IsSignaled());

    // Wait for the full monitoring process to be complete. This is to prove
    // that monitoring truly executed and that we raced the signaling.
    completed_monitoring_.Wait();

    // No hang means no capture.
    EXPECT_FALSE(completed_capture_);
  }

 protected:
  base::WaitableEvent capture_started_;
  base::WaitableEvent completed_monitoring_;

  // The HangWatcher waits on this event via the "on hang" closure when a hang
  // is detected.
  base::WaitableEvent continue_capture_;
  bool completed_capture_{false};

  base::test::ScopedFeatureList feature_list_{base::kEnableHangWatcher};
  HangWatcher hang_watcher_;
  base::ScopedClosureRunner unregister_thread_closure_;
};

// Tests that execution is unimpeded by ~WatchHangsInScope() when no capture
// ever takes place.
TEST_F(WatchHangsInScopeBlockingTest, ScopeDoesNotBlocksWithoutCapture) {
  // No capture should take place so |continue_capture_| is not signaled to
  // create a test hang if one ever does.
  VerifyScopesDontBlock();
}

// Test that execution blocks in ~WatchHangsInScope() for a thread under
// watch during the capturing of a hang.
TEST_F(WatchHangsInScopeBlockingTest, ScopeBlocksDuringCapture) {
  // The capture completing is not dependent on any test event. Signal to make
  // sure the test is not blocked.
  continue_capture_.Signal();

  // Start a WatchHangsInScope that expires in the past already. Ensures
  // that the first monitor will detect a hang.
  {
    // Start a WatchHangsInScope that expires right away. Ensures that the
    // first monitor will detect a hang.
    WatchHangsInScope expires_right_away(base::TimeDelta{});

    // Manually trigger a monitoring.
    hang_watcher_.SignalMonitorEventForTesting();

    // Ensure that the hang capturing started.
    capture_started_.Wait();

    // Execution will get stuck in the outer scope because it can't escape
    // ~WatchHangsInScope() if a hang capture is under way.
  }

  // A hang was in progress so execution should have been blocked in
  // BlockWhileCaptureInProgress() until capture finishes.
  EXPECT_TRUE(completed_capture_);
  completed_monitoring_.Wait();

  // Reset expectations
  completed_monitoring_.Reset();
  capture_started_.Reset();
  completed_capture_ = false;

  // Verify that scopes don't block just because a capture happened in the past.
  VerifyScopesDontBlock();
}

#if BUILDFLAG(IS_MAC) && defined(ARCH_CPU_ARM64)
// Flaky hangs on arm64 Macs: https://crbug.com/1140207
#define MAYBE_NewScopeDoesNotBlockDuringCapture \
  DISABLED_NewScopeDoesNotBlockDuringCapture
#else
#define MAYBE_NewScopeDoesNotBlockDuringCapture \
  NewScopeDoesNotBlockDuringCapture
#endif

// Test that execution does not block in ~WatchHangsInScope() when the scope
// was created after the start of a capture.
TEST_F(WatchHangsInScopeBlockingTest, MAYBE_NewScopeDoesNotBlockDuringCapture) {
  // Start a WatchHangsInScope that expires right away. Ensures that the
  // first monitor will detect a hang.
  WatchHangsInScope expires_right_away(base::TimeDelta{});

  // Manually trigger a monitoring.
  hang_watcher_.SignalMonitorEventForTesting();

  // Ensure that the hang capturing started.
  capture_started_.Wait();

  // A scope started once a capture is already under way should not block
  // execution.
  { WatchHangsInScope also_expires_right_away(base::TimeDelta{}); }

  // Wait for the new WatchHangsInScope to be destroyed to let the capture
  // finish. If the new scope block waiting for the capture to finish this would
  // create a deadlock and the test would hang.
  continue_capture_.Signal();
}

}  // namespace

namespace internal {
namespace {

// Matcher validating that the specified `HangWatchDeadline` has no flag set.
MATCHER(HasNoFlagSet, /*description=*/"") {
  static constexpr auto kAllFlags =
      base::MakeFixedFlatMap<HangWatchDeadline::Flag, std::string_view>({
          {HangWatchDeadline::Flag::kMinValue, "kMinValue"},
          {HangWatchDeadline::Flag::kIgnoreCurrentWatchHangsInScope,
           "kIgnoreCurrentWatchHangsInScope"},
          {HangWatchDeadline::Flag::kShouldBlockOnHang, "kShouldBlockOnHang"},
      });

  for (const auto& [flag, description] : kAllFlags) {
    if (arg.IsFlagSet(flag)) {
      *result_listener << "where flag " << description << " is set";
      return false;
    }
  }
  return true;
}

class HangWatchDeadlineTest : public testing::Test {
 protected:
  // Return a flag mask without one of the flags for test purposes. Use to
  // ignore that effect of setting a flag that was just set.
  uint64_t FlagsMinus(uint64_t flags, HangWatchDeadline::Flag flag) {
    return flags & ~(static_cast<uint64_t>(flag));
  }

  HangWatchDeadline deadline_;
};

}  // namespace

// Verify that the extract functions don't mangle any bits.
TEST_F(HangWatchDeadlineTest, BitsPreservedThroughExtract) {
  for (auto bits : {kAllOnes, kAllZeros, kOnesThenZeroes, kZeroesThenOnes}) {
    EXPECT_TRUE((HangWatchDeadline::ExtractFlags(bits) |
                 HangWatchDeadline::ExtractDeadline(bits)) == bits);
  }
}

namespace {

// Verify that setting and clearing a persistent flag works and has no unwanted
// side-effects. Neither the flags nor the deadline change concurrently in this
// test.
TEST_F(HangWatchDeadlineTest, SetAndClearPersistentFlag) {
  ASSERT_THAT(deadline_, HasNoFlagSet());

  // Grab the original values for flags and deadline.
  auto [old_flags, old_deadline] = deadline_.GetFlagsAndDeadline();

  // Set the flag. Operation cannot fail.
  deadline_.SetIgnoreCurrentWatchHangsInScope();

  // Get new flags and deadline.
  auto [new_flags, new_deadline] = deadline_.GetFlagsAndDeadline();

  // Flag was set properly.
  EXPECT_TRUE(HangWatchDeadline::IsFlagSet(
      HangWatchDeadline::Flag::kIgnoreCurrentWatchHangsInScope, new_flags));

  // No side-effect on deadline.
  EXPECT_EQ(new_deadline, old_deadline);

  // No side-effect on other flags.
  EXPECT_EQ(
      FlagsMinus(new_flags,
                 HangWatchDeadline::Flag::kIgnoreCurrentWatchHangsInScope),
      old_flags);

  // Clear the flag, operation cannot fail.
  deadline_.UnsetIgnoreCurrentWatchHangsInScope();

  // Update new values.
  std::tie(new_flags, new_deadline) = deadline_.GetFlagsAndDeadline();

  // All flags back to original state.
  EXPECT_EQ(new_flags, old_flags);

  // Deadline still unaffected.
  EXPECT_EQ(new_deadline, old_deadline);
}

// Verify setting the TimeTicks value works and has no unwanted side-effects.
TEST_F(HangWatchDeadlineTest, SetDeadline) {
  TimeTicks ticks;

  ASSERT_THAT(deadline_, HasNoFlagSet());
  ASSERT_NE(deadline_.GetDeadline(), ticks);

  // Set the deadline and verify it stuck.
  deadline_.SetDeadline(ticks);
  EXPECT_EQ(deadline_.GetDeadline(), ticks);

  // Only the value was modified, no flags should be set.
  EXPECT_THAT(deadline_, HasNoFlagSet());
}

// Verify that setting a non-persistent flag (kShouldBlockOnHang)
// when the TimeTicks value changed since calling the flag setting
// function fails and has no side-effects.
TEST_F(HangWatchDeadlineTest, SetShouldBlockOnHangDeadlineChanged) {
  ASSERT_THAT(deadline_, HasNoFlagSet());

  auto [flags, deadline] = deadline_.GetFlagsAndDeadline();

  // Simulate value change. Flags are constant.
  const base::TimeTicks new_deadline =
      base::TimeTicks::FromInternalValue(kArbitraryDeadline);
  ASSERT_NE(deadline, new_deadline);
  deadline_.SetSwitchBitsClosureForTesting(
      base::BindLambdaForTesting([] { return kArbitraryDeadline; }));

  // kShouldBlockOnHangs does not persist through value change.
  EXPECT_FALSE(deadline_.SetShouldBlockOnHang(flags, deadline));

  // Flag was not applied.
  EXPECT_FALSE(
      deadline_.IsFlagSet(HangWatchDeadline::Flag::kShouldBlockOnHang));

  // New value that was changed concurrently is preserved.
  EXPECT_EQ(deadline_.GetDeadline(), new_deadline);
}

// Verify that clearing a persistent (kIgnoreCurrentWatchHangsInScope) when
// the value changed succeeds and has non side-effects.
TEST_F(HangWatchDeadlineTest, ClearIgnoreHangsDeadlineChanged) {
  ASSERT_THAT(deadline_, HasNoFlagSet());

  auto [flags, deadline] = deadline_.GetFlagsAndDeadline();

  deadline_.SetIgnoreCurrentWatchHangsInScope();
  std::tie(flags, deadline) = deadline_.GetFlagsAndDeadline();
  ASSERT_TRUE(HangWatchDeadline::IsFlagSet(
      HangWatchDeadline::Flag::kIgnoreCurrentWatchHangsInScope, flags));

  // Simulate deadline change. Flags are constant.
  const base::TimeTicks new_deadline =
      base::TimeTicks::FromInternalValue(kArbitraryDeadline);
  ASSERT_NE(deadline, new_deadline);
  deadline_.SetSwitchBitsClosureForTesting(base::BindLambdaForTesting([] {
    return static_cast<uint64_t>(HangWatchDeadline::Flag::kShouldBlockOnHang) |
           kArbitraryDeadline;
  }));

  // Clearing kIgnoreHang is unaffected by deadline or flags change.
  deadline_.UnsetIgnoreCurrentWatchHangsInScope();
  EXPECT_FALSE(deadline_.IsFlagSet(
      HangWatchDeadline::Flag::kIgnoreCurrentWatchHangsInScope));

  // New deadline that was changed concurrently is preserved.
  EXPECT_TRUE(deadline_.IsFlagSet(HangWatchDeadline::Flag::kShouldBlockOnHang));
  EXPECT_EQ(deadline_.GetDeadline(), new_deadline);
}

// Verify that setting a persistent (kIgnoreCurrentWatchHangsInScope) when
// the deadline or flags changed succeeds and has non side-effects.
TEST_F(HangWatchDeadlineTest,
       SetIgnoreCurrentHangWatchScopeEnableDeadlineChanged) {
  ASSERT_THAT(deadline_, HasNoFlagSet());

  auto [flags, deadline] = deadline_.GetFlagsAndDeadline();

  // Simulate deadline change. Flags are constant.
  const base::TimeTicks new_deadline =
      base::TimeTicks::FromInternalValue(kArbitraryDeadline);

  ASSERT_NE(deadline, new_deadline);
  deadline_.SetSwitchBitsClosureForTesting(base::BindLambdaForTesting([] {
    return static_cast<uint64_t>(HangWatchDeadline::Flag::kShouldBlockOnHang) |
           kArbitraryDeadline;
  }));

  // kIgnoreHang persists through value change.
  deadline_.SetIgnoreCurrentWatchHangsInScope();
  EXPECT_TRUE(deadline_.IsFlagSet(
      HangWatchDeadline::Flag::kIgnoreCurrentWatchHangsInScope));

  // New deadline and flags that changed concurrently are preserved.
  EXPECT_TRUE(deadline_.IsFlagSet(HangWatchDeadline::Flag::kShouldBlockOnHang));
  EXPECT_EQ(deadline_.GetDeadline(), new_deadline);
}

// Setting a new deadline should wipe flags that a not persistent.
// Persistent flags should not be disturbed.
TEST_F(HangWatchDeadlineTest, SetDeadlineWipesFlags) {
  auto [flags, deadline] = deadline_.GetFlagsAndDeadline();

  ASSERT_TRUE(deadline_.SetShouldBlockOnHang(flags, deadline));
  ASSERT_TRUE(deadline_.IsFlagSet(HangWatchDeadline::Flag::kShouldBlockOnHang));

  std::tie(flags, deadline) = deadline_.GetFlagsAndDeadline();

  deadline_.SetIgnoreCurrentWatchHangsInScope();
  ASSERT_TRUE(deadline_.IsFlagSet(
      HangWatchDeadline::Flag::kIgnoreCurrentWatchHangsInScope));

  // Change the deadline.
  deadline_.SetDeadline(TimeTicks{});
  EXPECT_EQ(deadline_.GetDeadline(), TimeTicks{});

  // Verify the persistent flag stuck and the non-persistent one was unset.
  EXPECT_FALSE(
      deadline_.IsFlagSet(HangWatchDeadline::Flag::kShouldBlockOnHang));
  EXPECT_TRUE(deadline_.IsFlagSet(
      HangWatchDeadline::Flag::kIgnoreCurrentWatchHangsInScope));
}

}  // namespace
}  // namespace internal
}  // namespace base
