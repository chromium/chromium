// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/compiler_specific.h"
#include "base/debug/alias.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {

template <typename T>
void DoNotOptimize(T t) {
  // Ensure the compiler does not optimize the access out before the hardening
  // check. See https://github.com/llvm/llvm-project/issues/89432
  base::debug::Alias(&t);
}

TEST(CompilerHardeningDeathTest, ArrayOutOfBounds) {
  // Set up two arrays, and force them on the stack. This means one will be
  // placed after the other and provide some buffer for the out-of-bounds
  // access. (We wish to distinguish an crash from the memory error from a clean
  // crash from the compiler hardening.)
  char a[10] = {0};
  char b[10] = {0};
  base::debug::Alias(a);
  base::debug::Alias(a);

  int idx = 10;
  // Prevent the compiler from reasoning about the value of `idx` and rejecting
  // the array access at compile time.
  base::debug::Alias(&idx);

  // SAFETY: Although out of bounds, this is actually safe when built with
  // -fsanitize=array-bounds. The warning is a false positive. See
  // https://github.com/llvm/llvm-project/issues/87284
  EXPECT_DEATH_IF_SUPPORTED(UNSAFE_BUFFERS(DoNotOptimize(a[idx])), "");
  EXPECT_DEATH_IF_SUPPORTED(UNSAFE_BUFFERS(DoNotOptimize(b[idx])), "");
  EXPECT_DEATH_IF_SUPPORTED(UNSAFE_BUFFERS(a[idx] = 0), "");
  EXPECT_DEATH_IF_SUPPORTED(UNSAFE_BUFFERS(b[idx] = 0), "");

  idx = -1;
  // SAFETY: Although out of bounds, this is actually safe when built with
  // -fsanitize=array-bounds. The warning is a false positive. See
  // https://github.com/llvm/llvm-project/issues/87284
  EXPECT_DEATH_IF_SUPPORTED(UNSAFE_BUFFERS(DoNotOptimize(a[idx])), "");
  EXPECT_DEATH_IF_SUPPORTED(UNSAFE_BUFFERS(DoNotOptimize(b[idx])), "");
  EXPECT_DEATH_IF_SUPPORTED(UNSAFE_BUFFERS(a[idx] = 0), "");
  EXPECT_DEATH_IF_SUPPORTED(UNSAFE_BUFFERS(b[idx] = 0), "");
}

}  // namespace
