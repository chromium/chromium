// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/memory/unsafe_shared_memory_pool.h"

#include "testing/gtest/include/gtest/gtest.h"

namespace base {

TEST(UnsafeSharedMemoryPoolTest, CreatesRegion) {
  scoped_refptr<UnsafeSharedMemoryPool> pool(
      base::MakeRefCounted<UnsafeSharedMemoryPool>());
  auto handle = pool->MaybeAllocateBuffer(1000);
  ASSERT_TRUE(handle);
  EXPECT_TRUE(handle->GetRegion().IsValid());
  EXPECT_TRUE(handle->GetMapping().IsValid());
}

TEST(UnsafeSharedMemoryPoolTest, ReusesRegions) {
  scoped_refptr<UnsafeSharedMemoryPool> pool(
      base::MakeRefCounted<UnsafeSharedMemoryPool>());
  auto handle = pool->MaybeAllocateBuffer(1000u);
  ASSERT_TRUE(handle);
  auto id1 = handle->GetRegion().GetGUID();

  // Return memory to the pool.
  handle.reset();

  handle = pool->MaybeAllocateBuffer(1000u);
  // Should reuse the freed region.
  EXPECT_EQ(id1, handle->GetRegion().GetGUID());
}

TEST(UnsafeSharedMemoryPoolTest, RespectsSize) {
  scoped_refptr<UnsafeSharedMemoryPool> pool(
      base::MakeRefCounted<UnsafeSharedMemoryPool>());
  auto handle = pool->MaybeAllocateBuffer(1000u);
  ASSERT_TRUE(handle);
  EXPECT_GE(handle->GetRegion().GetSize(), 1000u);

  handle = pool->MaybeAllocateBuffer(100u);
  ASSERT_TRUE(handle);
  EXPECT_GE(handle->GetRegion().GetSize(), 100u);

  handle = pool->MaybeAllocateBuffer(1100u);
  ASSERT_TRUE(handle);
  EXPECT_GE(handle->GetRegion().GetSize(), 1100u);
}
}  // namespace base
