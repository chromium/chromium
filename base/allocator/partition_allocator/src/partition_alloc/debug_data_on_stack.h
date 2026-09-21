// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef PARTITION_ALLOC_DEBUG_DATA_ON_STACK_H_
#define PARTITION_ALLOC_DEBUG_DATA_ON_STACK_H_

#include <array>
#include <cstddef>
#include <cstdint>
#include <string_view>

#include "partition_alloc/build_config.h"
#include "partition_alloc/buildflags.h"
#include "partition_alloc/partition_alloc_base/debug/alias.h"

// alignas(16) DebugKv causes breakpad_unittests and sandbox_linux_unittests
// failures on android-marshmallow-x86-rel because of SIGSEGV.
#if PA_BUILDFLAG(IS_ANDROID) && PA_BUILDFLAG(PA_ARCH_CPU_X86_FAMILY) && \
    PA_BUILDFLAG(PA_ARCH_CPU_32_BITS)
#define PA_DEBUGKV_ALIGN alignas(8)
#else
#define PA_DEBUGKV_ALIGN alignas(16)
#endif

namespace partition_alloc::internal {

static constexpr size_t kDebugKeyMaxLength = 8ull;

// Used for PA_DEBUG_DATA_ON_STACK, below.
struct PA_DEBUGKV_ALIGN DebugKv {
  // 16 bytes object aligned on 16 bytes, to make it easier to see in crash
  // reports.
  std::array<char, kDebugKeyMaxLength> k = {};  // Not necessarily 0-terminated.
  uint64_t v = 0;

  DebugKv(std::string_view key, uint64_t value) : v(value) {
    // Fill with ' ', so that the stack dump is nicer to read.  Not using
    // memset() on purpose, this header is included from *many* places.
    for (char& c : k) {
      c = ' ';
    }

    for (size_t index = 0; index < k.size() && index < key.size(); index++) {
      k[index] = key[index];
      if (key[index] == '\0') {
        break;
      }
    }
  }
};

}  // namespace partition_alloc::internal

#define PA_CONCAT(x, y) x##y
#define PA_CONCAT2(x, y) PA_CONCAT(x, y)
#define PA_DEBUG_UNIQUE_NAME PA_CONCAT2(kv, __LINE__)

// Puts a key-value pair on the stack for debugging. `base::debug::Alias()`
// makes sure a local variable is saved on the stack, but the variables can be
// hard to find in crash reports, particularly if the frame pointer is not
// present / invalid.
//
// This puts a key right before the value on the stack. The key has to be a C
// string, which gets truncated if it's longer than 8 characters.
// Example use:
// PA_DEBUG_DATA_ON_STACK("size", 0x42)
//
// Sample output in lldb:
// (lldb) x 0x00007fffffffd0d0 0x00007fffffffd0f0
// 0x7fffffffd0d0: 73 69 7a 65 00 00 00 00 42 00 00 00 00 00 00 00
// size............
//
// With gdb, one can use:
// x/8g <STACK_POINTER>
// to see the data. With lldb, "x <STACK_POINTER> <FRAME_POINTER>" can be used.
#define PA_DEBUG_DATA_ON_STACK(name, value)                               \
  static_assert(sizeof name <=                                            \
                ::partition_alloc::internal::kDebugKeyMaxLength + 1);     \
  ::partition_alloc::internal::DebugKv PA_DEBUG_UNIQUE_NAME{name, value}; \
  ::partition_alloc::internal::base::debug::Alias(&PA_DEBUG_UNIQUE_NAME)

#endif  // PARTITION_ALLOC_DEBUG_DATA_ON_STACK_H_
