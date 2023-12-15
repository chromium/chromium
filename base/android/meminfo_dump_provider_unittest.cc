// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/android/meminfo_dump_provider.h"
#include "base/android/build_info.h"
#include "base/trace_event/base_tracing.h"
#include "testing/gtest/include/gtest/gtest.h"

#include <cstdint>
#include <map>
#include <string>

namespace base::android {

namespace {
std::map<std::string, uint64_t> GetEntries(
    base::trace_event::MemoryAllocatorDump* dump) {
  std::map<std::string, uint64_t> entries;
  for (const auto& entry : dump->entries()) {
    EXPECT_EQ(entry.entry_type,
              base::trace_event::MemoryAllocatorDump::Entry::kUint64);
    entries.insert({entry.name, entry.value_uint64});
  }
  return entries;
}
}  // namespace

TEST(MeminfoDumpProviderTest, Simple) {
  auto& instance = MeminfoDumpProvider::Initialize();

  base::trace_event::MemoryDumpArgs args = {};
  args.level_of_detail = base::trace_event::MemoryDumpLevelOfDetail::kDetailed;
  base::trace_event::ProcessMemoryDump first_pmd{args};

  bool success = instance.OnMemoryDump(args, &first_pmd);
  ASSERT_TRUE(success);
  base::trace_event::MemoryAllocatorDump* first_dump =
      first_pmd.GetAllocatorDump(MeminfoDumpProvider::kDumpName);
  ASSERT_TRUE(first_dump);

  std::map<std::string, uint64_t> first_entries = GetEntries(first_dump);

  EXPECT_TRUE(first_entries.contains(
      MeminfoDumpProvider::kIsStaleName));  // Cannot assert on the value.
  // Expect the values to not be 0, that would indicate that the values are not
  // reported.
  ASSERT_TRUE(
      first_entries.contains(MeminfoDumpProvider::kPrivateDirtyMetricName));
  ASSERT_TRUE(first_entries.contains(MeminfoDumpProvider::kPssMetricName));
  EXPECT_GT(first_entries[MeminfoDumpProvider::kPrivateDirtyMetricName], 0u);
  EXPECT_GT(first_entries[MeminfoDumpProvider::kPssMetricName], 0u);

  base::trace_event::ProcessMemoryDump second_pmd{args};
  ASSERT_TRUE(instance.OnMemoryDump(args, &second_pmd));
  base::trace_event::MemoryAllocatorDump* second_dump =
      second_pmd.GetAllocatorDump(MeminfoDumpProvider::kDumpName);
  ASSERT_TRUE(second_dump);
  std::map<std::string, uint64_t> second_entries = GetEntries(second_dump);

  EXPECT_TRUE(second_entries.contains(MeminfoDumpProvider::kIsStaleName));
  EXPECT_TRUE(
      second_entries[MeminfoDumpProvider::kIsStaleName]);  // Entries are stale
                                                           // this time.
  ASSERT_TRUE(
      second_entries.contains(MeminfoDumpProvider::kPrivateDirtyMetricName));
  ASSERT_TRUE(second_entries.contains(MeminfoDumpProvider::kPssMetricName));
  if (BuildInfo::GetInstance()->sdk_int() >= SdkVersion::SDK_VERSION_Q) {
    // Stale values are reported.
    EXPECT_EQ(first_entries[MeminfoDumpProvider::kPrivateDirtyMetricName],
              second_entries[MeminfoDumpProvider::kPrivateDirtyMetricName]);
    EXPECT_EQ(first_entries[MeminfoDumpProvider::kPssMetricName],
              second_entries[MeminfoDumpProvider::kPssMetricName]);
  }
}

TEST(MeminfoDumpProviderTest, NoStaleReportsInBackgroundDumps) {
  auto& instance = MeminfoDumpProvider::Initialize();

  // First dump, data may or may not be stale.
  {
    base::trace_event::MemoryDumpArgs args = {};
    args.level_of_detail =
        base::trace_event::MemoryDumpLevelOfDetail::kDetailed;
    base::trace_event::ProcessMemoryDump pmd{args};
    ASSERT_TRUE(instance.OnMemoryDump(args, &pmd));
  }

  // Second one, stale data, should not report.
  {
    base::trace_event::MemoryDumpArgs args = {};
    args.level_of_detail =
        base::trace_event::MemoryDumpLevelOfDetail::kBackground;
    base::trace_event::ProcessMemoryDump pmd{args};
    ASSERT_TRUE(instance.OnMemoryDump(args, &pmd));
    base::trace_event::MemoryAllocatorDump* dump =
        pmd.GetAllocatorDump(MeminfoDumpProvider::kDumpName);
    EXPECT_FALSE(dump);
  }
}

}  // namespace base::android
