// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/threading/platform_thread_metrics.h"

#include <array>
#include <atomic>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

#include "base/metrics/histogram_base.h"
#include "base/metrics/statistics_recorder.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_util.h"
#include "base/task/current_thread.h"
#include "base/task/thread_pool.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/task_environment.h"
#include "base/test/test_timeouts.h"
#include "base/test/test_waitable_event.h"
#include "base/threading/platform_thread.h"
#include "base/threading/simple_thread.h"
#include "base/time/time.h"
#include "base/timer/elapsed_timer.h"
#include "build/build_config.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

#if BUILDFLAG(IS_ANDROID)
#include <linux/resource.h>
#include <sys/resource.h>
#endif  // BUILDFLAG(IS_ANDROID)

#if BUILDFLAG(IS_WIN)
#include <windows.h>
#endif

namespace base {

namespace {

using ::testing::Ge;
using ::testing::Gt;
using ::testing::Optional;

void BusyWork() {
  ElapsedTimer timer;
  while (timer.Elapsed() < TestTimeouts::tiny_timeout()) {
    std::vector<std::string> vec;
    int64_t test_value = 0;
    for (int i = 0; i < 100000; ++i) {
      ++test_value;
      vec.push_back(NumberToString(test_value));
    }
  }
}

class MetricsTestThread final : public SimpleThread {
 public:
  MetricsTestThread() : SimpleThread("MetricsTestThread") {}

  MetricsTestThread(const MetricsTestThread&) = delete;
  MetricsTestThread& operator=(const MetricsTestThread&) = delete;

  ~MetricsTestThread() final {
    if (HasBeenStarted() && !HasBeenJoined()) {
      Stop();
    }
  }

  PlatformThreadHandle handle() const {
    handle_ready_event_.Wait();
    return handle_.load(std::memory_order_relaxed);
  }

  // Stop the thread.
  void Stop() {
    ASSERT_TRUE(HasBeenStarted());
    ASSERT_FALSE(HasBeenJoined());
    stop_event_.Signal();
    Join();
  }

  // Cause the thread to do busy work. The caller will block until it's done.
  void DoBusyWork() {
    ASSERT_TRUE(HasBeenStarted());
    ASSERT_FALSE(HasBeenJoined());
    do_busy_work_event_.Signal();
    done_busy_work_event_.Wait();
  }

  // SimpleThread:
  void Run() final {
#if BUILDFLAG(IS_WIN)
    // CurrentHandle() returns a pseudo-handle that's the same in every thread.
    // Duplicate it to get a real handle.
    HANDLE win_handle;
    ASSERT_TRUE(::DuplicateHandle(::GetCurrentProcess(), ::GetCurrentThread(),
                                  ::GetCurrentProcess(), &win_handle,
                                  THREAD_QUERY_LIMITED_INFORMATION, FALSE, 0));
    PlatformThreadHandle handle(win_handle);
#else
    PlatformThreadHandle handle = PlatformThread::CurrentHandle();
#endif
    handle_.store(handle, std::memory_order_relaxed);
    handle_ready_event_.Signal();

    std::array<WaitableEvent*, 2> events{&do_busy_work_event_, &stop_event_};
    while (!stop_event_.IsSignaled()) {
      // WaitMany returns the lowest index among signaled events.
      if (WaitableEvent::WaitMany(events) == 0) {
        // DoBusyWork() waits on `done_busy_work_event_` so it should be
        // impossible to signal `stop_event_` too.
        ASSERT_FALSE(stop_event_.IsSignaled());
        BusyWork();
        done_busy_work_event_.Signal();
      }
    }
  }

 private:
  // When this is signaled, stop the thread.
  TestWaitableEvent stop_event_;

  // When `do_busy_work_event_` is signaled, do some busy work, then signal
  // `done_busy_work_event_`. These are auto-reset so they can be reused to do
  // more busy work.
  TestWaitableEvent do_busy_work_event_{WaitableEvent::ResetPolicy::AUTOMATIC};
  TestWaitableEvent done_busy_work_event_{
      WaitableEvent::ResetPolicy::AUTOMATIC};

  std::atomic<PlatformThreadHandle> handle_;
  mutable TestWaitableEvent handle_ready_event_;
};

class PlatformThreadMetricsTest : public ::testing::Test {
 public:
  void SetUp() override {
#if BUILDFLAG(IS_WIN) && !defined(ARCH_CPU_ARM64)
    // TSC is only initialized once TSCTicksPerSecond() is called twice at least
    // 50 ms apart on the same thread to get a baseline. If the system has a
    // TSC, make sure it's initialized so all GetCumulativeCPUUsage calls use
    // it.
    if (time_internal::HasConstantRateTSC()) {
      if (time_internal::TSCTicksPerSecond() == 0) {
        PlatformThread::Sleep(Milliseconds(51));
      }
      ASSERT_GT(time_internal::TSCTicksPerSecond(), 0);
    }
#endif
  }
};

}  // namespace

#if BUILDFLAG(IS_APPLE) || BUILDFLAG(IS_WIN)
TEST_F(PlatformThreadMetricsTest, CreateFromHandle) {
  EXPECT_FALSE(PlatformThreadMetrics::CreateFromHandle(PlatformThreadHandle()));
  EXPECT_TRUE(
      PlatformThreadMetrics::CreateFromHandle(PlatformThread::CurrentHandle()));

  MetricsTestThread thread;
  thread.Start();
  PlatformThreadHandle handle = thread.handle();
  ASSERT_FALSE(handle.is_null());
  EXPECT_FALSE(handle.is_equal(PlatformThread::CurrentHandle()));
  EXPECT_TRUE(PlatformThreadMetrics::CreateFromHandle(handle));
}
#endif

#if BUILDFLAG(IS_ANDROID) || BUILDFLAG(IS_CHROMEOS) || \
    BUILDFLAG(IS_FUCHSIA) || BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_WIN)
TEST_F(PlatformThreadMetricsTest, CreateFromId) {
  EXPECT_FALSE(PlatformThreadMetrics::CreateFromId(PlatformThreadId()));
  EXPECT_FALSE(PlatformThreadMetrics::CreateFromId(kInvalidThreadId));
  EXPECT_TRUE(PlatformThreadMetrics::CreateFromId(PlatformThread::CurrentId()));

  MetricsTestThread thread;
  thread.Start();
  PlatformThreadId tid = thread.tid();
  ASSERT_NE(tid, kInvalidThreadId);
  EXPECT_NE(tid, PlatformThread::CurrentId());
  EXPECT_TRUE(PlatformThreadMetrics::CreateFromId(tid));
}
#endif

TEST_F(PlatformThreadMetricsTest, GetCumulativeCPUUsage_CurrentThread) {
  auto metrics = PlatformThreadMetrics::CreateForCurrentThread();
  ASSERT_TRUE(metrics);
  const auto cpu_usage = metrics->GetCumulativeCPUUsage();
  ASSERT_THAT(cpu_usage, Optional(Ge(TimeDelta())));

  // First call to GetCPUUsageProportion() always returns 0.
  EXPECT_EQ(metrics->GetCPUUsageProportion(cpu_usage.value()), 0.0);

  BusyWork();

  const auto cpu_usage2 = metrics->GetCumulativeCPUUsage();
  ASSERT_THAT(cpu_usage2, Optional(Gt(cpu_usage.value())));

  // Should be capped at 100%, but may be higher due to rounding so there's no
  // strict upper bound to test.
  EXPECT_GT(metrics->GetCPUUsageProportion(cpu_usage2.value()), 0.0);
}

TEST_F(PlatformThreadMetricsTest, GetCumulativeCPUUsage_OtherThread) {
  MetricsTestThread thread;
  thread.Start();
#if BUILDFLAG(IS_APPLE)
  // Apple is the only platform that doesn't support CreateFromId().
  ASSERT_FALSE(thread.handle().is_null());
  auto metrics = PlatformThreadMetrics::CreateFromHandle(thread.handle());
#else
  ASSERT_NE(thread.tid(), kInvalidThreadId);
  auto metrics = PlatformThreadMetrics::CreateFromId(thread.tid());
#endif
  ASSERT_TRUE(metrics);

  const auto cpu_usage = metrics->GetCumulativeCPUUsage();
  ASSERT_THAT(cpu_usage, Optional(Ge(TimeDelta())));

  // First call to GetCPUUsageProportion() always returns 0.
  EXPECT_EQ(metrics->GetCPUUsageProportion(cpu_usage.value()), 0.0);

  thread.DoBusyWork();

  const auto cpu_usage2 = metrics->GetCumulativeCPUUsage();
  ASSERT_THAT(cpu_usage2, Optional(Gt(cpu_usage.value())));

  // Should be capped at 100%, but may be higher due to rounding so there's no
  // strict upper bound to test.
  EXPECT_GT(metrics->GetCPUUsageProportion(cpu_usage2.value()), 0.0);

  thread.Stop();

  // Thread is no longer running.
  const auto cpu_usage3 = metrics->GetCumulativeCPUUsage();

  // Ensure that measuring the CPU usage of a stopped thread doesn't give bogus
  // values, although it may fail on some platforms. (If the measurement works,
  // it will include any CPU used between the last measurement and the join.)

#if BUILDFLAG(IS_WIN)
  // Windows can always read the final CPU usage of a stopped thread.
  ASSERT_NE(cpu_usage3, std::nullopt);
#else
  // POSIX platforms are racy, so the measurement may fail. Apple and Fuchsia
  // seem to always fail, but if a change causes measurements to start working,
  // that's good too.
  if (cpu_usage3.has_value()) {
    EXPECT_GE(cpu_usage3.value(), cpu_usage2.value());
  }
#endif
}

#if BUILDFLAG(IS_ANDROID)
class PlatformThreadPriorityMonitorTest : public ::testing::Test {
 public:
  static constexpr std::string_view kMainThreadSuffix = "MainThread";
  static constexpr std::string_view kChildThreadSuffix = "ChildThread";
  static constexpr TimeDelta kMinSamplingInterval =
      PlatformThreadPriorityMonitor::kMinSamplingInterval;

  void TearDown() override { ASSERT_EQ(setpriority(PRIO_PROCESS, 0, 0), 0); }

  size_t GetRegisteredThreadCount() {
    auto& monitor = PlatformThreadPriorityMonitor::Get();
    AutoLock lock(monitor.lock_);
    return monitor.thread_id_to_histogram_.size();
  }

  static std::string HistogramNameForSuffix(const std::string_view& suffix) {
    return PlatformThreadPriorityMonitor::Get().GetHistogramNameForSuffix(
        suffix);
  }
};

class PriorityMonitorTestDelegate : public PlatformThread::Delegate {
 public:
  PriorityMonitorTestDelegate() = default;
  ~PriorityMonitorTestDelegate() override = default;

  void ThreadMain() override {
    PlatformThreadPriorityMonitor::Get().RegisterCurrentThread(
        PlatformThreadPriorityMonitorTest::kChildThreadSuffix);
    registered_event_.Signal();
    stop_event_.Wait();
  }

  void WaitUntilRegistered() { registered_event_.Wait(); }

  void SignalStop() { stop_event_.Signal(); }

 private:
  TestWaitableEvent stop_event_;
  TestWaitableEvent registered_event_;
};

// Test UnregisterCurrentThread() is called on thread exit.
TEST_F(PlatformThreadPriorityMonitorTest, UnregisterOnJoin) {
  ASSERT_EQ(0u, GetRegisteredThreadCount());

  PriorityMonitorTestDelegate delegate;
  PlatformThreadHandle handle;
  ASSERT_TRUE(PlatformThread::Create(0, &delegate, &handle));

  delegate.WaitUntilRegistered();
  EXPECT_EQ(1u, GetRegisteredThreadCount());

  delegate.SignalStop();
  PlatformThread::Join(handle);

  EXPECT_EQ(0u, GetRegisteredThreadCount());
}

// Test that priority monitor reports thread priorities for all registered
// threads.
TEST_F(PlatformThreadPriorityMonitorTest, ReportThreadPriorities) {
  test::TaskEnvironment task_environment{
      test::TaskEnvironment::TimeSource::MOCK_TIME};

  PriorityMonitorTestDelegate delegate;
  PlatformThreadHandle handle;
  ASSERT_TRUE(PlatformThread::Create(0, &delegate, &handle));

  // Register the current thread and start monitoring thread priorities.
  PlatformThreadPriorityMonitor::Get().RegisterCurrentThread(kMainThreadSuffix);
  PlatformThreadPriorityMonitor::Get().Start();

  // Set the priority of the current thread to a different value than the child
  // thread.
  constexpr int kTestNiceValue = -7;
  ASSERT_EQ(setpriority(PRIO_PROCESS, 0, kTestNiceValue), 0);

  delegate.WaitUntilRegistered();
  EXPECT_EQ(2u, GetRegisteredThreadCount());

  HistogramTester histogram_tester;
  const std::string main_thread_histogram_name =
      HistogramNameForSuffix(kMainThreadSuffix);
  const std::string child_thread_histogram_name =
      HistogramNameForSuffix(kChildThreadSuffix);

  task_environment.FastForwardBy(Milliseconds(1));
  histogram_tester.ExpectTotalCount(main_thread_histogram_name, 0);
  histogram_tester.ExpectTotalCount(child_thread_histogram_name, 0);

  // Should record a sample for each thread.
  task_environment.FastForwardBy(kMinSamplingInterval - Milliseconds(1));
  histogram_tester.ExpectTotalCount(main_thread_histogram_name, 1);
  histogram_tester.ExpectUniqueSample(main_thread_histogram_name,
                                      kTestNiceValue, 1);
  histogram_tester.ExpectTotalCount(child_thread_histogram_name, 1);
  histogram_tester.ExpectUniqueSample(child_thread_histogram_name, 0, 1);

  delegate.SignalStop();
  PlatformThread::Join(handle);
}
#endif  // BUILDFLAG(IS_ANDROID)

}  // namespace base
