// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/profiler/stack_base_address_posix.h"

#include "base/numerics/clamped_math.h"
#include "base/process/process_handle.h"
#include "build/build_config.h"
#include "build/chromeos_buildflags.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace base {

using ::testing::Gt;
using ::testing::Le;
using ::testing::Optional;

// ASAN moves local variables outside of the stack extents.
#if defined(ADDRESS_SANITIZER)
#define MAYBE_CurrentThread DISABLED_CurrentThread
#elif BUILDFLAG(IS_LINUX)
// We don't support getting the stack base address on Linux.
// https://crbug.com/1394278
#define MAYBE_CurrentThread DISABLED_CurrentThread
#else
#define MAYBE_CurrentThread CurrentThread
#endif
TEST(GetThreadStackBaseAddressTest, MAYBE_CurrentThread) {
  std::optional<uintptr_t> base =
      GetThreadStackBaseAddress(PlatformThread::CurrentId(), pthread_self());
  EXPECT_THAT(base, Optional(Gt(0u)));
  uintptr_t stack_addr = reinterpret_cast<uintptr_t>(&base);
  // Check that end of stack is within 4MiB of a current stack address.
  EXPECT_THAT(base, Optional(Le(ClampAdd(stack_addr, 4 * 1024 * 1024))));
}

#if BUILDFLAG(IS_ANDROID) || BUILDFLAG(IS_CHROMEOS)

TEST(GetThreadStackBaseAddressTest, MainThread) {
  // GetThreadStackBaseAddress does not use pthread_id for main thread on these
  // platforms.
  std::optional<uintptr_t> base =
      GetThreadStackBaseAddress(GetCurrentProcId(), pthread_t());
  EXPECT_THAT(base, Optional(Gt(0u)));
}

#endif  // BUILDFLAG(IS_ANDROID) || BUILDFLAG(IS_CHROMEOS)
}  // namespace base
