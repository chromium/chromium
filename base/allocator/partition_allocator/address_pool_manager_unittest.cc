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

#if defined(PA_HAS_64_BITS_POINTERS)

class AddressPoolManagerTest : public testing::Test {
 protected:
  AddressPoolManagerTest() = default;
  ~AddressPoolManagerTest() override = default;

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
};

TEST_F(AddressPoolManagerTest, TooLargePool) {
  uintptr_t base_addr = 0x4200000;

  constexpr size_t kSize = 16ull * 1024 * 1024 * 1024;
  EXPECT_DEATH_IF_SUPPORTED(
      AddressPoolManager::GetInstance()->Add(base_addr, kSize + kSuperPageSize),
      "");
}

TEST_F(AddressPoolManagerTest, ManyPages) {
  char* base_ptr = reinterpret_cast<char*>(base_address_);

  EXPECT_EQ(AddressPoolManager::GetInstance()->Alloc(pool_,
                                                     kPageCnt * kSuperPageSize),
            base_ptr);
  EXPECT_EQ(AddressPoolManager::GetInstance()->Alloc(pool_, kSuperPageSize),
            nullptr);
  AddressPoolManager::GetInstance()->Free(pool_, base_ptr,
                                          kPageCnt * kSuperPageSize);
  EXPECT_EQ(AddressPoolManager::GetInstance()->Alloc(pool_,
                                                     kPageCnt * kSuperPageSize),
            base_ptr);
}

TEST_F(AddressPoolManagerTest, PagesFragmented) {
  char* base_ptr = reinterpret_cast<char*>(base_address_);
  void* addrs[kPageCnt];
  for (size_t i = 0; i < kPageCnt; ++i) {
    addrs[i] = AddressPoolManager::GetInstance()->Alloc(pool_, kSuperPageSize);
    EXPECT_EQ(addrs[i], base_ptr + i * kSuperPageSize);
  }
  EXPECT_EQ(AddressPoolManager::GetInstance()->Alloc(pool_, kSuperPageSize),
            nullptr);
  for (size_t i = 1; i < kPageCnt; i += 2) {
    AddressPoolManager::GetInstance()->Free(pool_, addrs[i], kSuperPageSize);
  }
  EXPECT_EQ(AddressPoolManager::GetInstance()->Alloc(pool_, 2 * kSuperPageSize),
            nullptr);
  for (size_t i = 1; i < kPageCnt; i += 2) {
    addrs[i] = AddressPoolManager::GetInstance()->Alloc(pool_, kSuperPageSize);
    EXPECT_EQ(addrs[i], base_ptr + i * kSuperPageSize);
  }
  EXPECT_EQ(AddressPoolManager::GetInstance()->Alloc(pool_, kSuperPageSize),
            nullptr);
}

TEST_F(AddressPoolManagerTest, IrregularPattern) {
  char* base_ptr = reinterpret_cast<char*>(base_address_);

  void* a1 = AddressPoolManager::GetInstance()->Alloc(pool_, kSuperPageSize);
  EXPECT_EQ(a1, base_ptr);
  void* a2 =
      AddressPoolManager::GetInstance()->Alloc(pool_, 2 * kSuperPageSize);
  EXPECT_EQ(a2, base_ptr + 1 * kSuperPageSize);
  void* a3 =
      AddressPoolManager::GetInstance()->Alloc(pool_, 3 * kSuperPageSize);
  EXPECT_EQ(a3, base_ptr + 3 * kSuperPageSize);
  void* a4 =
      AddressPoolManager::GetInstance()->Alloc(pool_, 4 * kSuperPageSize);
  EXPECT_EQ(a4, base_ptr + 6 * kSuperPageSize);
  void* a5 =
      AddressPoolManager::GetInstance()->Alloc(pool_, 5 * kSuperPageSize);
  EXPECT_EQ(a5, base_ptr + 10 * kSuperPageSize);

  AddressPoolManager::GetInstance()->Free(pool_, a4, 4 * kSuperPageSize);
  void* a6 =
      AddressPoolManager::GetInstance()->Alloc(pool_, 6 * kSuperPageSize);
  EXPECT_EQ(a6, base_ptr + 15 * kSuperPageSize);

  AddressPoolManager::GetInstance()->Free(pool_, a5, 5 * kSuperPageSize);
  void* a7 =
      AddressPoolManager::GetInstance()->Alloc(pool_, 7 * kSuperPageSize);
  EXPECT_EQ(a7, base_ptr + 6 * kSuperPageSize);
  void* a8 =
      AddressPoolManager::GetInstance()->Alloc(pool_, 3 * kSuperPageSize);
  EXPECT_EQ(a8, base_ptr + 21 * kSuperPageSize);
  void* a9 =
      AddressPoolManager::GetInstance()->Alloc(pool_, 2 * kSuperPageSize);
  EXPECT_EQ(a9, base_ptr + 13 * kSuperPageSize);

  AddressPoolManager::GetInstance()->Free(pool_, a7, 7 * kSuperPageSize);
  AddressPoolManager::GetInstance()->Free(pool_, a9, 2 * kSuperPageSize);
  AddressPoolManager::GetInstance()->Free(pool_, a6, 6 * kSuperPageSize);
  void* a10 =
      AddressPoolManager::GetInstance()->Alloc(pool_, 15 * kSuperPageSize);
  EXPECT_EQ(a10, base_ptr + 6 * kSuperPageSize);
}

TEST_F(AddressPoolManagerTest, DecommittedDataIsErased) {
  void* data = AddressPoolManager::GetInstance()->Alloc(pool_, kSuperPageSize);
  ASSERT_TRUE(data);

  memset(data, 42, kSuperPageSize);
  AddressPoolManager::GetInstance()->Free(pool_, data, kSuperPageSize);

  void* data2 = AddressPoolManager::GetInstance()->Alloc(pool_, kSuperPageSize);
  ASSERT_EQ(data, data2);

  uint32_t sum = 0;
  for (size_t i = 0; i < kSuperPageSize; i++) {
    sum += reinterpret_cast<uint8_t*>(data2)[i];
  }

  EXPECT_EQ(0u, sum) << sum / 42 << " bytes were not zeroed";
}
#endif  // defined(PA_HAS_64_BITS_POINTERS)

}  // namespace internal
}  // namespace base
