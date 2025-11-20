// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "partition_alloc/slot_start.h"
#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/40284755): Remove this and spanify to fix the errors.
#pragma allow_unsafe_buffers
#endif

#include "partition_alloc/scheduler_loop_quarantine.h"

#include "partition_alloc/extended_api.h"
#include "partition_alloc/partition_alloc_base/check.h"
#include "partition_alloc/partition_alloc_for_testing.h"
#include "partition_alloc/partition_page.h"
#include "partition_alloc/partition_root.h"
#include "partition_alloc/partition_stats.h"
#include "partition_alloc/scheduler_loop_quarantine_support.h"
#include "partition_alloc/thread_cache.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace partition_alloc {

#if !defined(MEMORY_TOOL_REPLACES_ALLOCATOR)

namespace {

template <bool thread_bound>
internal::SchedulerLoopQuarantineBranch<thread_bound>*
GetBranchFromAllocatorRoot(PartitionRoot* root);

template <>
internal::GlobalSchedulerLoopQuarantineBranch*
GetBranchFromAllocatorRoot<false>(PartitionRoot* root) {
  return &root->scheduler_loop_quarantine;
}

template <>
internal::ThreadBoundSchedulerLoopQuarantineBranch*
GetBranchFromAllocatorRoot<true>(PartitionRoot* root) {
  ThreadCache* tcache = ThreadCache::Get();
  PA_CHECK(ThreadCache::IsValid(tcache));
  PA_CHECK(root->settings.with_thread_cache);
  return &tcache->GetSchedulerLoopQuarantineBranch();
}

using QuarantineConfig = internal::SchedulerLoopQuarantineConfig;
using QuarantineRoot = internal::SchedulerLoopQuarantineRoot;

template <typename Param>
class SchedulerLoopQuarantineTest : public testing::Test {
  using QuarantineBranch = Param::BranchImpl;

 protected:
  constexpr QuarantineConfig GetConfig() { return Param::kConfig; }

  void SetUp() override {
    allocator_ =
        std::make_unique<PartitionAllocatorForTesting>(PartitionOptions{});
    root_.emplace(*allocator_->root());
    if constexpr (QuarantineBranch::kThreadBound) {
      scoped_tcache_swap_.emplace(allocator_->root());
    }
    branch_ = GetBranchFromAllocatorRoot<QuarantineBranch::kThreadBound>(
        allocator_->root());
    branch_->Configure(*root_, GetConfig());

    auto stats = GetStats();
    ASSERT_EQ(0u, stats.size_in_bytes);
    ASSERT_EQ(0u, stats.count);
    ASSERT_EQ(0u, stats.cumulative_size_in_bytes);
    ASSERT_EQ(0u, stats.cumulative_count);
  }

  void TearDown() override {
    branch_->Purge();
    branch_ = nullptr;
    root_.reset();
    scoped_tcache_swap_.reset();
    allocator_ = nullptr;
  }

  PartitionRoot* GetPartitionRoot() const { return allocator_->root(); }

  QuarantineRoot* GetQuarantineRoot() { return &root_.value(); }
  QuarantineBranch* GetQuarantineBranch() { return branch_; }

  void Quarantine(void* object) {
    internal::SlotStart slot_start = internal::SlotStart::Unchecked(object);
    auto* slot_span = internal::SlotSpanMetadata::FromSlotStart(
        slot_start.Untag(), GetPartitionRoot());
    GetQuarantineBranch()->Quarantine(slot_start, slot_span);
  }

  size_t GetObjectSize(void* object) {
    internal::SlotStart slot_start = internal::SlotStart::Unchecked(object);
    auto* entry_slot_span = internal::SlotSpanMetadata::FromSlotStart(
        slot_start.Untag(), GetPartitionRoot());
    return entry_slot_span->bucket->slot_size;
  }

  SchedulerLoopQuarantineStats GetStats() const {
    SchedulerLoopQuarantineStats stats{};
    root_->AccumulateStats(stats);
    return stats;
  }

  std::unique_ptr<PartitionAllocatorForTesting> allocator_;
  std::optional<QuarantineRoot> root_;
  QuarantineBranch* branch_;
  std::optional<internal::ThreadCacheProcessScopeForTesting>
      scoped_tcache_swap_;
};

struct SchedulerLoopQuarantineTestParamSmall {
  using BranchImpl = internal::GlobalSchedulerLoopQuarantineBranch;
  constexpr static QuarantineConfig kConfig = {
      .branch_capacity_in_bytes = 256,
      .enable_quarantine = true,
      .enable_zapping = true,
      .max_quarantine_size = 1024,
  };
};
struct SchedulerLoopQuarantineTestParamLarge {
  using BranchImpl = internal::GlobalSchedulerLoopQuarantineBranch;
  constexpr static QuarantineConfig kConfig = {
      .branch_capacity_in_bytes = 2048,
      .enable_quarantine = true,
      .enable_zapping = true,
      .max_quarantine_size = 1024,
  };
};
struct SchedulerLoopQuarantineTestParamSmallThreadBound {
  using BranchImpl = internal::ThreadBoundSchedulerLoopQuarantineBranch;
  constexpr static QuarantineConfig kConfig = {
      .branch_capacity_in_bytes = 256,
      .enable_quarantine = true,
      .enable_zapping = true,
      .max_quarantine_size = 1024,
  };
};
struct SchedulerLoopQuarantineTestParamLargeThreadBound {
  using BranchImpl = internal::ThreadBoundSchedulerLoopQuarantineBranch;
  constexpr static QuarantineConfig kConfig = {
      .branch_capacity_in_bytes = 2048,
      .enable_quarantine = true,
      .enable_zapping = true,
      .max_quarantine_size = 1024,
  };
};

using SchedulerLoopQuarantineTestParams =
    ::testing::Types<SchedulerLoopQuarantineTestParamSmall,
                     SchedulerLoopQuarantineTestParamLarge,
                     SchedulerLoopQuarantineTestParamSmallThreadBound,
                     SchedulerLoopQuarantineTestParamLargeThreadBound>;
TYPED_TEST_SUITE(SchedulerLoopQuarantineTest,
                 SchedulerLoopQuarantineTestParams);

TYPED_TEST(SchedulerLoopQuarantineTest, Basic) {
  constexpr size_t kObjectSize = 1;

  const size_t capacity_in_bytes =
      this->GetQuarantineBranch()->GetCapacityInBytes();

  constexpr size_t kCount = 100;
  for (size_t i = 1; i <= kCount; i++) {
    void* object = this->GetPartitionRoot()->Alloc(kObjectSize);
    const size_t size = this->GetObjectSize(object);
    const size_t max_count = capacity_in_bytes / size;

    this->Quarantine(object);
    ASSERT_TRUE(this->GetQuarantineBranch()->IsQuarantinedForTesting(object));

    const auto expected_count = std::min(i, max_count);
    auto stats = this->GetStats();
    ASSERT_EQ(expected_count * size, stats.size_in_bytes);
    ASSERT_EQ(expected_count, stats.count);
    ASSERT_EQ(i * size, stats.cumulative_size_in_bytes);
    ASSERT_EQ(i, stats.cumulative_count);
  }
}

TYPED_TEST(SchedulerLoopQuarantineTest, TooLargeAllocation) {
  const size_t kThreshold =
      std::min(this->GetConfig().max_quarantine_size,
               this->GetConfig().branch_capacity_in_bytes);

  void* object = this->GetPartitionRoot()->Alloc(kThreshold + 1);
  size_t size = this->GetObjectSize(object);
  ASSERT_GT(size, kThreshold);

  this->Quarantine(object);

  ASSERT_FALSE(this->GetQuarantineBranch()->IsQuarantinedForTesting(object));

  auto stats = this->GetStats();
  ASSERT_EQ(0u, stats.size_in_bytes);
  ASSERT_EQ(0u, stats.count);
  ASSERT_EQ(0u, stats.cumulative_size_in_bytes);
  ASSERT_EQ(0u, stats.cumulative_count);

  // -32 to ensure it falls into a bucket with size under the threshold.
  object = this->GetPartitionRoot()->Alloc(kThreshold - 32);
  size = this->GetObjectSize(object);
  ASSERT_LE(size, kThreshold);

  this->Quarantine(object);

  ASSERT_TRUE(this->GetQuarantineBranch()->IsQuarantinedForTesting(object));
}

TYPED_TEST(SchedulerLoopQuarantineTest, ScopedOptOut) {
  if (!this->GetQuarantineBranch()->kThreadBound) {
    // Supported only on lock-less mode.
    GTEST_SKIP();
  }

  constexpr size_t kObjectSize = 1;
  void* object1 = this->GetPartitionRoot()->Alloc(kObjectSize);
  void* object2 = this->GetPartitionRoot()->Alloc(kObjectSize);

  {
    ScopedSchedulerLoopQuarantineExclusion opt_out;

    this->Quarantine(object1);
    ASSERT_FALSE(this->GetQuarantineBranch()->IsQuarantinedForTesting(object1));
  }

  this->Quarantine(object2);
  ASSERT_TRUE(this->GetQuarantineBranch()->IsQuarantinedForTesting(object2));
}

}  // namespace

#endif  // !defined(MEMORY_TOOL_REPLACES_ALLOCATOR)

}  // namespace partition_alloc
