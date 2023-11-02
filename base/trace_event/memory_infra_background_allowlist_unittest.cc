// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/trace_event/memory_infra_background_allowlist.h"

#include "testing/gtest/include/gtest/gtest.h"

namespace base {

namespace trace_event {

TEST(MemoryInfraBackgroundAllowlist, Allowlist) {
  // Global dumps that are of hex digits are all allowed for background use.
  EXPECT_TRUE(IsMemoryAllocatorDumpNameInAllowlist("global/01234ABCDEF"));
  EXPECT_TRUE(
      IsMemoryAllocatorDumpNameInAllowlist("shared_memory/01234ABCDEF"));

  // Global dumps that contain non-hex digits are not in the allowlist.
  EXPECT_FALSE(IsMemoryAllocatorDumpNameInAllowlist("global/GHIJK"));
  EXPECT_FALSE(IsMemoryAllocatorDumpNameInAllowlist("shared_memory/GHIJK"));

  // Test a couple that contain pointer values.
  EXPECT_TRUE(IsMemoryAllocatorDumpNameInAllowlist("blink_gc/main/heap"));
  EXPECT_TRUE(IsMemoryAllocatorDumpNameInAllowlist(
      "blink_gc/workers/worker_0x123/heap"));
  EXPECT_TRUE(IsMemoryAllocatorDumpNameInAllowlist(
      "blink_gc/workers/heap/worker_0x123"));
  EXPECT_FALSE(
      IsMemoryAllocatorDumpNameInAllowlist("blink_gc/main/heap/0x123"));
}

}  // namespace trace_event

}  // namespace base
