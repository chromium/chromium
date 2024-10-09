// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/threading/hang_watcher.h"

#include <atomic>
#include <memory>
#include <optional>

#include "base/barrier_closure.h"
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

using testing::ElementsAre;
using testing::IsEmpty;

namespace base {
namespace {

// Use with a FeatureList to activate crash dumping for threads marked as
// threadpool threads.
const std::vector<base::test::FeatureRefAndParams> kFeatureAndParams{
    {base::kEnableHangWatcher, {{"ui_thread_log_level", "2"}}}};

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

#if BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_CHROMEOS)
class HangWatcherEnabledInZygoteChildTest
    : public testing::TestWithParam<std::tuple<bool, bool>> {
 public:
  HangWatcherEnabledInZygoteChildTest() {
    std::vector<base::test::FeatureRefAndParams> enabled_features =
        kFeatureAndParams;
    std::vector<test::FeatureRef> disabled_features;
    if (std::get<0>(GetParam())) {
      enabled_features.push_back(test::FeatureRefAndParams(
          base::kEnableHangWatcherInZygoteChildren, {}));
    } else {
      disabled_features.push_back(base::kEnableHangWatcherInZygoteChildren);
    }
    feature_list_.InitWithFeaturesAndParameters(enabled_features,
                                                disabled_features);
    HangWatcher::InitializeOnMainThread(
        HangWatcher::ProcessType::kUtilityProcess,
        /*is_zygote_child=*/std::get<1>(GetParam()),
        /*emit_crashes=*/true);
  }

  void TearDown() override { HangWatcher::UnitializeOnMainThreadForTesting(); }

  HangWatcherEnabledInZygoteChildTest(
      const HangWatcherEnabledInZygoteChildTest& other) = delete;
  HangWatcherEnabledInZygoteChildTest& operator=(
      const HangWatcherEnabledInZygoteChildTest& other) = delete;

 protected:
  base::test::ScopedFeatureList feature_list_;
};

TEST_P(HangWatcherEnabledInZygoteChildTest, IsEnabled) {
  // If the kEnableHangWatcherInZygoteChildren feature is disabled and
  // InitializeOnMainThread is called with is_zygote_child==true, IsEnabled()
  // should return false. It should return true in all other situations.
  ASSERT_EQ(std::get<0>(GetParam()) || !std::get<1>(GetParam()),
            HangWatcher::IsEnabled());
}

INSTANTIATE_TEST_SUITE_P(HangWatcherZygoteTest,
                         HangWatcherEnabledInZygoteChildTest,
                         testing::Combine(testing::Bool(), testing::Bool()));
#endif  // BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_CHROMEOS)

// Waits on provided WaitableEvent before executing and signals when done.
class BlockingThread : public DelegateSimpleThread::Delegate {
 public:
  explicit BlockingThread(base::WaitableEvent* unblock_thread,
                          base::TimeDelta timeout)
      : thread_(this, "BlockingThread"),
        unblock_thread_(unblock_thread),
        timeout_(timeout) {}

  ~BlockingThread() override = default;

  void Run() override {
    // (Un)Register the thread here instead of in ctor/dtor so that the action
    // happens on the right thread.
    base::ScopedClosureRunner unregister_closure =
        base::HangWatcher::RegisterThread(
            base::HangWatcher::ThreadType::kMainThread);

    WatchHangsInScope scope(timeout_);
    wait_until_entered_scope_.Signal();

    unblock_thread_->Wait();
    run_event_.Signal();
  }

  bool IsDone() { return run_event_.IsSignaled(); }

  void StartAndWaitForScopeEntered() {
    thread_.Start();
    // Block until this thread registered itself for hang watching and has
    // entered a WatchHangsInScope.
    wait_until_entered_scope_.Wait();
  }

  void Join() { thread_.Join(); }

  PlatformThreadId GetId() { return thread_.tid(); }

 private:
  base::DelegateSimpleThread thread_;

  // Will be signaled once the thread is properly registered for watching and
  // the WatchHangsInScope has been entered.
  WaitableEvent wait_until_entered_scope_;

  // Will be signaled once ThreadMain has run.
  WaitableEvent run_event_;

  const raw_ptr<base::WaitableEvent> unblock_thread_;

  base::TimeDelta timeout_;
};

class HangWatcherTest : public testing::Test {
 public:
  const base::TimeDelta kTimeout = base::Seconds(10);
  const base::TimeDelta kHangTime = kTimeout + base::Seconds(1);

  HangWatcherTest() {
    feature_list_.InitWithFeaturesAndParameters(kFeatureAndParams, {});
    HangWatcher::InitializeOnMainThread(
        HangWatcher::ProcessType::kBrowserProcess, false,
        /*emit_crashes=*/true);

    hang_watcher_.SetAfterMonitorClosureForTesting(base::BindRepeating(
        &WaitableEvent::Signal, base::Unretained(&monitor_event_)));

    hang_watcher_.SetOnHangClosureForTesting(base::BindRepeating(
        &WaitableEvent::Signal, base::Unretained(&hang_event_)));

    // We're not testing the monitoring loop behavior in this test so we want to
    // trigger monitoring manually.
    hang_watcher_.SetMonitoringPeriodForTesting(kVeryLongDelta);

    // Start the monitoring loop.
    hang_watcher_.Start();
  }

  void TearDown() override { HangWatcher::UnitializeOnMainThreadForTesting(); }

  HangWatcherTest(const HangWatcherTest& other) = delete;
  HangWatcherTest& operator=(const HangWatcherTest& other) = delete;

 protected:
  // Used to wait for monitoring. Will be signaled by the HangWatcher thread and
  // so needs to outlive it.
  WaitableEvent monitor_event_;

  // Signaled from the HangWatcher thread when a hang is detected. Needs to
  // outlive the HangWatcher thread.
  WaitableEvent hang_event_;

  base::test::ScopedFeatureList feature_list_;

  // Used exclusively for MOCK_TIME. No tasks will be run on the environment.
  // Single threaded to avoid ThreadPool WorkerThreads registering.
  test::SingleThreadTaskEnvironment task_environment_{
      test::TaskEnvironment::TimeSource::MOCK_TIME};

  // This must be declared last (after task_environment_, for example) so that
  // the watcher thread is joined before objects like the mock timer are
  // destroyed, causing racy crashes.
  HangWatcher hang_watcher_;
};

class HangWatcherBlockingThreadTest : public HangWatcherTest {
 public:
  HangWatcherBlockingThreadTest() : thread_(&unblock_thread_, kTimeout) {}

  HangWatcherBlockingThreadTest(const HangWatcherBlockingThreadTest& other) =
      delete;
  HangWatcherBlockingThreadTest& operator=(
      const HangWatcherBlockingThreadTest& other) = delete;

 protected:
  void JoinThread() {
    unblock_thread_.Signal();

    // Thread is joinable since we signaled |unblock_thread_|.
    thread_.Join();

    // If thread is done then it signaled.
    ASSERT_TRUE(thread_.IsDone());
  }

  void StartBlockedThread() {
    // Thread has not run yet.
    ASSERT_FALSE(thread_.IsDone());

    // Start the thread. It will block since |unblock_thread_| was not
    // signaled yet.
    thread_.StartAndWaitForScopeEntered();

    // Thread registration triggered a call to HangWatcher::Monitor() which
    // signaled |monitor_event_|. Reset it so it's ready for waiting later on.
    monitor_event_.Reset();
  }

  void MonitorHangs() {
    // HangWatcher::Monitor() should not be set which would mean a call to
    // HangWatcher::Monitor() happened and was unacounted for.
    // ASSERT_FALSE(monitor_event_.IsSignaled());

    // Trigger a monitoring on HangWatcher thread and verify results.
    hang_watcher_.SignalMonitorEventForTesting();
    monitor_event_.Wait();
  }

  // Used to unblock the monitored thread. Signaled from the test main thread.
  WaitableEvent unblock_thread_;

  BlockingThread thread_;
};
}  // namespace

TEST_F(HangWatcherTest, InvalidatingExpectationsPreventsCapture) {
  // Register the main test thread for hang watching.
  auto unregister_thread_closure =
      HangWatcher::RegisterThread(base::HangWatcher::ThreadType::kMainThread);

  // Create a hang.
  WatchHangsInScope expires_instantly(base::TimeDelta{});
  task_environment_.FastForwardBy(kHangTime);

  // de-activate hang watching,
  base::HangWatcher::InvalidateActiveExpectations();

  // Trigger a monitoring on HangWatcher thread and verify results.
  // Hang is not detected.
  hang_watcher_.SignalMonitorEventForTesting();
  monitor_event_.Wait();
  ASSERT_FALSE(hang_event_.IsSignaled());
}

TEST_F(HangWatcherTest, MultipleInvalidateExpectationsDoNotCancelOut) {
  // Register the main test thread for hang watching.
  auto unregister_thread_closure =
      HangWatcher::RegisterThread(base::HangWatcher::ThreadType::kMainThread);

  // Create a hang.
  WatchHangsInScope expires_instantly(base::TimeDelta{});
  task_environment_.FastForwardBy(kHangTime);

  // de-activate hang watching,
  base::HangWatcher::InvalidateActiveExpectations();

  // Redundently de-activate hang watching.
  base::HangWatcher::InvalidateActiveExpectations();

  // Trigger a monitoring on HangWatcher thread and verify results.
  // Hang is not detected.
  hang_watcher_.SignalMonitorEventForTesting();
  monitor_event_.Wait();
  ASSERT_FALSE(hang_event_.IsSignaled());
}

TEST_F(HangWatcherTest, NewInnerWatchHangsInScopeAfterInvalidationDetectsHang) {
  // Register the main test thread for hang watching.
  auto unregister_thread_closure =
      HangWatcher::RegisterThread(base::HangWatcher::ThreadType::kMainThread);

  WatchHangsInScope expires_instantly(base::TimeDelta{});
  task_environment_.FastForwardBy(kHangTime);

  // De-activate hang watching.
  base::HangWatcher::InvalidateActiveExpectations();

  {
    WatchHangsInScope also_expires_instantly(base::TimeDelta{});
    task_environment_.FastForwardBy(kHangTime);

    // Trigger a monitoring on HangWatcher thread and verify results.
    hang_watcher_.SignalMonitorEventForTesting();
    monitor_event_.Wait();

    // Hang is detected since the new WatchHangsInScope temporarily
    // re-activated hang_watching.
    monitor_event_.Wait();
    ASSERT_TRUE(hang_event_.IsSignaled());
  }

  // Reset to attempt capture again.
  monitor_event_.Reset();
  hang_event_.Reset();

  // Trigger a monitoring on HangWatcher thread and verify results.
  hang_watcher_.SignalMonitorEventForTesting();
  monitor_event_.Wait();

  // Hang is not detected since execution is back to being covered by
  // |expires_instantly| for which expectations were invalidated.
  monitor_event_.Wait();
  ASSERT_FALSE(hang_event_.IsSignaled());
}

TEST_F(HangWatcherTest,
       NewSeparateWatchHangsInScopeAfterInvalidationDetectsHang) {
  // Register the main test thread for hang watching.
  auto unregister_thread_closure =
      HangWatcher::RegisterThread(base::HangWatcher::ThreadType::kMainThread);

  {
    WatchHangsInScope expires_instantly(base::TimeDelta{});
    task_environment_.FastForwardBy(kHangTime);

    // De-activate hang watching.
    base::HangWatcher::InvalidateActiveExpectations();
  }

  WatchHangsInScope also_expires_instantly(base::TimeDelta{});
  task_environment_.FastForwardBy(kHangTime);

  // Trigger a monitoring on HangWatcher thread and verify results.
  hang_watcher_.SignalMonitorEventForTesting();
  monitor_event_.Wait();

  // Hang is detected since the new WatchHangsInScope did not have its
  // expectations invalidated.
  monitor_event_.Wait();
  ASSERT_TRUE(hang_event_.IsSignaled());
}

// Test that invalidating expectations from inner WatchHangsInScope will also
// prevent hang detection in outer scopes.
TEST_F(HangWatcherTest, ScopeDisabledObjectInnerScope) {
  // Register the main test thread for hang watching.
  auto unregister_thread_closure =
      HangWatcher::RegisterThread(base::HangWatcher::ThreadType::kMainThread);

  // Start a WatchHangsInScope that expires right away. Then advance
  // time to make sure no hang is detected.
  WatchHangsInScope expires_instantly(base::TimeDelta{});
  task_environment_.FastForwardBy(kHangTime);
  {
    WatchHangsInScope also_expires_instantly(base::TimeDelta{});

    // De-activate hang watching.
    base::HangWatcher::InvalidateActiveExpectations();
    task_environment_.FastForwardBy(kHangTime);
  }

  // Trigger a monitoring on HangWatcher thread and verify results.
  hang_watcher_.SignalMonitorEventForTesting();
  monitor_event_.Wait();

  // Hang is ignored since it concerns a scope for which one of the inner scope
  // was ignored.
  ASSERT_FALSE(hang_event_.IsSignaled());
}

TEST_F(HangWatcherTest, NewScopeAfterDisabling) {
  // Register the main test thread for hang watching.
  auto unregister_thread_closure =
      HangWatcher::RegisterThread(base::HangWatcher::ThreadType::kMainThread);

  // Start a WatchHangsInScope that expires right away. Then advance
  // time to make sure no hang is detected.
  WatchHangsInScope expires_instantly(base::TimeDelta{});
  task_environment_.FastForwardBy(kHangTime);
  {
    WatchHangsInScope also_expires_instantly(base::TimeDelta{});

    // De-activate hang watching.
    base::HangWatcher::InvalidateActiveExpectations();
    task_environment_.FastForwardBy(kHangTime);
  }

  // New scope for which expecations are never invalidated.
  WatchHangsInScope also_expires_instantly(base::TimeDelta{});
  task_environment_.FastForwardBy(kHangTime);

  // Trigger a monitoring on HangWatcher thread and verify results.
  hang_watcher_.SignalMonitorEventForTesting();
  monitor_event_.Wait();

  // Hang is detected because it's unrelated to the hangs that were disabled.
  ASSERT_TRUE(hang_event_.IsSignaled());
}

TEST_F(HangWatcherTest, NestedScopes) {
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
    ASSERT_FALSE(current_hang_watch_state->IsOverDeadline());
    ASSERT_EQ(current_hang_watch_state->GetDeadline(), first_deadline);
    {
      // Set a yet more restrictive deadline. Still no hang.
      WatchHangsInScope second_scope(kSecondTimeout);
      ASSERT_FALSE(current_hang_watch_state->IsOverDeadline());
      ASSERT_EQ(current_hang_watch_state->GetDeadline(), second_deadline);
    }
    // First deadline we set should be restored.
    ASSERT_FALSE(current_hang_watch_state->IsOverDeadline());
    ASSERT_EQ(current_hang_watch_state->GetDeadline(), first_deadline);
  }

  // Original deadline should now be restored.
  ASSERT_FALSE(current_hang_watch_state->IsOverDeadline());
  ASSERT_EQ(current_hang_watch_state->GetDeadline(), original_deadline);
}

TEST_F(HangWatcherBlockingThreadTest, HistogramsLoggedOnHang) {
  base::HistogramTester histogram_tester;
  StartBlockedThread();

  // Simulate hang.
  task_environment_.FastForwardBy(kHangTime);

  // First monitoring catches the hang and emits the histogram.
  MonitorHangs();
  EXPECT_THAT(histogram_tester.GetAllSamples("HangWatcher.IsThreadHung."
                                             "BrowserProcess.UIThread.Normal"),
              ElementsAre(base::Bucket(true, /*count=*/1)));

  // Reset to attempt capture again.
  hang_event_.Reset();
  monitor_event_.Reset();

  // Hang is logged again even if it would not trigger a crash dump.
  MonitorHangs();
  EXPECT_THAT(histogram_tester.GetAllSamples("HangWatcher.IsThreadHung."
                                             "BrowserProcess.UIThread.Normal"),
              ElementsAre(base::Bucket(true, /*count=*/2)));

  // Thread types that are not monitored should not get any samples.
  EXPECT_THAT(histogram_tester.GetAllSamples("HangWatcher.IsThreadHung."
                                             "BrowserProcess.IOThread.Normal"),
              IsEmpty());

  // No shutdown hangs, either.
  EXPECT_THAT(histogram_tester.GetAllSamples(
                  "HangWatcher.IsThreadHung.BrowserProcess.UIThread.Shutdown"),
              IsEmpty());
  EXPECT_THAT(histogram_tester.GetAllSamples(
                  "HangWatcher.IsThreadHung.BrowserProcess.IOThread.Shutdown"),
              IsEmpty());

  JoinThread();
}

TEST_F(HangWatcherBlockingThreadTest, HistogramsLoggedWithoutHangs) {
  base::HistogramTester histogram_tester;
  StartBlockedThread();

  // No hang to catch so nothing is recorded.
  MonitorHangs();
  ASSERT_FALSE(hang_event_.IsSignaled());

  // A thread of type ThreadForTesting was monitored but didn't hang. This is
  // logged.
  EXPECT_THAT(histogram_tester.GetAllSamples("HangWatcher.IsThreadHung."
                                             "BrowserProcess.UIThread.Normal"),
              ElementsAre(base::Bucket(false, /*count=*/1)));

  // Thread types that are not monitored should not get any samples.
  EXPECT_THAT(histogram_tester.GetAllSamples("HangWatcher.IsThreadHung."
                                             "BrowserProcess.IOThread.Normal"),
              IsEmpty());
  JoinThread();
}

TEST_F(HangWatcherBlockingThreadTest, HistogramsLoggedWithShutdownFlag) {
  base::HistogramTester histogram_tester;
  StartBlockedThread();

  // Simulate hang.
  task_environment_.FastForwardBy(kHangTime);

  // Make this process emit *.Shutdown instead of *.Normal histograms.
  base::HangWatcher::SetShuttingDown();

  // First monitoring catches the hang and emits the histogram.
  MonitorHangs();
  EXPECT_THAT(histogram_tester.GetAllSamples(
                  "HangWatcher.IsThreadHung.BrowserProcess.UIThread.Shutdown"),
              ElementsAre(base::Bucket(true, /*count=*/1)));

  // Reset to attempt capture again.
  hang_event_.Reset();
  monitor_event_.Reset();

  // Hang is logged again even if it would not trigger a crash dump.
  MonitorHangs();
  EXPECT_THAT(histogram_tester.GetAllSamples(
                  "HangWatcher.IsThreadHung.BrowserProcess.UIThread.Shutdown"),
              ElementsAre(base::Bucket(true, /*count=*/2)));

  // Thread types that are not monitored should not get any samples.
  EXPECT_THAT(histogram_tester.GetAllSamples(
                  "HangWatcher.IsThreadHung.BrowserProcess.IOThread.Shutdown"),
              IsEmpty());

  // No normal hangs.
  EXPECT_THAT(histogram_tester.GetAllSamples(
                  "HangWatcher.IsThreadHung.BrowserProcess.UIThread.Normal"),
              IsEmpty());
  EXPECT_THAT(histogram_tester.GetAllSamples(
                  "HangWatcher.IsThreadHung.BrowserProcess.IOThread.Normal"),
              IsEmpty());

  JoinThread();
}

TEST_F(HangWatcherBlockingThreadTest, Hang) {
  StartBlockedThread();

  // Simulate hang.
  task_environment_.FastForwardBy(kHangTime);

  // First monitoring catches and records the hang.
  MonitorHangs();
  ASSERT_TRUE(hang_event_.IsSignaled());

  JoinThread();
}

TEST_F(HangWatcherBlockingThreadTest, HangAlreadyRecorded) {
  StartBlockedThread();

  // Simulate hang.
  task_environment_.FastForwardBy(kHangTime);

  // First monitoring catches and records the hang.
  MonitorHangs();
  ASSERT_TRUE(hang_event_.IsSignaled());

  // Reset to attempt capture again.
  hang_event_.Reset();
  monitor_event_.Reset();

  // Second monitoring does not record because a hang that was already recorded
  // is still live.
  MonitorHangs();
  ASSERT_FALSE(hang_event_.IsSignaled());

  JoinThread();
}

TEST_F(HangWatcherBlockingThreadTest, NoHang) {
  StartBlockedThread();

  // No hang to catch so nothing is recorded.
  MonitorHangs();
  ASSERT_FALSE(hang_event_.IsSignaled());

  JoinThread();
}

namespace {
class HangWatcherSnapshotTest : public testing::Test {
 public:
  void SetUp() override {
    feature_list_.InitWithFeaturesAndParameters(kFeatureAndParams, {});
    HangWatcher::InitializeOnMainThread(
        HangWatcher::ProcessType::kBrowserProcess, false,
        /*emit_crashes=*/true);

    // The monitoring loop behavior is not verified in this test so we want to
    // trigger monitoring manually.
    hang_watcher_.SetMonitoringPeriodForTesting(kVeryLongDelta);
  }

  void TearDown() override { HangWatcher::UnitializeOnMainThreadForTesting(); }

  HangWatcherSnapshotTest() = default;
  HangWatcherSnapshotTest(const HangWatcherSnapshotTest& other) = delete;
  HangWatcherSnapshotTest& operator=(const HangWatcherSnapshotTest& other) =
      delete;

 protected:
  void TriggerMonitorAndWaitForCompletion() {
    monitor_event_.Reset();
    hang_watcher_.SignalMonitorEventForTesting();
    monitor_event_.Wait();
  }

  // Verify that a capture takes place and that at the time of the capture the
  // list of hung thread ids is correct.
  void TestIDList(const std::string& id_list) {
    list_of_hung_thread_ids_during_capture_ = id_list;
    task_environment_.AdvanceClock(kSmallCPUQuantum);
    TriggerMonitorAndWaitForCompletion();
    ASSERT_EQ(++reference_capture_count_, hang_capture_count_);
  }

  // Verify that even if hang monitoring takes place no hangs are detected.
  void ExpectNoCapture() {
    int old_capture_count = hang_capture_count_;
    task_environment_.AdvanceClock(kSmallCPUQuantum);
    TriggerMonitorAndWaitForCompletion();
    ASSERT_EQ(old_capture_count, hang_capture_count_);
  }

  std::string ConcatenateThreadIds(
      const std::vector<base::PlatformThreadId>& ids) const {
    std::string result;
    constexpr char kSeparator{'|'};

    for (PlatformThreadId id : ids) {
      result += base::NumberToString(id) + kSeparator;
    }

    return result;
  }

  // Will be signaled once monitoring took place. Marks the end of the test.
  WaitableEvent monitor_event_;

  const PlatformThreadId test_thread_id_ = PlatformThread::CurrentId();

  // This is written to by the test main thread and read from the hang watching
  // thread. It does not need to be protected because access to it is
  // synchronized by always setting before triggering the execution of the
  // reading code through HangWatcher::SignalMonitorEventForTesting().
  std::string list_of_hung_thread_ids_during_capture_;

  // This is written to by from the hang watching thread and read the test main
  // thread. It does not need to be protected because access to it is
  // synchronized by always reading  after monitor_event_ has been signaled.
  int hang_capture_count_ = 0;

  // Increases at the same time as |hang_capture_count_| to test that capture
  // actually took place.
  int reference_capture_count_ = 0;

  std::string seconds_since_last_power_resume_crash_key_;

  base::test::ScopedFeatureList feature_list_;

  // Used exclusively for MOCK_TIME.
  test::SingleThreadTaskEnvironment task_environment_{
      test::TaskEnvironment::TimeSource::MOCK_TIME};

  HangWatcher hang_watcher_;
};
}  // namespace

// Verify that the hang capture fails when marking a thread for blocking fails.
// This simulates a WatchHangsInScope completing between the time the hang
// was dected and the time it is recorded which would create a non-actionable
// report.
TEST_F(HangWatcherSnapshotTest, NonActionableReport) {
  hang_watcher_.SetOnHangClosureForTesting(
      base::BindLambdaForTesting([this] { ++hang_capture_count_; }));
  hang_watcher_.SetAfterMonitorClosureForTesting(
      base::BindLambdaForTesting([this] { monitor_event_.Signal(); }));

  hang_watcher_.Start();

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

    ExpectNoCapture();

    // Marking failed.
    ASSERT_FALSE(current_hang_watch_state->IsFlagSet(
        internal::HangWatchDeadline::Flag::kShouldBlockOnHang));

    current_hang_watch_state->GetHangWatchDeadlineForTesting()
        ->ResetSwitchBitsClosureForTesting();
  }
}

// TODO(crbug.com/40187449): On MAC, the base::PlatformThread::CurrentId(...)
// should return the system wide IDs. The HungThreadIDs test fails because the
// reported process ids do not match.
#if BUILDFLAG(IS_MAC)
#define MAYBE_HungThreadIDs DISABLED_HungThreadIDs
#else
#define MAYBE_HungThreadIDs HungThreadIDs
#endif

TEST_F(HangWatcherSnapshotTest, MAYBE_HungThreadIDs) {
  // During hang capture the list of hung threads should be populated.
  hang_watcher_.SetOnHangClosureForTesting(base::BindLambdaForTesting([this] {
    EXPECT_EQ(hang_watcher_.GrabWatchStateSnapshotForTesting()
                  .PrepareHungThreadListCrashKey(),
              list_of_hung_thread_ids_during_capture_);
    ++hang_capture_count_;
  }));

  // When hang capture is over the list should be empty.
  hang_watcher_.SetAfterMonitorClosureForTesting(
      base::BindLambdaForTesting([this] { monitor_event_.Signal(); }));

  hang_watcher_.Start();

  // Register the main test thread for hang watching.
  auto unregister_thread_closure =
      HangWatcher::RegisterThread(base::HangWatcher::ThreadType::kMainThread);

  BlockingThread blocking_thread(&monitor_event_, base::TimeDelta{});
  blocking_thread.StartAndWaitForScopeEntered();
  {
    // Ensure the blocking thread entered the scope before the main thread. This
    // will guarantee an ordering while reporting the list of hung threads.
    task_environment_.AdvanceClock(kSmallCPUQuantum);

    // Start a WatchHangsInScope that expires right away. Ensures that
    // the first monitor will detect a hang. This scope will naturally have a
    // later deadline than the one in |blocking_thread_| since it was created
    // after.
    WatchHangsInScope expires_instantly(base::TimeDelta{});

    // Hung thread list should contain the id the blocking thread and then the
    // id of the test main thread since that is the order of increasing
    // deadline.
    TestIDList(
        ConcatenateThreadIds({blocking_thread.GetId(), test_thread_id_}));

    // |expires_instantly| and the scope from |blocking_thread| are still live
    // but already recorded so should be ignored.
    ExpectNoCapture();

    // Thread is joinable since we signaled |monitor_event_|. This closes the
    // scope in |blocking_thread|.
    blocking_thread.Join();

    // |expires_instantly| is still live but already recorded so should be
    // ignored.
    ExpectNoCapture();
  }

  // All HangWatchScopeEnables are over. There should be no capture.
  ExpectNoCapture();

  // Once all recorded scopes are over creating a new one and monitoring will
  // trigger a hang detection.
  WatchHangsInScope expires_instantly(base::TimeDelta{});
  TestIDList(ConcatenateThreadIds({test_thread_id_}));
}

TEST_F(HangWatcherSnapshotTest, TimeSinceLastSystemPowerResumeCrashKey) {
  // Override the capture of hangs. Simulate a crash key capture.
  hang_watcher_.SetOnHangClosureForTesting(base::BindLambdaForTesting([this] {
    ++hang_capture_count_;
    seconds_since_last_power_resume_crash_key_ =
        hang_watcher_.GetTimeSinceLastSystemPowerResumeCrashKeyValue();
  }));

  // When hang capture is over, unblock the main thread.
  hang_watcher_.SetAfterMonitorClosureForTesting(
      base::BindLambdaForTesting([this] { monitor_event_.Signal(); }));

  hang_watcher_.Start();

  // Register the main test thread for hang watching.
  auto unregister_thread_closure =
      HangWatcher::RegisterThread(base::HangWatcher::ThreadType::kMainThread);

  {
    WatchHangsInScope expires_instantly(base::TimeDelta{});
    task_environment_.AdvanceClock(kSmallCPUQuantum);

    TriggerMonitorAndWaitForCompletion();
    EXPECT_EQ(1, hang_capture_count_);
    EXPECT_EQ("Never suspended", seconds_since_last_power_resume_crash_key_);
  }

  {
    test::ScopedPowerMonitorTestSource power_monitor_source;
    power_monitor_source.Suspend();
    task_environment_.AdvanceClock(kSmallCPUQuantum);

    {
      WatchHangsInScope expires_instantly(base::TimeDelta{});
      task_environment_.AdvanceClock(kSmallCPUQuantum);
      TriggerMonitorAndWaitForCompletion();
      EXPECT_EQ(2, hang_capture_count_);
      EXPECT_EQ("Power suspended", seconds_since_last_power_resume_crash_key_);
    }

    power_monitor_source.Resume();
    constexpr TimeDelta kAfterResumeTime{base::Seconds(5)};
    task_environment_.AdvanceClock(kAfterResumeTime);

    {
      WatchHangsInScope expires_instantly(base::TimeDelta{});
      TriggerMonitorAndWaitForCompletion();
      EXPECT_EQ(3, hang_capture_count_);
      EXPECT_EQ(base::NumberToString(kAfterResumeTime.InSeconds()),
                seconds_since_last_power_resume_crash_key_);
    }
  }
}

namespace {

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
        HangWatcher::ProcessType::kBrowserProcess, false,
        /*emit_crashes=*/true);

    hang_watcher_.SetMonitoringPeriodForTesting(kMonitoringPeriod);
    hang_watcher_.SetOnHangClosureForTesting(base::BindRepeating(
        &WaitableEvent::Signal, base::Unretained(&hang_event_)));

    // HangWatcher uses a TickClock to detect how long it slept in between calls
    // to Monitor(). Override that clock to control its subjective passage of
    // time.
    hang_watcher_.SetTickClockForTesting(&test_clock_);
  }

  HangWatcherPeriodicMonitoringTest(
      const HangWatcherPeriodicMonitoringTest& other) = delete;
  HangWatcherPeriodicMonitoringTest& operator=(
      const HangWatcherPeriodicMonitoringTest& other) = delete;

  void TearDown() override { hang_watcher_.UnitializeOnMainThreadForTesting(); }

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

  std::unique_ptr<base::TickClock> fake_tick_clock_;
  HangWatcher hang_watcher_;

  // Signaled when a hang is detected.
  WaitableEvent hang_event_;

  base::ScopedClosureRunner unregister_thread_closure_;
};
}  // namespace

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

// During normal execution periodic monitorings should take place.
TEST_F(HangWatcherPeriodicMonitoringTest, PeriodicCallsTakePlace) {
  // HangWatcher::Monitor() will run once right away on thread registration.
  // We want to make sure it runs at a couple more times from being scheduled.
  constexpr int kMinimumMonitorCount = 3;

  RunLoop run_loop;

  // Setup the HangWatcher to unblock run_loop when the Monitor() has been
  // invoked enough times.
  hang_watcher_.SetAfterMonitorClosureForTesting(BarrierClosure(
      kMinimumMonitorCount, base::BindLambdaForTesting([&run_loop] {
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

  run_loop.Run();

  // No monitored scope means no possible hangs.
  ASSERT_FALSE(hang_event_.IsSignaled());
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

namespace {
class WatchHangsInScopeBlockingTest : public testing::Test {
 public:
  WatchHangsInScopeBlockingTest() {
    feature_list_.InitWithFeaturesAndParameters(kFeatureAndParams, {});
    HangWatcher::InitializeOnMainThread(
        HangWatcher::ProcessType::kBrowserProcess, false,
        /*emit_crashes=*/true);

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

  void TearDown() override { HangWatcher::UnitializeOnMainThreadForTesting(); }

  WatchHangsInScopeBlockingTest(const WatchHangsInScopeBlockingTest& other) =
      delete;
  WatchHangsInScopeBlockingTest& operator=(
      const WatchHangsInScopeBlockingTest& other) = delete;

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

  base::test::ScopedFeatureList feature_list_;
  HangWatcher hang_watcher_;
  base::ScopedClosureRunner unregister_thread_closure_;
};
}  // namespace

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

namespace internal {
namespace {

constexpr std::array<HangWatchDeadline::Flag, 3> kAllFlags{
    {HangWatchDeadline::Flag::kMinValue,
     HangWatchDeadline::Flag::kIgnoreCurrentWatchHangsInScope,
     HangWatchDeadline::Flag::kShouldBlockOnHang}};
}  // namespace

class HangWatchDeadlineTest : public testing::Test {
 protected:
  void AssertNoFlagsSet() const {
    for (HangWatchDeadline::Flag flag : kAllFlags) {
      ASSERT_FALSE(deadline_.IsFlagSet(flag));
    }
  }

  // Return a flag mask without one of the flags for test purposes. Use to
  // ignore that effect of setting a flag that was just set.
  uint64_t FlagsMinus(uint64_t flags, HangWatchDeadline::Flag flag) {
    return flags & ~(static_cast<uint64_t>(flag));
  }

  HangWatchDeadline deadline_;
};

// Verify that the extract functions don't mangle any bits.
TEST_F(HangWatchDeadlineTest, BitsPreservedThroughExtract) {
  for (auto bits : {kAllOnes, kAllZeros, kOnesThenZeroes, kZeroesThenOnes}) {
    ASSERT_TRUE((HangWatchDeadline::ExtractFlags(bits) |
                 HangWatchDeadline::ExtractDeadline(bits)) == bits);
  }
}

// Verify that setting and clearing a persistent flag works and has no unwanted
// side-effects. Neither the flags nor the deadline change concurrently in this
// test.
TEST_F(HangWatchDeadlineTest, SetAndClearPersistentFlag) {
  AssertNoFlagsSet();

  // Grab the original values for flags and deadline.
  auto [old_flags, old_deadline] = deadline_.GetFlagsAndDeadline();

  // Set the flag. Operation cannot fail.
  deadline_.SetIgnoreCurrentWatchHangsInScope();

  // Get new flags and deadline.
  auto [new_flags, new_deadline] = deadline_.GetFlagsAndDeadline();

  // Flag was set properly.
  ASSERT_TRUE(HangWatchDeadline::IsFlagSet(
      HangWatchDeadline::Flag::kIgnoreCurrentWatchHangsInScope, new_flags));

  // No side-effect on deadline.
  ASSERT_EQ(new_deadline, old_deadline);

  // No side-effect on other flags.
  ASSERT_EQ(
      FlagsMinus(new_flags,
                 HangWatchDeadline::Flag::kIgnoreCurrentWatchHangsInScope),
      old_flags);

  // Clear the flag, operation cannot fail.
  deadline_.UnsetIgnoreCurrentWatchHangsInScope();

  // Update new values.
  std::tie(new_flags, new_deadline) = deadline_.GetFlagsAndDeadline();

  // All flags back to original state.
  ASSERT_EQ(new_flags, old_flags);

  // Deadline still unnafected.
  ASSERT_EQ(new_deadline, old_deadline);
}

// Verify setting the TimeTicks value works and has no unwanted side-effects.
TEST_F(HangWatchDeadlineTest, SetDeadline) {
  TimeTicks ticks;

  AssertNoFlagsSet();
  ASSERT_NE(deadline_.GetDeadline(), ticks);

  // Set the deadline and verify it stuck.
  deadline_.SetDeadline(ticks);
  ASSERT_EQ(deadline_.GetDeadline(), ticks);

  // Only the value was modified, no flags should be set.
  AssertNoFlagsSet();
}

// Verify that setting a non-persistent flag (kShouldBlockOnHang)
// when the TimeTicks value changed since calling the flag setting
// function fails and has no side-effects.
TEST_F(HangWatchDeadlineTest, SetShouldBlockOnHangDeadlineChanged) {
  AssertNoFlagsSet();

  auto [flags, deadline] = deadline_.GetFlagsAndDeadline();

  // Simulate value change. Flags are constant.
  const base::TimeTicks new_deadline =
      base::TimeTicks::FromInternalValue(kArbitraryDeadline);
  ASSERT_NE(deadline, new_deadline);
  deadline_.SetSwitchBitsClosureForTesting(
      base::BindLambdaForTesting([] { return kArbitraryDeadline; }));

  // kShouldBlockOnHangs does not persist through value change.
  ASSERT_FALSE(deadline_.SetShouldBlockOnHang(flags, deadline));

  // Flag was not applied.
  ASSERT_FALSE(
      deadline_.IsFlagSet(HangWatchDeadline::Flag::kShouldBlockOnHang));

  // New value that was changed concurrently is preserved.
  ASSERT_EQ(deadline_.GetDeadline(), new_deadline);
}

// Verify that clearing a persistent (kIgnoreCurrentWatchHangsInScope) when
// the value changed succeeds and has non side-effects.
TEST_F(HangWatchDeadlineTest, ClearIgnoreHangsDeadlineChanged) {
  AssertNoFlagsSet();

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
  ASSERT_FALSE(deadline_.IsFlagSet(
      HangWatchDeadline::Flag::kIgnoreCurrentWatchHangsInScope));

  // New deadline that was changed concurrently is preserved.
  ASSERT_TRUE(deadline_.IsFlagSet(HangWatchDeadline::Flag::kShouldBlockOnHang));
  ASSERT_EQ(deadline_.GetDeadline(), new_deadline);
}

// Verify that setting a persistent (kIgnoreCurrentWatchHangsInScope) when
// the deadline or flags changed succeeds and has non side-effects.
TEST_F(HangWatchDeadlineTest,
       SetIgnoreCurrentHangWatchScopeEnableDeadlineChangedd) {
  AssertNoFlagsSet();

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
  ASSERT_TRUE(deadline_.IsFlagSet(
      HangWatchDeadline::Flag::kIgnoreCurrentWatchHangsInScope));

  // New deadline and flags that changed concurrently are preserved.
  ASSERT_TRUE(deadline_.IsFlagSet(HangWatchDeadline::Flag::kShouldBlockOnHang));
  ASSERT_EQ(deadline_.GetDeadline(), new_deadline);
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
  ASSERT_EQ(deadline_.GetDeadline(), TimeTicks{});

  // Verify the persistent flag stuck and the non-persistent one was unset.
  ASSERT_FALSE(
      deadline_.IsFlagSet(HangWatchDeadline::Flag::kShouldBlockOnHang));
  ASSERT_TRUE(deadline_.IsFlagSet(
      HangWatchDeadline::Flag::kIgnoreCurrentWatchHangsInScope));
}

}  // namespace internal

}  // namespace base
