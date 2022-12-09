// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/profiler/thread_delegate_posix.h"

#include "base/numerics/clamped_math.h"
#include "base/process/process_handle.h"
#include "build/build_config.h"
#include "build/chromeos_buildflags.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace base {

// ASAN moves local variables outside of the stack extents.
#if defined(ADDRESS_SANITIZER)
#define MAYBE_CurrentThreadBase DISABLED_CurrentThreadBase
#elif BUILDFLAG(IS_LINUX)
// We don't support getting the stack base address on Linux.
// https://crbug.com/1394278
#define MAYBE_CurrentThreadBase DISABLED_CurrentThreadBase
#else
#define MAYBE_CurrentThreadBase CurrentThreadBase
#endif
TEST(ThreadDelegatePosixTest, MAYBE_CurrentThreadBase) {
  auto delegate =
      ThreadDelegatePosix::Create(GetSamplingProfilerCurrentThreadToken());
  ASSERT_TRUE(delegate);
  uintptr_t base = delegate->GetStackBaseAddress();
  EXPECT_GT(base, 0u);
  uintptr_t stack_addr = reinterpret_cast<uintptr_t>(&base);
  // Check that end of stack is within 4MiB of a current stack address.
  EXPECT_LE(base, ClampAdd(stack_addr, 4 * 1024 * 1024));
}

#if BUILDFLAG(IS_ANDROID)
// On ChromeOS, this functionality is tested by
// GetThreadStackBaseAddressTest.MainThread.
TEST(ThreadDelegatePosixTest, MainThreadStackBase) {
  // The delegate does not use pthread id for main thread.
  auto delegate = ThreadDelegatePosix::Create(
      SamplingProfilerThreadToken{GetCurrentProcId(), pthread_t()});
  ASSERT_TRUE(delegate);
  uintptr_t base = delegate->GetStackBaseAddress();
  EXPECT_GT(base, 0u);
}

#endif  // BUILDFLAG(IS_ANDROID)
}  // namespace base
