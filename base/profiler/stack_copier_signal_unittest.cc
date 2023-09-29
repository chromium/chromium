// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <string.h>
#include <algorithm>
#include <utility>
#include <vector>

#include "base/debug/alias.h"
#include "base/profiler/sampling_profiler_thread_token.h"
#include "base/profiler/stack_buffer.h"
#include "base/profiler/stack_copier_signal.h"
#include "base/profiler/thread_delegate_posix.h"
#include "base/synchronization/waitable_event.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/threading/platform_thread.h"
#include "base/threading/simple_thread.h"
#include "build/build_config.h"
#include "build/chromeos_buildflags.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace base {

namespace {

using ::testing::ElementsAre;
using ::testing::Return;

// Values to write to the stack and look for in the copy.
static const uint32_t kStackSentinels[] = {0xf312ecd9, 0x1fcd7f19, 0xe69e617d,
                                           0x8245f94f};

class TargetThread : public SimpleThread {
 public:
  TargetThread()
      : SimpleThread("target", Options()),
        started_(WaitableEvent::ResetPolicy::MANUAL,
                 WaitableEvent::InitialState::NOT_SIGNALED),
        copy_finished_(WaitableEvent::ResetPolicy::MANUAL,
                       WaitableEvent::InitialState::NOT_SIGNALED) {}

  void Run() override {
    thread_token_ = GetSamplingProfilerCurrentThreadToken();

    // Copy the sentinel values onto the stack. Volatile to defeat compiler
    // optimizations.
    volatile uint32_t sentinels[std::size(kStackSentinels)];
    for (size_t i = 0; i < std::size(kStackSentinels); ++i)
      sentinels[i] = kStackSentinels[i];

    started_.Signal();
    copy_finished_.Wait();
  }

  SamplingProfilerThreadToken GetThreadToken() {
    started_.Wait();
    return thread_token_;
  }

  void NotifyCopyFinished() { copy_finished_.Signal(); }

 private:
  WaitableEvent started_;
  WaitableEvent copy_finished_;
  SamplingProfilerThreadToken thread_token_;
};

class TestStackCopierDelegate : public StackCopier::Delegate {
 public:
  void OnStackCopy() override {
    on_stack_copy_was_invoked_ = true;
  }

  bool on_stack_copy_was_invoked() const { return on_stack_copy_was_invoked_; }

 private:
  bool on_stack_copy_was_invoked_ = false;
};

class MockTickClock : public TickClock {
 public:
  MOCK_METHOD(TimeTicks, NowTicks, (), (const, override));
};

}  // namespace

// ASAN moves local variables outside of the stack extents, which breaks the
// sentinels.
// MSan complains that the memcmp() reads uninitialized memory.
// TSAN hangs on the AsyncSafeWaitableEvent FUTEX_WAIT call.
#if defined(ADDRESS_SANITIZER) || defined(MEMORY_SANITIZER) || \
    defined(THREAD_SANITIZER)
#define MAYBE_CopyStack DISABLED_CopyStack
#elif BUILDFLAG(IS_LINUX)
// We don't support getting the stack base address on Linux, and thus can't
// copy the stack. // https://crbug.com/1394278
#define MAYBE_CopyStack DISABLED_CopyStack
#else
#define MAYBE_CopyStack CopyStack
#endif
TEST(StackCopierSignalTest, MAYBE_CopyStack) {
  StackBuffer stack_buffer(/* buffer_size = */ 1 << 20);
  memset(stack_buffer.buffer(), 0, stack_buffer.size());
  uintptr_t stack_top = 0;
  TimeTicks timestamp;
  RegisterContext context;
  TestStackCopierDelegate stack_copier_delegate;

  auto thread_delegate =
      ThreadDelegatePosix::Create(GetSamplingProfilerCurrentThreadToken());
  ASSERT_TRUE(thread_delegate);
  StackCopierSignal copier(std::move(thread_delegate));

  // Copy the sentinel values onto the stack.
  uint32_t sentinels[std::size(kStackSentinels)];
  for (size_t i = 0; i < std::size(kStackSentinels); ++i)
    sentinels[i] = kStackSentinels[i];
  base::debug::Alias((void*)sentinels);  // Defeat compiler optimizations.

  bool result = copier.CopyStack(&stack_buffer, &stack_top, &timestamp,
                                 &context, &stack_copier_delegate);
  ASSERT_TRUE(result);

  uint32_t* const end = reinterpret_cast<uint32_t*>(stack_top);
  uint32_t* const sentinel_location = std::find_if(
      reinterpret_cast<uint32_t*>(RegisterContextStackPointer(&context)), end,
      [](const uint32_t& location) {
        return memcmp(&location, &kStackSentinels[0],
                      sizeof(kStackSentinels)) == 0;
      });
  EXPECT_NE(end, sentinel_location);
}

// TSAN hangs on the AsyncSafeWaitableEvent FUTEX_WAIT call.
#if defined(THREAD_SANITIZER)
#define MAYBE_CopyStackTimestamp DISABLED_CopyStackTimestamp
#elif BUILDFLAG(IS_LINUX)
// We don't support getting the stack base address on Linux, and thus can't
// copy the stack. // https://crbug.com/1394278
#define MAYBE_CopyStackTimestamp DISABLED_CopyStackTimestamp
#else
#define MAYBE_CopyStackTimestamp CopyStackTimestamp
#endif
TEST(StackCopierSignalTest, MAYBE_CopyStackTimestamp) {
  StackBuffer stack_buffer(/* buffer_size = */ 1 << 20);
  memset(stack_buffer.buffer(), 0, stack_buffer.size());
  uintptr_t stack_top = 0;
  TimeTicks timestamp;
  RegisterContext context;
  TestStackCopierDelegate stack_copier_delegate;

  auto thread_delegate =
      ThreadDelegatePosix::Create(GetSamplingProfilerCurrentThreadToken());
  ASSERT_TRUE(thread_delegate);
  StackCopierSignal copier(std::move(thread_delegate));

  TimeTicks before = TimeTicks::Now();
  bool result = copier.CopyStack(&stack_buffer, &stack_top, &timestamp,
                                 &context, &stack_copier_delegate);
  TimeTicks after = TimeTicks::Now();
  ASSERT_TRUE(result);

  EXPECT_GE(timestamp, before);
  EXPECT_LE(timestamp, after);
}

// TSAN hangs on the AsyncSafeWaitableEvent FUTEX_WAIT call.
#if defined(THREAD_SANITIZER)
#define MAYBE_CopyStackDelegateInvoked DISABLED_CopyStackDelegateInvoked
#elif BUILDFLAG(IS_LINUX)
// We don't support getting the stack base address on Linux, and thus can't
// copy the stack. // https://crbug.com/1394278
#define MAYBE_CopyStackDelegateInvoked DISABLED_CopyStackDelegateInvoked
#else
#define MAYBE_CopyStackDelegateInvoked CopyStackDelegateInvoked
#endif
TEST(StackCopierSignalTest, MAYBE_CopyStackDelegateInvoked) {
  StackBuffer stack_buffer(/* buffer_size = */ 1 << 20);
  memset(stack_buffer.buffer(), 0, stack_buffer.size());
  uintptr_t stack_top = 0;
  TimeTicks timestamp;
  RegisterContext context;
  TestStackCopierDelegate stack_copier_delegate;

  auto thread_delegate =
      ThreadDelegatePosix::Create(GetSamplingProfilerCurrentThreadToken());
  ASSERT_TRUE(thread_delegate);
  StackCopierSignal copier(std::move(thread_delegate));

  bool result = copier.CopyStack(&stack_buffer, &stack_top, &timestamp,
                                 &context, &stack_copier_delegate);
  ASSERT_TRUE(result);

  EXPECT_TRUE(stack_copier_delegate.on_stack_copy_was_invoked());
}

// TSAN hangs on the AsyncSafeWaitableEvent FUTEX_WAIT call.
#if defined(THREAD_SANITIZER)
#define MAYBE_CopyStackUMAStats DISABLED_CopyStackUMAStats
#elif BUILDFLAG(IS_LINUX)
// We don't support getting the stack base address on Linux, and thus can't
// copy the stack. // https://crbug.com/1394278
#define MAYBE_CopyStackUMAStats DISABLED_CopyStackUMAStats
#else
#define MAYBE_CopyStackUMAStats CopyStackUMAStats
#endif
TEST(StackCopierSignalTest, MAYBE_CopyStackUMAStats) {
  HistogramTester histograms;
  StackBuffer stack_buffer(/* buffer_size = */ 1 << 20);
  memset(stack_buffer.buffer(), 0, stack_buffer.size());
  uintptr_t stack_top = 0;
  TimeTicks timestamp;
  RegisterContext context;
  TestStackCopierDelegate stack_copier_delegate;
  MockTickClock clock;
  TimeTicks real_now = TimeTicks::Now();
  EXPECT_CALL(clock, NowTicks())
      // signal_time
      .WillOnce(Return(real_now - Microseconds(1000)))
      // wait_start_time
      .WillOnce(Return(real_now - Microseconds(600)))
      // wait_end_time. Also covers the fallback if needed.
      .WillRepeatedly(Return(real_now));

  auto thread_delegate =
      ThreadDelegatePosix::Create(GetSamplingProfilerCurrentThreadToken());
  ASSERT_TRUE(thread_delegate);
  StackCopierSignal copier(std::move(thread_delegate));
  copier.set_clock_for_testing(&clock);

  bool result = copier.CopyStack(&stack_buffer, &stack_top, &timestamp,
                                 &context, &stack_copier_delegate);
  ASSERT_TRUE(result);

  EXPECT_THAT(
      histograms.GetAllSamples("UMA.StackProfiler.CopyStack.Event"),
      ElementsAre(Bucket(StackCopierSignal::CopyStackEvent::kStarted, 1),
                  Bucket(StackCopierSignal::CopyStackEvent::kSucceeded, 1)));

  // Do not use HistogramTester::ExpectUniqueTimeSample which assumes the
  // histogram has units of milliseconds.
  histograms.ExpectUniqueSample(
      "UMA.StackProfiler.CopyStack.TotalCrossThreadTime",
      // signal_time to wait_end_time should be 1000 microseconds.
      1000, 1);
  histograms.ExpectUniqueSample(
      "UMA.StackProfiler.CopyStack.ProfileThreadTotalWaitTime",
      // start_wait_time to end_wait_time should be 600 microseconds.
      600, 1);
  // Since we can't override the times returned from the signal handler, we
  // can't use the normal ExpectUniqueSample. HistogramTester doesn't give us
  // enough information to check if a sample was in a range. So the best we can
  // do is check that we got a sample for SignalToHandlerTime, HandlerRunTime,
  // and EventSignalToWaitEndTime. However, we might not even get a sample
  // for those if the signal handler couldn't get a time, and we don't want to
  // fail the test for that. So all we can verify is that:
  // 1. We have at most one sample for each and
  // 2. If SignalToHandlerTime and EventSignalToWaitEndTime both have a sample
  //    (meaning both clock fetches succeeded), then HandlerRunTime does as
  //    well.
  HistogramTester::CountsMap counts =
      histograms.GetTotalCountsForPrefix("UMA.StackProfiler.CopyStack.");

  int signal_to_handler_sample_count = 0;
  if (auto it = counts.find("UMA.StackProfiler.CopyStack.SignalToHandlerTime");
      it != counts.end()) {
    signal_to_handler_sample_count = it->second;
    EXPECT_EQ(signal_to_handler_sample_count, 1);
  }
  int handler_run_time_sample_count = 0;
  if (auto it = counts.find("UMA.StackProfiler.CopyStack.HandlerRunTime");
      it != counts.end()) {
    handler_run_time_sample_count = it->second;
    EXPECT_EQ(handler_run_time_sample_count, 1);
  }
  int event_signal_to_wait_end_time_sample_count = 0;
  if (auto it =
          counts.find("UMA.StackProfiler.CopyStack.EventSignalToWaitEndTime");
      it != counts.end()) {
    event_signal_to_wait_end_time_sample_count = it->second;
    EXPECT_EQ(event_signal_to_wait_end_time_sample_count, 1);
  }

  EXPECT_EQ(handler_run_time_sample_count != 0,
            signal_to_handler_sample_count != 0 &&
                event_signal_to_wait_end_time_sample_count != 0);
}

// Limit to 32-bit Android, which is the platform we care about for this
// functionality. The test is broken on too many other varied platforms to try
// to selectively disable.
#if !(BUILDFLAG(IS_ANDROID) && defined(ARCH_CPU_32_BITS))
#define MAYBE_CopyStackFromOtherThread DISABLED_CopyStackFromOtherThread
#elif BUILDFLAG(IS_LINUX)
// We don't support getting the stack base address on Linux, and thus can't
// copy the stack. // https://crbug.com/1394278
#define MAYBE_CopyStackFromOtherThread DISABLED_CopyStackFromOtherThread
#else
#define MAYBE_CopyStackFromOtherThread CopyStackFromOtherThread
#endif
TEST(StackCopierSignalTest, MAYBE_CopyStackFromOtherThread) {
  StackBuffer stack_buffer(/* buffer_size = */ 1 << 20);
  memset(stack_buffer.buffer(), 0, stack_buffer.size());
  uintptr_t stack_top = 0;
  TimeTicks timestamp;
  RegisterContext context{};
  TestStackCopierDelegate stack_copier_delegate;

  TargetThread target_thread;
  target_thread.Start();
  const SamplingProfilerThreadToken thread_token =
      target_thread.GetThreadToken();

  auto thread_delegate = ThreadDelegatePosix::Create(thread_token);
  ASSERT_TRUE(thread_delegate);
  StackCopierSignal copier(std::move(thread_delegate));

  bool result = copier.CopyStack(&stack_buffer, &stack_top, &timestamp,
                                 &context, &stack_copier_delegate);
  ASSERT_TRUE(result);

  target_thread.NotifyCopyFinished();
  target_thread.Join();

  uint32_t* const end = reinterpret_cast<uint32_t*>(stack_top);
  uint32_t* const sentinel_location = std::find_if(
      reinterpret_cast<uint32_t*>(RegisterContextStackPointer(&context)), end,
      [](const uint32_t& location) {
        return memcmp(&location, &kStackSentinels[0],
                      sizeof(kStackSentinels)) == 0;
      });
  EXPECT_NE(end, sentinel_location);
}

}  // namespace base
