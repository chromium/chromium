// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/allocator/partition_allocator/address_pool_manager.h"

#include "base/allocator/partition_allocator/page_allocator.h"
#include "base/allocator/partition_allocator/page_allocator_internal.h"
#include "base/allocator/partition_allocator/partition_alloc_constants.h"
#include "base/bits.h"
#include "build/build_config.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace base {
namespace internal {

#if defined(PA_HAS_64_BITS_POINTERS)

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
        AllocPages(nullptr, kPoolSize, kSuperPageSize, base::PageInaccessible,
                   PageTag::kPartitionAlloc);
    ASSERT_TRUE(base_address_);
    pool_ =
        manager_->Add(reinterpret_cast<uintptr_t>(base_address_), kPoolSize);
  }

  void TearDown() override {
    manager_->Remove(pool_);
    FreePages(base_address_, kPoolSize);
    manager_.reset();
  }

  AddressPoolManager* GetAddressPoolManager() { return manager_.get(); }

  static constexpr size_t kPageCnt = 4096;
  static constexpr size_t kPoolSize = kSuperPageSize * kPageCnt;

  std::unique_ptr<AddressPoolManagerForTesting> manager_;
  void* base_address_;
  pool_handle pool_;
};

TEST_F(PartitionAllocAddressPoolManagerTest, TooLargePool) {
  uintptr_t base_addr = 0x4200000;

  EXPECT_DEATH_IF_SUPPORTED(
      GetAddressPoolManager()->Add(base_addr, kPoolSize + kSuperPageSize), "");
}

TEST_F(PartitionAllocAddressPoolManagerTest, ManyPages) {
  char* base_ptr = reinterpret_cast<char*>(base_address_);

  EXPECT_EQ(GetAddressPoolManager()->Reserve(pool_, nullptr,
                                             kPageCnt * kSuperPageSize),
            base_ptr);
  EXPECT_EQ(GetAddressPoolManager()->Reserve(pool_, nullptr, kSuperPageSize),
            nullptr);
  GetAddressPoolManager()->UnreserveAndDecommit(pool_, base_ptr,
                                                kPageCnt * kSuperPageSize);

  EXPECT_EQ(GetAddressPoolManager()->Reserve(pool_, nullptr,
                                             kPageCnt * kSuperPageSize),
            base_ptr);
  GetAddressPoolManager()->UnreserveAndDecommit(pool_, base_ptr,
                                                kPageCnt * kSuperPageSize);
}

TEST_F(PartitionAllocAddressPoolManagerTest, PagesFragmented) {
  char* base_ptr = reinterpret_cast<char*>(base_address_);
  void* addrs[kPageCnt];
  for (size_t i = 0; i < kPageCnt; ++i) {
    addrs[i] = GetAddressPoolManager()->Reserve(pool_, nullptr, kSuperPageSize);
    EXPECT_EQ(addrs[i], base_ptr + i * kSuperPageSize);
  }
  EXPECT_EQ(GetAddressPoolManager()->Reserve(pool_, nullptr, kSuperPageSize),
            nullptr);
  // Free other other super page, so that we have plenty of free space, but none
  // of the empty spaces can fit 2 super pages.
  for (size_t i = 1; i < kPageCnt; i += 2) {
    GetAddressPoolManager()->UnreserveAndDecommit(pool_, addrs[i],
                                                  kSuperPageSize);
  }
  EXPECT_EQ(
      GetAddressPoolManager()->Reserve(pool_, nullptr, 2 * kSuperPageSize),
      nullptr);
  // Reserve freed super pages back, so that there are no free ones.
  for (size_t i = 1; i < kPageCnt; i += 2) {
    addrs[i] = GetAddressPoolManager()->Reserve(pool_, nullptr, kSuperPageSize);
    EXPECT_EQ(addrs[i], base_ptr + i * kSuperPageSize);
  }
  EXPECT_EQ(GetAddressPoolManager()->Reserve(pool_, nullptr, kSuperPageSize),
            nullptr);
  // Lastly, clean up.
  for (size_t i = 0; i < kPageCnt; ++i) {
    GetAddressPoolManager()->UnreserveAndDecommit(pool_, addrs[i],
                                                  kSuperPageSize);
  }
}

TEST_F(PartitionAllocAddressPoolManagerTest, GetUsedSuperpages) {
  char* base_ptr = reinterpret_cast<char*>(base_address_);
  void* addrs[kPageCnt];
  for (size_t i = 0; i < kPageCnt; ++i) {
    addrs[i] = GetAddressPoolManager()->Reserve(pool_, nullptr, kSuperPageSize);
    EXPECT_EQ(addrs[i], base_ptr + i * kSuperPageSize);
  }
  EXPECT_EQ(GetAddressPoolManager()->Reserve(pool_, nullptr, kSuperPageSize),
            nullptr);

  std::bitset<base::kMaxSuperPages> used_super_pages;
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

  EXPECT_EQ(
      GetAddressPoolManager()->Reserve(pool_, nullptr, 2 * kSuperPageSize),
      nullptr);

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
  char* base_ptr = reinterpret_cast<char*>(base_address_);

  void* a1 = GetAddressPoolManager()->Reserve(pool_, nullptr, kSuperPageSize);
  EXPECT_EQ(a1, base_ptr);
  void* a2 =
      GetAddressPoolManager()->Reserve(pool_, nullptr, 2 * kSuperPageSize);
  EXPECT_EQ(a2, base_ptr + 1 * kSuperPageSize);
  void* a3 =
      GetAddressPoolManager()->Reserve(pool_, nullptr, 3 * kSuperPageSize);
  EXPECT_EQ(a3, base_ptr + 3 * kSuperPageSize);
  void* a4 =
      GetAddressPoolManager()->Reserve(pool_, nullptr, 4 * kSuperPageSize);
  EXPECT_EQ(a4, base_ptr + 6 * kSuperPageSize);
  void* a5 =
      GetAddressPoolManager()->Reserve(pool_, nullptr, 5 * kSuperPageSize);
  EXPECT_EQ(a5, base_ptr + 10 * kSuperPageSize);

  GetAddressPoolManager()->UnreserveAndDecommit(pool_, a4, 4 * kSuperPageSize);
  void* a6 =
      GetAddressPoolManager()->Reserve(pool_, nullptr, 6 * kSuperPageSize);
  EXPECT_EQ(a6, base_ptr + 15 * kSuperPageSize);

  GetAddressPoolManager()->UnreserveAndDecommit(pool_, a5, 5 * kSuperPageSize);
  void* a7 =
      GetAddressPoolManager()->Reserve(pool_, nullptr, 7 * kSuperPageSize);
  EXPECT_EQ(a7, base_ptr + 6 * kSuperPageSize);
  void* a8 =
      GetAddressPoolManager()->Reserve(pool_, nullptr, 3 * kSuperPageSize);
  EXPECT_EQ(a8, base_ptr + 21 * kSuperPageSize);
  void* a9 =
      GetAddressPoolManager()->Reserve(pool_, nullptr, 2 * kSuperPageSize);
  EXPECT_EQ(a9, base_ptr + 13 * kSuperPageSize);

  GetAddressPoolManager()->UnreserveAndDecommit(pool_, a7, 7 * kSuperPageSize);
  GetAddressPoolManager()->UnreserveAndDecommit(pool_, a9, 2 * kSuperPageSize);
  GetAddressPoolManager()->UnreserveAndDecommit(pool_, a6, 6 * kSuperPageSize);
  void* a10 =
      GetAddressPoolManager()->Reserve(pool_, nullptr, 15 * kSuperPageSize);
  EXPECT_EQ(a10, base_ptr + 6 * kSuperPageSize);

  // Clean up.
  GetAddressPoolManager()->UnreserveAndDecommit(pool_, a1, kSuperPageSize);
  GetAddressPoolManager()->UnreserveAndDecommit(pool_, a2, 2 * kSuperPageSize);
  GetAddressPoolManager()->UnreserveAndDecommit(pool_, a3, 3 * kSuperPageSize);
  GetAddressPoolManager()->UnreserveAndDecommit(pool_, a8, 3 * kSuperPageSize);
  GetAddressPoolManager()->UnreserveAndDecommit(pool_, a10,
                                                15 * kSuperPageSize);
}

TEST_F(PartitionAllocAddressPoolManagerTest, DecommittedDataIsErased) {
  void* data = GetAddressPoolManager()->Reserve(pool_, nullptr, kSuperPageSize);
  ASSERT_TRUE(data);
  RecommitSystemPages(data, kSuperPageSize, PageReadWrite,
                      PageUpdatePermissions);

  memset(data, 42, kSuperPageSize);
  GetAddressPoolManager()->UnreserveAndDecommit(pool_, data, kSuperPageSize);

  void* data2 =
      GetAddressPoolManager()->Reserve(pool_, nullptr, kSuperPageSize);
  ASSERT_EQ(data, data2);
  RecommitSystemPages(data2, kSuperPageSize, PageReadWrite,
                      PageUpdatePermissions);

  uint32_t sum = 0;
  for (size_t i = 0; i < kSuperPageSize; i++) {
    sum += reinterpret_cast<uint8_t*>(data2)[i];
  }
  EXPECT_EQ(0u, sum) << sum / 42 << " bytes were not zeroed";

  GetAddressPoolManager()->UnreserveAndDecommit(pool_, data2, kSuperPageSize);
}

#else   // defined(PA_HAS_64_BITS_POINTERS)

TEST(PartitionAllocAddressPoolManagerTest, IsManagedByNonBRPPool) {
  constexpr size_t kAllocCount = 8;
  static const size_t kNumPages[kAllocCount] = {1, 4, 7, 8, 13, 16, 31, 60};
  void* addrs[kAllocCount];
  for (size_t i = 0; i < kAllocCount; ++i) {
    addrs[i] = AddressPoolManager::GetInstance()->Reserve(
        GetNonBRPPool(), nullptr,
        AddressPoolManagerBitmap::kBytesPer1BitOfNonBRPPoolBitmap *
            kNumPages[i]);
    EXPECT_TRUE(addrs[i]);
    EXPECT_TRUE(
        !(reinterpret_cast<uintptr_t>(addrs[i]) & kSuperPageOffsetMask));
    AddressPoolManager::GetInstance()->MarkUsed(
        GetNonBRPPool(), addrs[i],
        AddressPoolManagerBitmap::kBytesPer1BitOfNonBRPPoolBitmap *
            kNumPages[i]);
  }
  for (size_t i = 0; i < kAllocCount; ++i) {
    const char* ptr = reinterpret_cast<const char*>(addrs[i]);
    size_t num_pages =
        bits::AlignUp(
            kNumPages[i] *
                AddressPoolManagerBitmap::kBytesPer1BitOfNonBRPPoolBitmap,
            kSuperPageSize) /
        AddressPoolManagerBitmap::kBytesPer1BitOfNonBRPPoolBitmap;
    for (size_t j = 0; j < num_pages; ++j) {
      if (j < kNumPages[i]) {
        EXPECT_TRUE(AddressPoolManager::IsManagedByNonBRPPool(ptr));
      } else {
        EXPECT_FALSE(AddressPoolManager::IsManagedByNonBRPPool(ptr));
      }
      EXPECT_FALSE(AddressPoolManager::IsManagedByBRPPool(ptr));
      ptr += AddressPoolManagerBitmap::kBytesPer1BitOfNonBRPPoolBitmap;
    }
  }
  for (size_t i = 0; i < kAllocCount; ++i) {
    AddressPoolManager::GetInstance()->MarkUnused(
        GetNonBRPPool(), addrs[i],
        AddressPoolManagerBitmap::kBytesPer1BitOfNonBRPPoolBitmap *
            kNumPages[i]);
    AddressPoolManager::GetInstance()->UnreserveAndDecommit(
        GetNonBRPPool(), addrs[i],
        AddressPoolManagerBitmap::kBytesPer1BitOfNonBRPPoolBitmap *
            kNumPages[i]);
    EXPECT_FALSE(AddressPoolManager::IsManagedByNonBRPPool(addrs[i]));
    EXPECT_FALSE(AddressPoolManager::IsManagedByBRPPool(addrs[i]));
  }
}

TEST(PartitionAllocAddressPoolManagerTest, IsManagedByBRPPool) {
  constexpr size_t kAllocCount = 4;
  // Totally (1+3+7+11) * 2MB = 44MB allocation
  static const size_t kNumPages[kAllocCount] = {1, 3, 7, 11};
  void* addrs[kAllocCount];
  for (size_t i = 0; i < kAllocCount; ++i) {
    addrs[i] = AddressPoolManager::GetInstance()->Reserve(
        GetBRPPool(), nullptr, kSuperPageSize * kNumPages[i]);
    EXPECT_TRUE(addrs[i]);
    EXPECT_TRUE(
        !(reinterpret_cast<uintptr_t>(addrs[i]) & kSuperPageOffsetMask));
    AddressPoolManager::GetInstance()->MarkUsed(GetBRPPool(), addrs[i],
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
    const char* base_ptr = reinterpret_cast<const char*>(addrs[i]);
    const char* ptr = base_ptr;
    size_t num_allocated_size = kNumPages[i] * kSuperPageSize;
    size_t num_system_pages = num_allocated_size / SystemPageSize();
    for (size_t j = 0; j < num_system_pages; ++j) {
      size_t offset = ptr - base_ptr;
      if (offset < first_guard_size ||
          offset >= (num_allocated_size - last_guard_size)) {
        EXPECT_FALSE(AddressPoolManager::IsManagedByBRPPool(ptr));
      } else {
        EXPECT_TRUE(AddressPoolManager::IsManagedByBRPPool(ptr));
      }
      EXPECT_FALSE(AddressPoolManager::IsManagedByNonBRPPool(ptr));
      ptr += SystemPageSize();
    }
  }
  for (size_t i = 0; i < kAllocCount; ++i) {
    AddressPoolManager::GetInstance()->MarkUnused(
        GetBRPPool(), addrs[i], kSuperPageSize * kNumPages[i]);
    AddressPoolManager::GetInstance()->UnreserveAndDecommit(
        GetBRPPool(), addrs[i], kSuperPageSize * kNumPages[i]);
    EXPECT_FALSE(AddressPoolManager::IsManagedByNonBRPPool(addrs[i]));
    EXPECT_FALSE(AddressPoolManager::IsManagedByBRPPool(addrs[i]));
  }
}
#endif  // defined(PA_HAS_64_BITS_POINTERS)

}  // namespace internal
}  // namespace base
