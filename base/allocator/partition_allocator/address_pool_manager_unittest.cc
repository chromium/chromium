// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/allocator/partition_allocator/address_pool_manager.h"

#include "base/allocator/partition_allocator/page_allocator.h"
#include "base/allocator/partition_allocator/page_allocator_internal.h"
#include "base/allocator/partition_allocator/partition_alloc_constants.h"
#include "build/build_config.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace base {
namespace internal {

class AddressPoolManagerTest : public testing::Test {
 protected:
  AddressPoolManagerTest() = default;
  ~AddressPoolManagerTest() override = default;

#if defined(PA_HAS_64_BITS_POINTERS)
  void SetUp() override {
    AddressPoolManager::GetInstance()->ResetForTesting();
    base_address_ =
        AllocPages(nullptr, kPoolSize, kSuperPageSize, base::PageInaccessible,
                   PageTag::kPartitionAlloc, false);
    ASSERT_TRUE(base_address_);
    pool_ = AddressPoolManager::GetInstance()->Add(
        reinterpret_cast<uintptr_t>(base_address_), kPoolSize);
  }

  void TearDown() override { FreePages(base_address_, kPoolSize); }

  static constexpr size_t kPageCnt = 8192;
  static constexpr size_t kPoolSize = kSuperPageSize * kPageCnt;

  void* base_address_;
  pool_handle pool_;
#endif
};

#if defined(PA_HAS_64_BITS_POINTERS)
TEST_F(AddressPoolManagerTest, TooLargePool) {
  uintptr_t base_addr = 0x4200000;

  constexpr size_t kSize = 16ull * 1024 * 1024 * 1024;
  EXPECT_DEATH_IF_SUPPORTED(
      AddressPoolManager::GetInstance()->Add(base_addr, kSize + kSuperPageSize),
      "");
}

TEST_F(AddressPoolManagerTest, ManyPages) {
  char* base_ptr = reinterpret_cast<char*>(base_address_);

  EXPECT_EQ(AddressPoolManager::GetInstance()->Alloc(pool_, nullptr,
                                                     kPageCnt * kSuperPageSize),
            base_ptr);
  EXPECT_EQ(
      AddressPoolManager::GetInstance()->Alloc(pool_, nullptr, kSuperPageSize),
      nullptr);
  AddressPoolManager::GetInstance()->Free(pool_, base_ptr,
                                          kPageCnt * kSuperPageSize);
  EXPECT_EQ(AddressPoolManager::GetInstance()->Alloc(pool_, nullptr,
                                                     kPageCnt * kSuperPageSize),
            base_ptr);
}

TEST_F(AddressPoolManagerTest, PagesFragmented) {
  char* base_ptr = reinterpret_cast<char*>(base_address_);
  void* addrs[kPageCnt];
  for (size_t i = 0; i < kPageCnt; ++i) {
    addrs[i] = AddressPoolManager::GetInstance()->Alloc(pool_, nullptr,
                                                        kSuperPageSize);
    EXPECT_EQ(addrs[i], base_ptr + i * kSuperPageSize);
  }
  EXPECT_EQ(
      AddressPoolManager::GetInstance()->Alloc(pool_, nullptr, kSuperPageSize),
      nullptr);
  for (size_t i = 1; i < kPageCnt; i += 2) {
    AddressPoolManager::GetInstance()->Free(pool_, addrs[i], kSuperPageSize);
  }
  EXPECT_EQ(AddressPoolManager::GetInstance()->Alloc(pool_, nullptr,
                                                     2 * kSuperPageSize),
            nullptr);
  for (size_t i = 1; i < kPageCnt; i += 2) {
    addrs[i] = AddressPoolManager::GetInstance()->Alloc(pool_, nullptr,
                                                        kSuperPageSize);
    EXPECT_EQ(addrs[i], base_ptr + i * kSuperPageSize);
  }
  EXPECT_EQ(
      AddressPoolManager::GetInstance()->Alloc(pool_, nullptr, kSuperPageSize),
      nullptr);
}

TEST_F(AddressPoolManagerTest, IrregularPattern) {
  char* base_ptr = reinterpret_cast<char*>(base_address_);

  void* a1 =
      AddressPoolManager::GetInstance()->Alloc(pool_, nullptr, kSuperPageSize);
  EXPECT_EQ(a1, base_ptr);
  void* a2 = AddressPoolManager::GetInstance()->Alloc(pool_, nullptr,
                                                      2 * kSuperPageSize);
  EXPECT_EQ(a2, base_ptr + 1 * kSuperPageSize);
  void* a3 = AddressPoolManager::GetInstance()->Alloc(pool_, nullptr,
                                                      3 * kSuperPageSize);
  EXPECT_EQ(a3, base_ptr + 3 * kSuperPageSize);
  void* a4 = AddressPoolManager::GetInstance()->Alloc(pool_, nullptr,
                                                      4 * kSuperPageSize);
  EXPECT_EQ(a4, base_ptr + 6 * kSuperPageSize);
  void* a5 = AddressPoolManager::GetInstance()->Alloc(pool_, nullptr,
                                                      5 * kSuperPageSize);
  EXPECT_EQ(a5, base_ptr + 10 * kSuperPageSize);

  AddressPoolManager::GetInstance()->Free(pool_, a4, 4 * kSuperPageSize);
  void* a6 = AddressPoolManager::GetInstance()->Alloc(pool_, nullptr,
                                                      6 * kSuperPageSize);
  EXPECT_EQ(a6, base_ptr + 15 * kSuperPageSize);

  AddressPoolManager::GetInstance()->Free(pool_, a5, 5 * kSuperPageSize);
  void* a7 = AddressPoolManager::GetInstance()->Alloc(pool_, nullptr,
                                                      7 * kSuperPageSize);
  EXPECT_EQ(a7, base_ptr + 6 * kSuperPageSize);
  void* a8 = AddressPoolManager::GetInstance()->Alloc(pool_, nullptr,
                                                      3 * kSuperPageSize);
  EXPECT_EQ(a8, base_ptr + 21 * kSuperPageSize);
  void* a9 = AddressPoolManager::GetInstance()->Alloc(pool_, nullptr,
                                                      2 * kSuperPageSize);
  EXPECT_EQ(a9, base_ptr + 13 * kSuperPageSize);

  AddressPoolManager::GetInstance()->Free(pool_, a7, 7 * kSuperPageSize);
  AddressPoolManager::GetInstance()->Free(pool_, a9, 2 * kSuperPageSize);
  AddressPoolManager::GetInstance()->Free(pool_, a6, 6 * kSuperPageSize);
  void* a10 = AddressPoolManager::GetInstance()->Alloc(pool_, nullptr,
                                                       15 * kSuperPageSize);
  EXPECT_EQ(a10, base_ptr + 6 * kSuperPageSize);
}

TEST_F(AddressPoolManagerTest, DecommittedDataIsErased) {
  void* data =
      AddressPoolManager::GetInstance()->Alloc(pool_, nullptr, kSuperPageSize);
  ASSERT_TRUE(data);

  memset(data, 42, kSuperPageSize);
  AddressPoolManager::GetInstance()->Free(pool_, data, kSuperPageSize);

  void* data2 =
      AddressPoolManager::GetInstance()->Alloc(pool_, nullptr, kSuperPageSize);
  ASSERT_EQ(data, data2);

  uint32_t sum = 0;
  for (size_t i = 0; i < kSuperPageSize; i++) {
    sum += reinterpret_cast<uint8_t*>(data2)[i];
  }

  EXPECT_EQ(0u, sum) << sum / 42 << " bytes were not zeroed";
}

#else   // defined(PA_HAS_64_BITS_POINTERS)

TEST_F(AddressPoolManagerTest, IsManagedByDirectMapPool) {
  constexpr size_t kAllocCount = 8;
  static const size_t kNumPages[kAllocCount] = {1, 4, 7, 8, 13, 16, 31, 60};
  void* addrs[kAllocCount];
  for (size_t i = 0; i < kAllocCount; ++i) {
    addrs[i] = AddressPoolManager::GetInstance()->Alloc(
        GetDirectMapPool(), nullptr,
        PageAllocationGranularity() * kNumPages[i]);
    EXPECT_TRUE(addrs[i]);
    EXPECT_TRUE(
        !(reinterpret_cast<uintptr_t>(addrs[i]) & kSuperPageOffsetMask));
  }
  for (size_t i = 0; i < kAllocCount; ++i) {
    const char* ptr = reinterpret_cast<const char*>(addrs[i]);
    size_t num_pages = bits::Align(kNumPages[i] * PageAllocationGranularity(),
                                   kSuperPageSize) /
                       PageAllocationGranularity();
    for (size_t j = 0; j < num_pages; ++j) {
      if (j < kNumPages[i]) {
        EXPECT_TRUE(AddressPoolManager::IsManagedByDirectMapPool(ptr));
      } else {
        EXPECT_FALSE(AddressPoolManager::IsManagedByDirectMapPool(ptr));
      }
      EXPECT_FALSE(AddressPoolManager::IsManagedByNormalBucketPool(ptr));
      ptr += PageAllocationGranularity();
    }
  }
  for (size_t i = 0; i < kAllocCount; ++i) {
    AddressPoolManager::GetInstance()->Free(
        GetDirectMapPool(), addrs[i],
        PageAllocationGranularity() * kNumPages[i]);
    EXPECT_FALSE(AddressPoolManager::IsManagedByDirectMapPool(addrs[i]));
    EXPECT_FALSE(AddressPoolManager::IsManagedByNormalBucketPool(addrs[i]));
  }
}

TEST_F(AddressPoolManagerTest, IsManagedByNormalBucketPool) {
  constexpr size_t kAllocCount = 4;
  // Totally (1+3+7+11) * 2MB = 44MB allocation
  static const size_t kNumPages[kAllocCount] = {1, 3, 7, 11};
  void* addrs[kAllocCount];
  for (size_t i = 0; i < kAllocCount; ++i) {
    addrs[i] = AddressPoolManager::GetInstance()->Alloc(
        GetNormalBucketPool(), nullptr, kSuperPageSize * kNumPages[i]);
    EXPECT_TRUE(addrs[i]);
    EXPECT_TRUE(
        !(reinterpret_cast<uintptr_t>(addrs[i]) & kSuperPageOffsetMask));
  }
  for (size_t i = 0; i < kAllocCount; ++i) {
    const char* ptr = reinterpret_cast<const char*>(addrs[i]);
    size_t num_system_pages = kNumPages[i] * kSuperPageSize / SystemPageSize();
    for (size_t j = 0; j < num_system_pages; ++j) {
      EXPECT_TRUE(AddressPoolManager::IsManagedByNormalBucketPool(ptr));
      EXPECT_FALSE(AddressPoolManager::IsManagedByDirectMapPool(ptr));
      ptr += SystemPageSize();
    }
  }
  for (size_t i = 0; i < kAllocCount; ++i) {
    AddressPoolManager::GetInstance()->Free(GetNormalBucketPool(), addrs[i],
                                            kSuperPageSize * kNumPages[i]);
    EXPECT_FALSE(AddressPoolManager::IsManagedByDirectMapPool(addrs[i]));
    EXPECT_FALSE(AddressPoolManager::IsManagedByNormalBucketPool(addrs[i]));
  }
}
#endif  // defined(PA_HAS_64_BITS_POINTERS)

}  // namespace internal
}  // namespace base
