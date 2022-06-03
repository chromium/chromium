// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/stack_canary_linux.h"

#include "base/compiler_specific.h"
#include "build/build_config.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace base {

#if defined(LIBC_GLIBC) && \
    (defined(ARCH_CPU_ARM_FAMILY) || defined(ARCH_CPU_X86_FAMILY))

namespace {
__attribute__((noinline, optnone)) void ResetCanaryAndReturn() {
  // Create a buffer >=8 bytes to force the stack protector on this function,
  // which should work as long as -fno-stack-protector isn't passed in the
  // default options. We compile this file with -fstack-protector-all, but it
  // may be overridden with -fstack-protector or -fstack-protector-strong.
  char buffer[10];
  ALLOW_UNUSED_LOCAL(buffer);
  ResetStackCanaryIfPossible();
}
}  // namespace

// Essentially tests that ResetStackCanaryIfPossible() changes the
// actual reference canary that is checked in the function prologue.
TEST(StackCanary, ChangingStackCanaryCrashesOnReturn) {
  ASSERT_DEATH(ResetCanaryAndReturn(), "stack smashing");
}

#if !defined(NDEBUG)
// Tests that the useful debug message works--specifically that on death, it
// prints out the bug URL with useful information.
TEST(StackCanary, ChangingStackCanaryPrintsDebugMessage) {
  SetStackSmashingEmitsDebugMessage();
  ASSERT_DEATH(ResetCanaryAndReturn(), "crbug\\.com/1206626");
}
#endif  // !defined(NDEBUG)

#endif  // defined(LIBC_GLIBC) && (defined(ARCH_CPU_ARM_FAMILY) ||
        // defined(ARCH_CPU_X86_FAMILY))

}  // namespace base
