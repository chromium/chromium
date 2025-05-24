// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "partition_alloc/lightweight_quarantine.h"

#include "partition_alloc/extended_api.h"
#include "partition_alloc/lightweight_quarantine_support.h"
#include "partition_alloc/partition_alloc_for_testing.h"
#include "partition_alloc/partition_page.h"
#include "partition_alloc/partition_root.h"
#include "partition_alloc/partition_stats.h"
#include "partition_alloc/thread_cache.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace partition_alloc {

#if !defined(MEMORY_TOOL_REPLACES_ALLOCATOR)

namespace {

using QuarantineConfig = internal::LightweightQuarantineBranchConfig;
using QuarantineRoot = internal::LightweightQuarantineRoot;
using QuarantineBranch = internal::LightweightQuarantineBranch;

class PartitionAllocLightweightQuarantineTest
    : public testing::TestWithParam<QuarantineConfig> {
 protected:
  void SetUp() override {
    allocator_ =
        std::make_unique<PartitionAllocatorForTesting>(PartitionOptions{});

    root_.emplace(*allocator_->root());
    branch_.emplace(root_->CreateBranch(GetParam()));

    auto stats = GetStats();
    ASSERT_EQ(0u, stats.size_in_bytes);
    ASSERT_EQ(0u, stats.count);
    ASSERT_EQ(0u, stats.cumulative_size_in_bytes);
    ASSERT_EQ(0u, stats.cumulative_count);
  }

  void TearDown() override {
    // |Purge()|d here.
    branch_.reset();
    root_.reset();
    allocator_ = nullptr;
  }

  PartitionRoot* GetPartitionRoot() const { return allocator_->root(); }

  QuarantineRoot* GetQuarantineRoot() { return &root_.value(); }
  QuarantineBranch* GetQuarantineBranch() { return &branch_.value(); }

  bool Quarantine(void* object) {
    auto* slot_span = internal::SlotSpanMetadata<
        internal::MetadataKind::kReadOnly>::FromObject(object);
    uintptr_t slot_start = GetPartitionRoot()->ObjectToSlotStart(object);
    size_t usable_size = GetPartitionRoot()->GetSlotUsableSize(slot_span);
    return GetQuarantineBranch()->Quarantine(object, slot_span, slot_start,
                                             usable_size);
  }

  size_t GetObjectSize(void* object) {
    auto* entry_slot_span = internal::SlotSpanMetadata<
        internal::MetadataKind::kReadOnly>::FromObject(object);
    return GetPartitionRoot()->GetSlotUsableSize(entry_slot_span);
  }

  LightweightQuarantineStats GetStats() const {
    LightweightQuarantineStats stats{};
    root_->AccumulateStats(stats);
    return stats;
  }

  std::unique_ptr<PartitionAllocatorForTesting> allocator_;
  std::optional<QuarantineRoot> root_;
  std::optional<QuarantineBranch> branch_;
};

constexpr QuarantineConfig kConfigSmall = {.lock_required = false,
                                           .branch_capacity_in_bytes = 256};
constexpr QuarantineConfig kConfigLarge = {.lock_required = false,
                                           .branch_capacity_in_bytes = 256};
constexpr QuarantineConfig kConfigSmallThreadSafe = {
    .lock_required = true,
    .branch_capacity_in_bytes = 2048};
constexpr QuarantineConfig kConfigLargeThreadSafe = {
    .lock_required = true,
    .branch_capacity_in_bytes = 2048};
INSTANTIATE_TEST_SUITE_P(PartitionAllocLightweightQuarantineTestInstantiation,
                         PartitionAllocLightweightQuarantineTest,
                         ::testing::Values(kConfigSmall,
                                           kConfigLarge,
                                           kConfigSmallThreadSafe,
                                           kConfigLargeThreadSafe));

}  // namespace

TEST_P(PartitionAllocLightweightQuarantineTest, Basic) {
  constexpr size_t kObjectSize = 1;

  const size_t capacity_in_bytes = GetQuarantineBranch()->GetCapacityInBytes();

  constexpr size_t kCount = 100;
  for (size_t i = 1; i <= kCount; i++) {
    void* object = GetPartitionRoot()->Alloc(kObjectSize);
    const size_t size = GetObjectSize(object);
    const size_t max_count = capacity_in_bytes / size;

    const bool success = Quarantine(object);

    ASSERT_TRUE(success);
    ASSERT_TRUE(GetQuarantineBranch()->IsQuarantinedForTesting(object));

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
  const size_t capacity_in_bytes = GetQuarantineBranch()->GetCapacityInBytes();

  void* object = GetPartitionRoot()->Alloc(kObjectSize);
  const size_t size = GetObjectSize(object);
  ASSERT_GT(size, capacity_in_bytes);

  const bool success = Quarantine(object);

  ASSERT_FALSE(success);
  ASSERT_FALSE(GetQuarantineBranch()->IsQuarantinedForTesting(object));

  auto stats = GetStats();
  ASSERT_EQ(0u, stats.size_in_bytes);
  ASSERT_EQ(0u, stats.count);
  ASSERT_EQ(0u, stats.cumulative_size_in_bytes);
  ASSERT_EQ(0u, stats.cumulative_count);
}

namespace {

using internal::SchedulerLoopQuarantineBranch;
using internal::SchedulerLoopQuarantineConfig;

class PartitionAllocSchedulerLoopQuarantineTest
    : public testing::TestWithParam<QuarantineConfig> {
 protected:
  void SetUp() override {
    allocator_ =
        std::make_unique<PartitionAllocatorForTesting>(PartitionOptions{});
    root_.emplace(*allocator_->root());
    scoped_tcache_swap_.emplace(allocator_->root());

    ThreadCache* tcache = ThreadCache::Get();
    ASSERT_TRUE(ThreadCache::IsValid(tcache));

    branch_ = &tcache->GetSchedulerLoopQuarantineBranch();

    SchedulerLoopQuarantineConfig config = {
        .quarantine_config = GetParam(),
        .enable_quarantine = true,
        .enable_zapping = true,
    };
    branch_->Configure(*root_, config);

    auto stats = GetStats();
    ASSERT_EQ(0u, stats.size_in_bytes);
    ASSERT_EQ(0u, stats.count);
    ASSERT_EQ(0u, stats.cumulative_size_in_bytes);
    ASSERT_EQ(0u, stats.cumulative_count);
  }

  void TearDown() override {
    branch_ = nullptr;
    root_.reset();
    // |Purge()|d here.
    scoped_tcache_swap_.reset();
    allocator_ = nullptr;
  }

  PartitionRoot* GetPartitionRoot() const { return allocator_->root(); }

  QuarantineRoot* GetQuarantineRoot() { return &root_.value(); }
  SchedulerLoopQuarantineBranch* GetQuarantineBranch() { return branch_; }
  QuarantineBranch* GetInternalBranch() {
    return &GetQuarantineBranch()->GetInternalBranchForTesting();
  }

  void Quarantine(void* object) {
    auto* slot_span = internal::SlotSpanMetadata<
        internal::MetadataKind::kReadOnly>::FromObject(object);
    uintptr_t slot_start = GetPartitionRoot()->ObjectToSlotStart(object);
    size_t usable_size = GetPartitionRoot()->GetSlotUsableSize(slot_span);
    GetQuarantineBranch()->QuarantineWithAcquiringLock(object, slot_span,
                                                       slot_start, usable_size);
  }

  size_t GetObjectSize(void* object) {
    auto* entry_slot_span = internal::SlotSpanMetadata<
        internal::MetadataKind::kReadOnly>::FromObject(object);
    return GetPartitionRoot()->GetSlotUsableSize(entry_slot_span);
  }

  LightweightQuarantineStats GetStats() const {
    LightweightQuarantineStats stats{};
    root_->AccumulateStats(stats);
    return stats;
  }

  std::unique_ptr<PartitionAllocatorForTesting> allocator_;
  std::optional<QuarantineRoot> root_;
  SchedulerLoopQuarantineBranch* branch_;
  std::optional<internal::ThreadCacheProcessScopeForTesting>
      scoped_tcache_swap_;
};

INSTANTIATE_TEST_SUITE_P(PartitionAllocSchedulerLoopQuarantineTestInstantiation,
                         PartitionAllocSchedulerLoopQuarantineTest,
                         ::testing::Values(kConfigSmallThreadSafe,
                                           kConfigLargeThreadSafe));

TEST_P(PartitionAllocSchedulerLoopQuarantineTest, Basic) {
  constexpr size_t kObjectSize = 1;

  const size_t capacity_in_bytes = GetInternalBranch()->GetCapacityInBytes();

  constexpr size_t kCount = 100;
  for (size_t i = 1; i <= kCount; i++) {
    void* object = GetPartitionRoot()->Alloc(kObjectSize);
    const size_t size = GetObjectSize(object);
    const size_t max_count = capacity_in_bytes / size;

    Quarantine(object);
    ASSERT_TRUE(GetInternalBranch()->IsQuarantinedForTesting(object));

    const auto expected_count = std::min(i, max_count);
    auto stats = GetStats();
    ASSERT_EQ(expected_count * size, stats.size_in_bytes);
    ASSERT_EQ(expected_count, stats.count);
    ASSERT_EQ(i * size, stats.cumulative_size_in_bytes);
    ASSERT_EQ(i, stats.cumulative_count);
  }
}

TEST_P(PartitionAllocSchedulerLoopQuarantineTest, ScopedOptOut) {
  constexpr size_t kObjectSize = 1;
  void* object1 = GetPartitionRoot()->Alloc(kObjectSize);
  void* object2 = GetPartitionRoot()->Alloc(kObjectSize);

  {
    ScopedSchedulerLoopQuarantineExclusion opt_out;

    Quarantine(object1);
    ASSERT_FALSE(GetInternalBranch()->IsQuarantinedForTesting(object1));
  }

  Quarantine(object2);
  ASSERT_TRUE(GetInternalBranch()->IsQuarantinedForTesting(object2));
}

}  // namespace

#endif  // !defined(MEMORY_TOOL_REPLACES_ALLOCATOR)

}  // namespace partition_alloc
