// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <string.h>
#include <algorithm>
#include <utility>

#include "base/profiler/profile_builder.h"
#include "base/profiler/sampling_profiler_thread_token.h"
#include "base/profiler/stack_buffer.h"
#include "base/profiler/stack_copier_signal.h"
#include "base/profiler/thread_delegate_posix.h"
#include "base/stl_util.h"
#include "base/synchronization/waitable_event.h"
#include "base/threading/platform_thread.h"
#include "base/threading/simple_thread.h"
#include "build/build_config.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace base {

namespace {

class TestProfileBuilder : public ProfileBuilder {
 public:
  TestProfileBuilder() = default;

  TestProfileBuilder(const TestProfileBuilder&) = delete;
  TestProfileBuilder& operator=(const TestProfileBuilder&) = delete;

  // ProfileBuilder
  ModuleCache* GetModuleCache() override { return nullptr; }

  void RecordMetadata(
      base::ProfileBuilder::MetadataProvider* metadata_provider) override {}

  void OnSampleCompleted(std::vector<Frame> frames) override {}
  void OnProfileCompleted(TimeDelta profile_duration,
                          TimeDelta sampling_period) override {}

 private:
};

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
    volatile uint32_t sentinels[size(kStackSentinels)];
    for (size_t i = 0; i < size(kStackSentinels); ++i)
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

}  // namespace

// ASAN moves local variables outside of the stack extents, which breaks the
// sentinels. TSAN hangs on the AsyncSafeWaitableEvent FUTEX_WAIT call.
#if defined(ADDRESS_SANITIZER) || defined(THREAD_SANITIZER)
#define MAYBE_CopyStack DISABLED_CopyStack
#else
#define MAYBE_CopyStack CopyStack
#endif
TEST(StackCopierSignalTest, MAYBE_CopyStack) {
  StackBuffer stack_buffer(/* buffer_size = */ 1 << 20);
  memset(stack_buffer.buffer(), 0, stack_buffer.size());
  uintptr_t stack_top = 0;
  TestProfileBuilder profiler_builder;
  RegisterContext context;

  StackCopierSignal copier(std::make_unique<ThreadDelegatePosix>(
      GetSamplingProfilerCurrentThreadToken()));

  // Copy the sentinel values onto the stack. Volatile to defeat compiler
  // optimizations.
  volatile uint32_t sentinels[size(kStackSentinels)];
  for (size_t i = 0; i < size(kStackSentinels); ++i)
    sentinels[i] = kStackSentinels[i];

  bool result =
      copier.CopyStack(&stack_buffer, &stack_top, &profiler_builder, &context);
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

// Limit to 32-bit Android, which is the platform we care about for this
// functionality. The test is broken on too many other varied platforms to try
// to selectively disable.
#if !(defined(OS_ANDROID) && defined(ARCH_CPU_32_BITS))
#define MAYBE_CopyStackFromOtherThread DISABLED_CopyStackFromOtherThread
#else
#define MAYBE_CopyStackFromOtherThread CopyStackFromOtherThread
#endif
TEST(StackCopierSignalTest, MAYBE_CopyStackFromOtherThread) {
  StackBuffer stack_buffer(/* buffer_size = */ 1 << 20);
  memset(stack_buffer.buffer(), 0, stack_buffer.size());
  uintptr_t stack_top = 0;
  TestProfileBuilder profiler_builder;
  RegisterContext context{};

  TargetThread target_thread;
  target_thread.Start();
  const SamplingProfilerThreadToken thread_token =
      target_thread.GetThreadToken();

  StackCopierSignal copier(std::make_unique<ThreadDelegatePosix>(thread_token));

  bool result =
      copier.CopyStack(&stack_buffer, &stack_top, &profiler_builder, &context);
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
