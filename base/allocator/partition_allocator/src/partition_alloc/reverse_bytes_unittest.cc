// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/40284755): Remove this and spanify to fix the errors.
#pragma allow_unsafe_buffers
#endif

#include "partition_alloc/reverse_bytes.h"

#include <cstdint>

#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace partition_alloc::internal {
namespace {

TEST(ReverseBytes, DeadBeefScramble) {
  if (sizeof(uintptr_t) == 4) {
    EXPECT_EQ(ReverseBytes(uintptr_t{0xefbeadde}), 0xdeadbeef);
  } else {
    // Hacky kludge to escape the compiler from immediately noticing that
    // this won't fit into a uintptr_t when it's four bytes.
    EXPECT_EQ(ReverseBytes(uint64_t{0xffeeddccefbeadde}), 0xdeadbeefccddeeff);
  }
}

}  // namespace
}  // namespace partition_alloc::internal
