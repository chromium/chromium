// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "partition_alloc/address_pool_manager.h"

#include <array>
#include <atomic>
#include <cstdint>
#include <cstring>

#include "partition_alloc/address_space_stats.h"
#include "partition_alloc/build_config.h"
#include "partition_alloc/buildflags.h"
#include "partition_alloc/page_allocator.h"
#include "partition_alloc/partition_alloc_base/bits.h"
#include "partition_alloc/partition_alloc_base/compiler_specific.h"
#include "partition_alloc/partition_alloc_constants.h"
#include "partition_alloc/partition_alloc_public_constants.h"
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
  using ReservationLifetime = AddressPoolManager::ReservationLifetime;

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
  std::array<uintptr_t, kPageCnt> addrs;
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
  std::array<uintptr_t, kPageCnt> addrs;
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

TEST_F(PartitionAllocAddressPoolManagerTest,
       IrregularPatternNonPersistentReservation) {
  const uintptr_t pool_end = base_address_ + kPoolSize;
  uintptr_t a1 = GetAddressPoolManager()->Reserve(
      pool_, 0, kSuperPageSize, ReservationLifetime::kTransient);
  EXPECT_EQ(a1, pool_end - 1 * kSuperPageSize);
  uintptr_t a2 = GetAddressPoolManager()->Reserve(
      pool_, 0, 2 * kSuperPageSize, ReservationLifetime::kTransient);
  EXPECT_EQ(a2, pool_end - 3 * kSuperPageSize);
  uintptr_t a3 = GetAddressPoolManager()->Reserve(
      pool_, 0, 3 * kSuperPageSize, ReservationLifetime::kTransient);
  EXPECT_EQ(a3, pool_end - 6 * kSuperPageSize);
  uintptr_t a4 = GetAddressPoolManager()->Reserve(
      pool_, 0, 4 * kSuperPageSize, ReservationLifetime::kTransient);
  EXPECT_EQ(a4, pool_end - 10 * kSuperPageSize);
  uintptr_t a5 = GetAddressPoolManager()->Reserve(
      pool_, 0, 5 * kSuperPageSize, ReservationLifetime::kTransient);
  EXPECT_EQ(a5, pool_end - 15 * kSuperPageSize);

  GetAddressPoolManager()->UnreserveAndDecommit(pool_, a4, 4 * kSuperPageSize);
  uintptr_t a6 = GetAddressPoolManager()->Reserve(
      pool_, 0, 6 * kSuperPageSize, ReservationLifetime::kTransient);
  EXPECT_EQ(a6, pool_end - 21 * kSuperPageSize);

  GetAddressPoolManager()->UnreserveAndDecommit(pool_, a5, 5 * kSuperPageSize);
  uintptr_t a7 = GetAddressPoolManager()->Reserve(
      pool_, 0, 7 * kSuperPageSize, ReservationLifetime::kTransient);
  EXPECT_EQ(a7, pool_end - 13 * kSuperPageSize);
  uintptr_t a8 = GetAddressPoolManager()->Reserve(
      pool_, 0, 3 * kSuperPageSize, ReservationLifetime::kTransient);
  EXPECT_EQ(a8, pool_end - 24 * kSuperPageSize);
  uintptr_t a9 = GetAddressPoolManager()->Reserve(
      pool_, 0, 2 * kSuperPageSize, ReservationLifetime::kTransient);
  EXPECT_EQ(a9, pool_end - 15 * kSuperPageSize);

  GetAddressPoolManager()->UnreserveAndDecommit(pool_, a7, 7 * kSuperPageSize);
  GetAddressPoolManager()->UnreserveAndDecommit(pool_, a9, 2 * kSuperPageSize);
  GetAddressPoolManager()->UnreserveAndDecommit(pool_, a6, 6 * kSuperPageSize);
  uintptr_t a10 = GetAddressPoolManager()->Reserve(
      pool_, 0, 15 * kSuperPageSize, ReservationLifetime::kTransient);
  EXPECT_EQ(a10, pool_end - 21 * kSuperPageSize);

  // Clean up.
  GetAddressPoolManager()->UnreserveAndDecommit(pool_, a1, kSuperPageSize);
  GetAddressPoolManager()->UnreserveAndDecommit(pool_, a2, 2 * kSuperPageSize);
  GetAddressPoolManager()->UnreserveAndDecommit(pool_, a3, 3 * kSuperPageSize);
  GetAddressPoolManager()->UnreserveAndDecommit(pool_, a8, 3 * kSuperPageSize);
  GetAddressPoolManager()->UnreserveAndDecommit(pool_, a10,
                                                15 * kSuperPageSize);
}

TEST_F(PartitionAllocAddressPoolManagerTest,
       PersistentAndNonPersistentReservationsDoNotFragment) {
  const uintptr_t pool_end = base_address_ + kPoolSize;

  // Simulate interleaving transient (DirectMap) reservations with persistent
  // (normal bucket) reservations.
  uintptr_t dm1 = GetAddressPoolManager()->Reserve(
      pool_, 0, 4 * kSuperPageSize, ReservationLifetime::kTransient);
  EXPECT_EQ(dm1, pool_end - 4 * kSuperPageSize);

  uintptr_t normal1 = GetAddressPoolManager()->Reserve(
      pool_, 0, kSuperPageSize, ReservationLifetime::kPersistent);
  EXPECT_EQ(normal1, base_address_);

  uintptr_t dm2 = GetAddressPoolManager()->Reserve(
      pool_, 0, 8 * kSuperPageSize, ReservationLifetime::kTransient);
  EXPECT_EQ(dm2, pool_end - 12 * kSuperPageSize);

  uintptr_t normal2 = GetAddressPoolManager()->Reserve(
      pool_, normal1 + kSuperPageSize, kSuperPageSize,
      ReservationLifetime::kPersistent);
  EXPECT_EQ(normal2, base_address_ + kSuperPageSize);

  // Free both transient reservations.
  GetAddressPoolManager()->UnreserveAndDecommit(pool_, dm1, 4 * kSuperPageSize);
  GetAddressPoolManager()->UnreserveAndDecommit(pool_, dm2, 8 * kSuperPageSize);

  // The entire remaining pool (kPageCnt - 2 superpages) should be contiguous!
  AddressSpaceStatsDumperForTesting dumper{};
  GetAddressPoolManager()->DumpStats(&dumper);
  EXPECT_EQ(dumper.regular_pool_usage_, 2ull);
  EXPECT_EQ(dumper.regular_pool_largest_reservation_, kPageCnt - 2);

  uintptr_t huge_dm = GetAddressPoolManager()->Reserve(
      pool_, 0, (kPageCnt - 2) * kSuperPageSize,
      ReservationLifetime::kTransient);
  EXPECT_EQ(huge_dm, base_address_ + 2 * kSuperPageSize);

  // Pool is now completely full from both ends.
  EXPECT_EQ(GetAddressPoolManager()->Reserve(pool_, 0, kSuperPageSize,
                                             ReservationLifetime::kPersistent),
            0u);
  EXPECT_EQ(GetAddressPoolManager()->Reserve(
                pool_, base_address_ + 2 * kSuperPageSize, kSuperPageSize,
                ReservationLifetime::kPersistent),
            0u);
  EXPECT_EQ(GetAddressPoolManager()->Reserve(pool_, 0, kSuperPageSize,
                                             ReservationLifetime::kTransient),
            0u);
  GetAddressPoolManager()->DumpStats(&dumper);
  EXPECT_EQ(dumper.regular_pool_usage_, kPageCnt);
  EXPECT_EQ(dumper.regular_pool_largest_reservation_, 0ull);

  GetAddressPoolManager()->UnreserveAndDecommit(
      pool_, huge_dm, (kPageCnt - 2) * kSuperPageSize);

  // Verify that hints recover properly after the pool was 100% full.
  uintptr_t dm3 = GetAddressPoolManager()->Reserve(
      pool_, 0, kSuperPageSize, ReservationLifetime::kTransient);
  EXPECT_EQ(dm3, pool_end - kSuperPageSize);
  uintptr_t normal3 = GetAddressPoolManager()->Reserve(
      pool_, 0, kSuperPageSize, ReservationLifetime::kPersistent);
  EXPECT_EQ(normal3, base_address_ + 2 * kSuperPageSize);

  GetAddressPoolManager()->UnreserveAndDecommit(pool_, dm3, kSuperPageSize);
  GetAddressPoolManager()->UnreserveAndDecommit(pool_, normal3, kSuperPageSize);
  GetAddressPoolManager()->UnreserveAndDecommit(pool_, normal2, kSuperPageSize);
  GetAddressPoolManager()->UnreserveAndDecommit(pool_, normal1, kSuperPageSize);
}

TEST_F(PartitionAllocAddressPoolManagerTest, DecommittedDataIsErased) {
  uintptr_t address =
      GetAddressPoolManager()->Reserve(pool_, 0, kSuperPageSize);
  ASSERT_TRUE(address);
  RecommitSystemPages(address, kSuperPageSize,
                      PageAccessibilityConfiguration(
                          PageAccessibilityConfiguration::kReadWrite),
                      PageAccessibilityDisposition::kRequireUpdate);

  PA_UNSAFE_TODO(memset(reinterpret_cast<void*>(address), 42, kSuperPageSize));
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
    sum += PA_UNSAFE_TODO(reinterpret_cast<uint8_t*>(address2)[i]);
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

namespace {
std::atomic<int> g_memory_pressure_notification_count{0};
void OnMemoryPressureNotification() {
  g_memory_pressure_notification_count.fetch_add(1, std::memory_order_relaxed);
}
}  // namespace

TEST_F(PartitionAllocAddressPoolManagerTest,
       AsyncSafeMemoryPressureNotification) {
  g_memory_pressure_notification_count.store(0, std::memory_order_relaxed);
  GetAddressPoolManager()->SetAsyncSafeMemoryPressureNotification(
      &OnMemoryPressureNotification);

  EXPECT_FALSE(GetAddressPoolManager()->IsAlmostFull());

  // Reserve just below the default threshold (90%) of the pool.
  const size_t almost_full_pages =
      (kPageCnt * AddressPoolManager::kDefaultAlmostFullPercent + 99) / 100;
  const size_t below_threshold_pages = almost_full_pages - 1;
  uintptr_t addr1 = GetAddressPoolManager()->Reserve(
      pool_, 0, below_threshold_pages * kSuperPageSize);
  ASSERT_NE(addr1, 0u);
  EXPECT_FALSE(GetAddressPoolManager()->IsAlmostFull());
  EXPECT_EQ(
      g_memory_pressure_notification_count.load(std::memory_order_relaxed), 0);

  // Reserve 1 more super page using an explicit requested_address
  // (`TryReserveChunk`) to reach the 90% threshold -> notifies once.
  uintptr_t requested_addr2 = addr1 + below_threshold_pages * kSuperPageSize;
  uintptr_t addr2 =
      GetAddressPoolManager()->Reserve(pool_, requested_addr2, kSuperPageSize);
  ASSERT_EQ(addr2, requested_addr2);
  EXPECT_TRUE(GetAddressPoolManager()->IsAlmostFull());
  EXPECT_EQ(
      g_memory_pressure_notification_count.load(std::memory_order_relaxed), 1);

  // Reserve another super page while >= 90% full -> no additional notification.
  uintptr_t addr3 = GetAddressPoolManager()->Reserve(pool_, 0, kSuperPageSize);
  ASSERT_NE(addr3, 0u);
  EXPECT_TRUE(GetAddressPoolManager()->IsAlmostFull());
  EXPECT_EQ(
      g_memory_pressure_notification_count.load(std::memory_order_relaxed), 1);

  // Unreserve addr3 and addr2 to drop just below 90% (still above 85% low
  // watermark) -> state stays almost full and does not notify.
  GetAddressPoolManager()->UnreserveAndDecommit(pool_, addr3, kSuperPageSize);
  GetAddressPoolManager()->UnreserveAndDecommit(pool_, addr2, kSuperPageSize);
  EXPECT_TRUE(GetAddressPoolManager()->IsAlmostFull());
  EXPECT_EQ(
      g_memory_pressure_notification_count.load(std::memory_order_relaxed), 1);

  // Unreserve enough pages from addr1 to drop to <= 85% (5% below threshold)
  // -> state changes to not almost full and notifies once.
  const size_t low_watermark_pages =
      (kPageCnt * (AddressPoolManager::kDefaultAlmostFullPercent - 5)) / 100;
  const size_t pages_to_unreserve = below_threshold_pages - low_watermark_pages;
  GetAddressPoolManager()->UnreserveAndDecommit(
      pool_, addr1 + low_watermark_pages * kSuperPageSize,
      pages_to_unreserve * kSuperPageSize);
  EXPECT_FALSE(GetAddressPoolManager()->IsAlmostFull());
  EXPECT_EQ(
      g_memory_pressure_notification_count.load(std::memory_order_relaxed), 2);

  // Unreserve remaining pages -> already not almost full, no notification.
  GetAddressPoolManager()->UnreserveAndDecommit(
      pool_, addr1, low_watermark_pages * kSuperPageSize);
  EXPECT_FALSE(GetAddressPoolManager()->IsAlmostFull());
  EXPECT_EQ(
      g_memory_pressure_notification_count.load(std::memory_order_relaxed), 2);
  GetAddressPoolManager()->SetAsyncSafeMemoryPressureNotification(nullptr);
}

TEST_F(PartitionAllocAddressPoolManagerTest,
       AsyncSafeMemoryPressureNotificationMultiplePools) {
  static_assert(kNumPools >= 2);
  constexpr pool_handle kSecondPool = static_cast<pool_handle>(2u);
  uintptr_t second_base_address =
      AllocPages(kPoolSize, kSuperPageSize,
                 PageAccessibilityConfiguration(
                     PageAccessibilityConfiguration::kInaccessible),
                 PageTag::kPartitionAlloc);
  ASSERT_TRUE(second_base_address);
  GetAddressPoolManager()->Add(kSecondPool, second_base_address, kPoolSize);

  g_memory_pressure_notification_count.store(0, std::memory_order_relaxed);
  GetAddressPoolManager()->SetAsyncSafeMemoryPressureNotification(
      &OnMemoryPressureNotification);

  const size_t almost_full_pages =
      (kPageCnt * AddressPoolManager::kDefaultAlmostFullPercent + 99) / 100;

  // 1. Pool 1 reaches 90% -> count transitions 0 -> 1, notifies once.
  uintptr_t pool1_addr = GetAddressPoolManager()->Reserve(
      pool_, 0, almost_full_pages * kSuperPageSize);
  ASSERT_NE(pool1_addr, 0u);
  EXPECT_TRUE(GetAddressPoolManager()->IsAlmostFull());
  EXPECT_EQ(
      g_memory_pressure_notification_count.load(std::memory_order_relaxed), 1);

  // 2. Pool 2 also reaches 90% -> count transitions 1 -> 2, does not notify.
  uintptr_t pool2_addr = GetAddressPoolManager()->Reserve(
      kSecondPool, 0, almost_full_pages * kSuperPageSize);
  ASSERT_NE(pool2_addr, 0u);
  EXPECT_TRUE(GetAddressPoolManager()->IsAlmostFull());
  EXPECT_EQ(
      g_memory_pressure_notification_count.load(std::memory_order_relaxed), 1);

  // 3. Pool 1 frees all its pages -> count transitions 2 -> 1, manager remains
  // almost full (due to Pool 2) and does not notify.
  GetAddressPoolManager()->UnreserveAndDecommit(
      pool_, pool1_addr, almost_full_pages * kSuperPageSize);
  EXPECT_TRUE(GetAddressPoolManager()->IsAlmostFull());
  EXPECT_EQ(
      g_memory_pressure_notification_count.load(std::memory_order_relaxed), 1);

  // 4. Removing Pool 2 while it is the last almost-full pool -> count
  // transitions 1 -> 0, manager is no longer almost full and notifies once.
  GetAddressPoolManager()->Remove(kSecondPool);
  EXPECT_FALSE(GetAddressPoolManager()->IsAlmostFull());
  EXPECT_EQ(
      g_memory_pressure_notification_count.load(std::memory_order_relaxed), 2);

  GetAddressPoolManager()->SetAsyncSafeMemoryPressureNotification(nullptr);
  FreePages(second_base_address, kPoolSize);
}

#else  // PA_BUILDFLAG(HAS_64_BIT_POINTERS)

TEST(PartitionAllocAddressPoolManagerTest, IsManagedByRegularPool) {
  constexpr size_t kAllocCount = 8;
  static const std::array<size_t, kAllocCount> kNumPages = {1,  4,  7,  8,
                                                            13, 16, 31, 60};
  std::array<uintptr_t, kAllocCount> addrs;
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
  static const std::array<size_t, kAllocCount> kNumPages = {1, 3, 7, 11};
  std::array<uintptr_t, kAllocCount> addrs;
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
