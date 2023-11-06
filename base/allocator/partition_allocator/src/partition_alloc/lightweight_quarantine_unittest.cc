// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/allocator/partition_allocator/src/partition_alloc/lightweight_quarantine.h"

#include "base/allocator/partition_allocator/src/partition_alloc/partition_alloc_for_testing.h"
#include "base/allocator/partition_allocator/src/partition_alloc/partition_page.h"
#include "base/allocator/partition_allocator/src/partition_alloc/partition_root.h"
#include "base/allocator/partition_allocator/src/partition_alloc/partition_stats.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace partition_alloc {

#if !defined(MEMORY_TOOL_REPLACES_ALLOCATOR)

namespace {

size_t GetObjectSize(void* object) {
  const auto* entry_slot_span = internal::SlotSpanMetadata::FromObject(object);
  return entry_slot_span->GetUtilizedSlotSize();
}

struct LightweightQuarantineTestParam {
  size_t capacity_in_bytes;
};

using QuarantineList =
    internal::LightweightQuarantineList<internal::LightweightQuarantineEntry,
                                        1024>;
constexpr LightweightQuarantineTestParam kSmallQuarantineList = {
    .capacity_in_bytes = 256};
constexpr LightweightQuarantineTestParam kLargeQuarantineList = {
    .capacity_in_bytes = 4096};

class PartitionAllocLightweightQuarantineTest
    : public testing::TestWithParam<LightweightQuarantineTestParam> {
 protected:
  void SetUp() override {
    const auto param = GetParam();

    allocator_ =
        std::make_unique<PartitionAllocatorForTesting>(PartitionOptions{});
    list_ = std::make_unique<QuarantineList>(allocator_->root(),
                                             param.capacity_in_bytes);

    auto stats = GetStats();
    ASSERT_EQ(0u, stats.size_in_bytes);
    ASSERT_EQ(0u, stats.count);
    ASSERT_EQ(0u, stats.cumulative_size_in_bytes);
    ASSERT_EQ(0u, stats.cumulative_count);
  }

  void TearDown() override {
    // |Purge()|d here.
    list_ = nullptr;
    allocator_ = nullptr;
  }

  PartitionRoot* GetRoot() const { return allocator_->root(); }

  QuarantineList* GetList() const { return list_.get(); }

  LightweightQuarantineStats GetStats() const {
    LightweightQuarantineStats stats{};
    list_->AccumulateStats(stats);
    return stats;
  }

  std::unique_ptr<PartitionAllocatorForTesting> allocator_;
  std::unique_ptr<QuarantineList> list_;
};
INSTANTIATE_TEST_SUITE_P(
    PartitionAllocLightweightQuarantineTestMultipleQuarantineSizeInstantiation,
    PartitionAllocLightweightQuarantineTest,
    ::testing::Values(kSmallQuarantineList, kLargeQuarantineList));

}  // namespace

TEST_P(PartitionAllocLightweightQuarantineTest, Basic) {
  constexpr size_t kObjectSize = 1;

  uintptr_t slots_address = GetList()->GetSlotsAddress();
  const size_t capacity_in_bytes = GetList()->GetCapacityInBytes();

  constexpr size_t kCount = 100;
  for (size_t i = 1; i <= kCount; i++) {
    void* object = GetRoot()->Alloc(kObjectSize);
    const size_t size = GetObjectSize(object);
    const size_t max_count = capacity_in_bytes / size;

    auto entry = QuarantineList::Entry(object);
    const uint32_t entry_id = GetList()->Quarantine(std::move(entry));
    const auto* entry_ptr =
        QuarantineList::GetEntryByID(slots_address, entry_id);

    ASSERT_NE(entry_ptr, nullptr);
    ASSERT_EQ(object, entry_ptr->GetObject());
    ASSERT_TRUE(GetList()->IsQuarantinedForTesting(object));

    const auto expected_count = std::min(i, max_count);
    auto stats = GetStats();
    ASSERT_EQ(expected_count * size, stats.size_in_bytes);
    ASSERT_EQ(expected_count, stats.count);
    ASSERT_EQ(i * size, stats.cumulative_size_in_bytes);
    ASSERT_EQ(i, stats.cumulative_count);
  }
}

TEST_P(PartitionAllocLightweightQuarantineTest, TooLargeAllocation) {
  constexpr size_t kObjectSize = 1 << 26;  // 64 MiB.
  const size_t capacity_in_bytes = GetList()->GetCapacityInBytes();

  void* object = GetRoot()->Alloc(kObjectSize);
  const size_t size = GetObjectSize(object);
  ASSERT_GT(size, capacity_in_bytes);

  auto entry = QuarantineList::Entry(object);
  GetList()->Quarantine(std::move(entry));

  ASSERT_FALSE(GetList()->IsQuarantinedForTesting(object));

  auto stats = GetStats();
  ASSERT_EQ(0u, stats.size_in_bytes);
  ASSERT_EQ(0u, stats.count);
  ASSERT_EQ(0u, stats.cumulative_size_in_bytes);
  ASSERT_EQ(0u, stats.cumulative_count);
}

#endif  // !defined(MEMORY_TOOL_REPLACES_ALLOCATOR)

}  // namespace partition_alloc
