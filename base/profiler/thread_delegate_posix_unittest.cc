// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/profiler/thread_delegate_posix.h"

#include "base/process/process_handle.h"
#include "build/build_config.h"
#include "build/chromeos_buildflags.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace base {

// ASAN moves local variables outside of the stack extents.
// Test is flaky on ChromeOS. crbug.com/1133434.
#if defined(ADDRESS_SANITIZER) || BUILDFLAG(IS_CHROMEOS_ASH)
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
  EXPECT_LE(base, stack_addr + 4 * 1024 * 1024);
}

#if defined(OS_ANDROID)

TEST(ThreadDelegatePosixTest, AndroidMainThreadStackBase) {
  // The delegate does not use pthread id for main thread.
  auto delegate = ThreadDelegatePosix::Create(
      SamplingProfilerThreadToken{GetCurrentProcId(), pthread_t()});
  ASSERT_TRUE(delegate);
  uintptr_t base = delegate->GetStackBaseAddress();
  EXPECT_GT(base, 0u);
}

#endif  // defined(OS_ANDROID)
}  // namespace base
