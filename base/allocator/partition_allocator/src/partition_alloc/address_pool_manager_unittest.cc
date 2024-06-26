// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "partition_alloc/address_pool_manager.h"

#include <cstdint>

#include "partition_alloc/address_space_stats.h"
#include "partition_alloc/build_config.h"
#include "partition_alloc/buildflags.h"
#include "partition_alloc/page_allocator.h"
#include "partition_alloc/partition_alloc_base/bits.h"
#include "partition_alloc/partition_alloc_constants.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace partition_alloc::internal {

class AddressSpaceStatsDumperForTesting final : public AddressSpaceStatsDumper {
 public:
  AddressSpaceStatsDumperForTesting() = default;
  ~AddressSpaceStatsDumperForTesting() final = default;

  void DumpStats(
      const partition_alloc::AddressSpaceStats* address_space_stats) override {
    regular_pool_usage_ = address_space_stats->regular_pool_stats.usage;
#if PA_BUILDFLAG(HAS_64_BIT_POINTERS)
    regular_pool_largest_reservation_ =
        address_space_stats->regular_pool_stats.largest_available_reservation;
#endif
#if !PA_BUILDFLAG(HAS_64_BIT_POINTERS) && \
    PA_BUILDFLAG(ENABLE_BACKUP_REF_PTR_SUPPORT)
    blocklist_size_ = address_space_stats->blocklist_size;
#endif
  }

  size_t regular_pool_usage_ = 0;
  size_t regular_pool_largest_reservation_ = 0;
  size_t blocklist_size_ = 0;
};

#if PA_BUILDFLAG(HAS_64_BIT_POINTERS)

class AddressPoolManagerForTesting : public AddressPoolManager {
 public:
  AddressPoolManagerForTesting() = default;
  ~AddressPoolManagerForTesting() = default;
};

class PartitionAllocAddressPoolManagerTest : public testing::Test {
 protected:
  PartitionAllocAddressPoolManagerTest() = default;
  ~PartitionAllocAddressPoolManagerTest() override = default;

  void SetUp() override {
    manager_ = std::make_unique<AddressPoolManagerForTesting>();
    base_address_ =
        AllocPages(kPoolSize, kSuperPageSize,
                   PageAccessibilityConfiguration(
                       PageAccessibilityConfiguration::kInaccessible),
                   PageTag::kPartitionAlloc);
    ASSERT_TRUE(base_address_);
    manager_->Add(kRegularPoolHandle, base_address_, kPoolSize);
    pool_ = kRegularPoolHandle;
  }

  void TearDown() override {
    manager_->Remove(pool_);
    FreePages(base_address_, kPoolSize);
    manager_.reset();
  }

  AddressPoolManager* GetAddressPoolManager() { return manager_.get(); }

  static constexpr size_t kPoolSize = kPoolMaxSize;
  static constexpr size_t kPageCnt = kPoolSize / kSuperPageSize;

  std::unique_ptr<AddressPoolManagerForTesting> manager_;
  uintptr_t base_address_;
  pool_handle pool_;
};

TEST_F(PartitionAllocAddressPoolManagerTest, TooLargePool) {
  uintptr_t base_addr = 0x4200000;
  const pool_handle extra_pool = static_cast<pool_handle>(2u);
  static_assert(kNumPools >= 2);

  EXPECT_DEATH_IF_SUPPORTED(
      GetAddressPoolManager()->Add(extra_pool, base_addr,
                                   kPoolSize + kSuperPageSize),
      "");
}

TEST_F(PartitionAllocAddressPoolManagerTest, ManyPages) {
  EXPECT_EQ(
      GetAddressPoolManager()->Reserve(pool_, 0, kPageCnt * kSuperPageSize),
      base_address_);
  EXPECT_EQ(GetAddressPoolManager()->Reserve(pool_, 0, kSuperPageSize), 0u);
  GetAddressPoolManager()->UnreserveAndDecommit(pool_, base_address_,
                                                kPageCnt * kSuperPageSize);

  EXPECT_EQ(
      GetAddressPoolManager()->Reserve(pool_, 0, kPageCnt * kSuperPageSize),
      base_address_);
  GetAddressPoolManager()->UnreserveAndDecommit(pool_, base_address_,
                                                kPageCnt * kSuperPageSize);
}

TEST_F(PartitionAllocAddressPoolManagerTest, PagesFragmented) {
  uintptr_t addrs[kPageCnt];
  for (size_t i = 0; i < kPageCnt; ++i) {
    addrs[i] = GetAddressPoolManager()->Reserve(pool_, 0, kSuperPageSize);
    EXPECT_EQ(addrs[i], base_address_ + i * kSuperPageSize);
  }
  EXPECT_EQ(GetAddressPoolManager()->Reserve(pool_, 0, kSuperPageSize), 0u);
  // Free other other super page, so that we have plenty of free space, but none
  // of the empty spaces can fit 2 super pages.
  for (size_t i = 1; i < kPageCnt; i += 2) {
    GetAddressPoolManager()->UnreserveAndDecommit(pool_, addrs[i],
                                                  kSuperPageSize);
  }
  EXPECT_EQ(GetAddressPoolManager()->Reserve(pool_, 0, 2 * kSuperPageSize), 0u);
  // Reserve freed super pages back, so that there are no free ones.
  for (size_t i = 1; i < kPageCnt; i += 2) {
    addrs[i] = GetAddressPoolManager()->Reserve(pool_, 0, kSuperPageSize);
    EXPECT_EQ(addrs[i], base_address_ + i * kSuperPageSize);
  }
  EXPECT_EQ(GetAddressPoolManager()->Reserve(pool_, 0, kSuperPageSize), 0u);
  // Lastly, clean up.
  for (uintptr_t addr : addrs) {
    GetAddressPoolManager()->UnreserveAndDecommit(pool_, addr, kSuperPageSize);
  }
}

TEST_F(PartitionAllocAddressPoolManagerTest, GetUsedSuperpages) {
  uintptr_t addrs[kPageCnt];
  for (size_t i = 0; i < kPageCnt; ++i) {
    addrs[i] = GetAddressPoolManager()->Reserve(pool_, 0, kSuperPageSize);
    EXPECT_EQ(addrs[i], base_address_ + i * kSuperPageSize);
  }
  EXPECT_EQ(GetAddressPoolManager()->Reserve(pool_, 0, kSuperPageSize), 0u);

  std::bitset<kMaxSuperPagesInPool> used_super_pages;
  GetAddressPoolManager()->GetPoolUsedSuperPages(pool_, used_super_pages);

  // We expect every bit to be set.
  for (size_t i = 0; i < kPageCnt; ++i) {
    ASSERT_TRUE(used_super_pages.test(i));
  }

  // Free every other super page, so that we have plenty of free space, but none
  // of the empty spaces can fit 2 super pages.
  for (size_t i = 1; i < kPageCnt; i += 2) {
    GetAddressPoolManager()->UnreserveAndDecommit(pool_, addrs[i],
                                                  kSuperPageSize);
  }

  EXPECT_EQ(GetAddressPoolManager()->Reserve(pool_, 0, 2 * kSuperPageSize), 0u);

  GetAddressPoolManager()->GetPoolUsedSuperPages(pool_, used_super_pages);

  // We expect every other bit to be set.
  for (size_t i = 0; i < kPageCnt; i++) {
    if (i % 2 == 0) {
      ASSERT_TRUE(used_super_pages.test(i));
    } else {
      ASSERT_FALSE(used_super_pages.test(i));
    }
  }

  // Free the even numbered super pages.
  for (size_t i = 0; i < kPageCnt; i += 2) {
    GetAddressPoolManager()->UnreserveAndDecommit(pool_, addrs[i],
                                                  kSuperPageSize);
  }

  // Finally check to make sure all bits are zero in the used superpage bitset.
  GetAddressPoolManager()->GetPoolUsedSuperPages(pool_, used_super_pages);

  for (size_t i = 0; i < kPageCnt; i++) {
    ASSERT_FALSE(used_super_pages.test(i));
  }
}

TEST_F(PartitionAllocAddressPoolManagerTest, IrregularPattern) {
  uintptr_t a1 = GetAddressPoolManager()->Reserve(pool_, 0, kSuperPageSize);
  EXPECT_EQ(a1, base_address_);
  uintptr_t a2 = GetAddressPoolManager()->Reserve(pool_, 0, 2 * kSuperPageSize);
  EXPECT_EQ(a2, base_address_ + 1 * kSuperPageSize);
  uintptr_t a3 = GetAddressPoolManager()->Reserve(pool_, 0, 3 * kSuperPageSize);
  EXPECT_EQ(a3, base_address_ + 3 * kSuperPageSize);
  uintptr_t a4 = GetAddressPoolManager()->Reserve(pool_, 0, 4 * kSuperPageSize);
  EXPECT_EQ(a4, base_address_ + 6 * kSuperPageSize);
  uintptr_t a5 = GetAddressPoolManager()->Reserve(pool_, 0, 5 * kSuperPageSize);
  EXPECT_EQ(a5, base_address_ + 10 * kSuperPageSize);

  GetAddressPoolManager()->UnreserveAndDecommit(pool_, a4, 4 * kSuperPageSize);
  uintptr_t a6 = GetAddressPoolManager()->Reserve(pool_, 0, 6 * kSuperPageSize);
  EXPECT_EQ(a6, base_address_ + 15 * kSuperPageSize);

  GetAddressPoolManager()->UnreserveAndDecommit(pool_, a5, 5 * kSuperPageSize);
  uintptr_t a7 = GetAddressPoolManager()->Reserve(pool_, 0, 7 * kSuperPageSize);
  EXPECT_EQ(a7, base_address_ + 6 * kSuperPageSize);
  uintptr_t a8 = GetAddressPoolManager()->Reserve(pool_, 0, 3 * kSuperPageSize);
  EXPECT_EQ(a8, base_address_ + 21 * kSuperPageSize);
  uintptr_t a9 = GetAddressPoolManager()->Reserve(pool_, 0, 2 * kSuperPageSize);
  EXPECT_EQ(a9, base_address_ + 13 * kSuperPageSize);

  GetAddressPoolManager()->UnreserveAndDecommit(pool_, a7, 7 * kSuperPageSize);
  GetAddressPoolManager()->UnreserveAndDecommit(pool_, a9, 2 * kSuperPageSize);
  GetAddressPoolManager()->UnreserveAndDecommit(pool_, a6, 6 * kSuperPageSize);
  uintptr_t a10 =
      GetAddressPoolManager()->Reserve(pool_, 0, 15 * kSuperPageSize);
  EXPECT_EQ(a10, base_address_ + 6 * kSuperPageSize);

  // Clean up.
  GetAddressPoolManager()->UnreserveAndDecommit(pool_, a1, kSuperPageSize);
  GetAddressPoolManager()->UnreserveAndDecommit(pool_, a2, 2 * kSuperPageSize);
  GetAddressPoolManager()->UnreserveAndDecommit(pool_, a3, 3 * kSuperPageSize);
  GetAddressPoolManager()->UnreserveAndDecommit(pool_, a8, 3 * kSuperPageSize);
  GetAddressPoolManager()->UnreserveAndDecommit(pool_, a10,
                                                15 * kSuperPageSize);
}

TEST_F(PartitionAllocAddressPoolManagerTest, DecommittedDataIsErased) {
  uintptr_t address =
      GetAddressPoolManager()->Reserve(pool_, 0, kSuperPageSize);
  ASSERT_TRUE(address);
  RecommitSystemPages(address, kSuperPageSize,
                      PageAccessibilityConfiguration(
                          PageAccessibilityConfiguration::kReadWrite),
                      PageAccessibilityDisposition::kRequireUpdate);

  memset(reinterpret_cast<void*>(address), 42, kSuperPageSize);
  GetAddressPoolManager()->UnreserveAndDecommit(pool_, address, kSuperPageSize);

  uintptr_t address2 =
      GetAddressPoolManager()->Reserve(pool_, 0, kSuperPageSize);
  ASSERT_EQ(address, address2);
  RecommitSystemPages(address2, kSuperPageSize,
                      PageAccessibilityConfiguration(
                          PageAccessibilityConfiguration::kReadWrite),
                      PageAccessibilityDisposition::kRequireUpdate);

  uint32_t sum = 0;
  for (size_t i = 0; i < kSuperPageSize; i++) {
    sum += reinterpret_cast<uint8_t*>(address2)[i];
  }
  EXPECT_EQ(0u, sum) << sum / 42 << " bytes were not zeroed";

  GetAddressPoolManager()->UnreserveAndDecommit(pool_, address2,
                                                kSuperPageSize);
}

TEST_F(PartitionAllocAddressPoolManagerTest, RegularPoolUsageChanges) {
  AddressSpaceStatsDumperForTesting dumper{};

  GetAddressPoolManager()->DumpStats(&dumper);
  ASSERT_EQ(dumper.regular_pool_usage_, 0ull);
  ASSERT_EQ(dumper.regular_pool_largest_reservation_, kPageCnt);

  // Bisect the pool by reserving a super page in the middle.
  const uintptr_t midpoint_address =
      base_address_ + (kPageCnt / 2) * kSuperPageSize;
  ASSERT_EQ(
      GetAddressPoolManager()->Reserve(pool_, midpoint_address, kSuperPageSize),
      midpoint_address);

  GetAddressPoolManager()->DumpStats(&dumper);
  ASSERT_EQ(dumper.regular_pool_usage_, 1ull);
  ASSERT_EQ(dumper.regular_pool_largest_reservation_, kPageCnt / 2);

  GetAddressPoolManager()->UnreserveAndDecommit(pool_, midpoint_address,
                                                kSuperPageSize);

  GetAddressPoolManager()->DumpStats(&dumper);
  ASSERT_EQ(dumper.regular_pool_usage_, 0ull);
  ASSERT_EQ(dumper.regular_pool_largest_reservation_, kPageCnt);
}

#else  // PA_BUILDFLAG(HAS_64_BIT_POINTERS)

TEST(PartitionAllocAddressPoolManagerTest, IsManagedByRegularPool) {
  constexpr size_t kAllocCount = 8;
  static const size_t kNumPages[kAllocCount] = {1, 4, 7, 8, 13, 16, 31, 60};
  uintptr_t addrs[kAllocCount];
  for (size_t i = 0; i < kAllocCount; ++i) {
    addrs[i] = AddressPoolManager::GetInstance().Reserve(
        kRegularPoolHandle, 0,
        AddressPoolManagerBitmap::kBytesPer1BitOfRegularPoolBitmap *
            kNumPages[i]);
    EXPECT_TRUE(addrs[i]);
    EXPECT_TRUE(!(addrs[i] & kSuperPageOffsetMask));
    AddressPoolManager::GetInstance().MarkUsed(
        kRegularPoolHandle, addrs[i],
        AddressPoolManagerBitmap::kBytesPer1BitOfRegularPoolBitmap *
            kNumPages[i]);
  }
  for (size_t i = 0; i < kAllocCount; ++i) {
    uintptr_t address = addrs[i];
    size_t num_pages =
        base::bits::AlignUp(
            kNumPages[i] *
                AddressPoolManagerBitmap::kBytesPer1BitOfRegularPoolBitmap,
            kSuperPageSize) /
        AddressPoolManagerBitmap::kBytesPer1BitOfRegularPoolBitmap;
    for (size_t j = 0; j < num_pages; ++j) {
      if (j < kNumPages[i]) {
        EXPECT_TRUE(AddressPoolManager::IsManagedByRegularPool(address));
      } else {
        EXPECT_FALSE(AddressPoolManager::IsManagedByRegularPool(address));
      }
      EXPECT_FALSE(AddressPoolManager::IsManagedByBRPPool(address));
      address += AddressPoolManagerBitmap::kBytesPer1BitOfRegularPoolBitmap;
    }
  }
  for (size_t i = 0; i < kAllocCount; ++i) {
    AddressPoolManager::GetInstance().MarkUnused(
        kRegularPoolHandle, addrs[i],
        AddressPoolManagerBitmap::kBytesPer1BitOfRegularPoolBitmap *
            kNumPages[i]);
    AddressPoolManager::GetInstance().UnreserveAndDecommit(
        kRegularPoolHandle, addrs[i],
        AddressPoolManagerBitmap::kBytesPer1BitOfRegularPoolBitmap *
            kNumPages[i]);
    EXPECT_FALSE(AddressPoolManager::IsManagedByRegularPool(addrs[i]));
    EXPECT_FALSE(AddressPoolManager::IsManagedByBRPPool(addrs[i]));
  }
}

#if PA_BUILDFLAG(ENABLE_BACKUP_REF_PTR_SUPPORT)
TEST(PartitionAllocAddressPoolManagerTest, IsManagedByBRPPool) {
  constexpr size_t kAllocCount = 4;
  // Totally (1+3+7+11) * 2MB = 44MB allocation
  static const size_t kNumPages[kAllocCount] = {1, 3, 7, 11};
  uintptr_t addrs[kAllocCount];
  for (size_t i = 0; i < kAllocCount; ++i) {
    addrs[i] = AddressPoolManager::GetInstance().Reserve(
        kBRPPoolHandle, 0, kSuperPageSize * kNumPages[i]);
    EXPECT_TRUE(addrs[i]);
    EXPECT_TRUE(!(addrs[i] & kSuperPageOffsetMask));
    AddressPoolManager::GetInstance().MarkUsed(kBRPPoolHandle, addrs[i],
                                               kSuperPageSize * kNumPages[i]);
  }

  constexpr size_t first_guard_size =
      AddressPoolManagerBitmap::kBytesPer1BitOfBRPPoolBitmap *
      AddressPoolManagerBitmap::kGuardOffsetOfBRPPoolBitmap;
  constexpr size_t last_guard_size =
      AddressPoolManagerBitmap::kBytesPer1BitOfBRPPoolBitmap *
      (AddressPoolManagerBitmap::kGuardBitsOfBRPPoolBitmap -
       AddressPoolManagerBitmap::kGuardOffsetOfBRPPoolBitmap);

  for (size_t i = 0; i < kAllocCount; ++i) {
    uintptr_t address = addrs[i];
    size_t num_allocated_size = kNumPages[i] * kSuperPageSize;
    size_t num_system_pages = num_allocated_size / SystemPageSize();
    for (size_t j = 0; j < num_system_pages; ++j) {
      size_t offset = address - addrs[i];
      if (offset < first_guard_size ||
          offset >= (num_allocated_size - last_guard_size)) {
        EXPECT_FALSE(AddressPoolManager::IsManagedByBRPPool(address));
      } else {
        EXPECT_TRUE(AddressPoolManager::IsManagedByBRPPool(address));
      }
      EXPECT_FALSE(AddressPoolManager::IsManagedByRegularPool(address));
      address += SystemPageSize();
    }
  }
  for (size_t i = 0; i < kAllocCount; ++i) {
    AddressPoolManager::GetInstance().MarkUnused(kBRPPoolHandle, addrs[i],
                                                 kSuperPageSize * kNumPages[i]);
    AddressPoolManager::GetInstance().UnreserveAndDecommit(
        kBRPPoolHandle, addrs[i], kSuperPageSize * kNumPages[i]);
    EXPECT_FALSE(AddressPoolManager::IsManagedByRegularPool(addrs[i]));
    EXPECT_FALSE(AddressPoolManager::IsManagedByBRPPool(addrs[i]));
  }
}
#endif  // PA_BUILDFLAG(ENABLE_BACKUP_REF_PTR_SUPPORT)

TEST(PartitionAllocAddressPoolManagerTest, RegularPoolUsageChanges) {
  AddressSpaceStatsDumperForTesting dumper{};
  AddressPoolManager::GetInstance().DumpStats(&dumper);
  const size_t usage_before = dumper.regular_pool_usage_;

  const uintptr_t address = AddressPoolManager::GetInstance().Reserve(
      kRegularPoolHandle, 0, kSuperPageSize);
  ASSERT_TRUE(address);
  AddressPoolManager::GetInstance().MarkUsed(kRegularPoolHandle, address,
                                             kSuperPageSize);

  AddressPoolManager::GetInstance().DumpStats(&dumper);
  EXPECT_GT(dumper.regular_pool_usage_, usage_before);

  AddressPoolManager::GetInstance().MarkUnused(kRegularPoolHandle, address,
                                               kSuperPageSize);
  AddressPoolManager::GetInstance().UnreserveAndDecommit(
      kRegularPoolHandle, address, kSuperPageSize);

  AddressPoolManager::GetInstance().DumpStats(&dumper);
  EXPECT_EQ(dumper.regular_pool_usage_, usage_before);
}

#endif  // PA_BUILDFLAG(HAS_64_BIT_POINTERS)

}  // namespace partition_alloc::internal
