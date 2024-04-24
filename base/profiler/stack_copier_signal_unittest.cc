// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/40284755): Remove this and spanify to fix the errors.
#pragma allow_unsafe_buffers
#endif

#include <string.h>
#include <algorithm>
#include <utility>

#include "base/debug/alias.h"
#include "base/profiler/sampling_profiler_thread_token.h"
#include "base/profiler/stack_buffer.h"
#include "base/profiler/stack_copier_signal.h"
#include "base/profiler/thread_delegate_posix.h"
#include "base/synchronization/waitable_event.h"
#include "base/threading/platform_thread.h"
#include "base/threading/simple_thread.h"
#include "build/build_config.h"
#include "build/chromeos_buildflags.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace base {

namespace {

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
