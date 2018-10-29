// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <utility>

#include "base/memory/platform_shared_memory_region.h"
#include "base/memory/read_only_shared_memory_region.h"
#include "base/memory/shared_memory.h"
#include "base/memory/unsafe_shared_memory_region.h"
#include "base/memory/writable_shared_memory_region.h"
#include "base/sys_info.h"
#include "base/test/test_shared_memory_util.h"
#include "build/build_config.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace base {

const size_t kRegionSize = 1024;

bool IsMemoryFilledWithByte(const void* memory, size_t size, char byte) {
  const char* start_ptr = static_cast<const char*>(memory);
  const char* end_ptr = start_ptr + size;
  for (const char* ptr = start_ptr; ptr < end_ptr; ++ptr) {
    if (*ptr != byte)
      return false;
  }

  return true;
}

template <typename SharedMemoryRegionType>
class SharedMemoryRegionTest : public ::testing::Test {
 public:
  void SetUp() override {
    std::tie(region_, rw_mapping_) =
        CreateMappedRegion<SharedMemoryRegionType>(kRegionSize);
    ASSERT_TRUE(region_.IsValid());
    ASSERT_TRUE(rw_mapping_.IsValid());
    memset(rw_mapping_.memory(), 'G', kRegionSize);
    EXPECT_TRUE(IsMemoryFilledWithByte(rw_mapping_.memory(), kRegionSize, 'G'));
  }

 protected:
  SharedMemoryRegionType region_;
  WritableSharedMemoryMapping rw_mapping_;
};

typedef ::testing::Types<WritableSharedMemoryRegion,
                         UnsafeSharedMemoryRegion,
                         ReadOnlySharedMemoryRegion>
    AllRegionTypes;
TYPED_TEST_CASE(SharedMemoryRegionTest, AllRegionTypes);

TYPED_TEST(SharedMemoryRegionTest, NonValidRegion) {
  TypeParam region;
  EXPECT_FALSE(region.IsValid());
  // We shouldn't crash on Map but should return an invalid mapping.
  typename TypeParam::MappingType mapping = region.Map();
  EXPECT_FALSE(mapping.IsValid());
}

TYPED_TEST(SharedMemoryRegionTest, MoveRegion) {
  TypeParam moved_region = std::move(this->region_);
  EXPECT_FALSE(this->region_.IsValid());
  ASSERT_TRUE(moved_region.IsValid());

  // Check that moved region maps correctly.
  typename TypeParam::MappingType mapping = moved_region.Map();
  ASSERT_TRUE(mapping.IsValid());
  EXPECT_NE(this->rw_mapping_.memory(), mapping.memory());
  EXPECT_EQ(memcmp(this->rw_mapping_.memory(), mapping.memory(), kRegionSize),
            0);

  // Verify that the second mapping reflects changes in the first.
  memset(this->rw_mapping_.memory(), '#', kRegionSize);
  EXPECT_EQ(memcmp(this->rw_mapping_.memory(), mapping.memory(), kRegionSize),
            0);
}

TYPED_TEST(SharedMemoryRegionTest, MappingValidAfterClose) {
  // Check the mapping is still valid after the region is closed.
  this->region_ = TypeParam();
  EXPECT_FALSE(this->region_.IsValid());
  ASSERT_TRUE(this->rw_mapping_.IsValid());
  EXPECT_TRUE(
      IsMemoryFilledWithByte(this->rw_mapping_.memory(), kRegionSize, 'G'));
}

TYPED_TEST(SharedMemoryRegionTest, MapTwice) {
  // The second mapping is either writable or read-only.
  typename TypeParam::MappingType mapping = this->region_.Map();
  ASSERT_TRUE(mapping.IsValid());
  EXPECT_NE(this->rw_mapping_.memory(), mapping.memory());
  EXPECT_EQ(memcmp(this->rw_mapping_.memory(), mapping.memory(), kRegionSize),
            0);

  // Verify that the second mapping reflects changes in the first.
  memset(this->rw_mapping_.memory(), '#', kRegionSize);
  EXPECT_EQ(memcmp(this->rw_mapping_.memory(), mapping.memory(), kRegionSize),
            0);

  // Close the region and unmap the first memory segment, verify the second
  // still has the right data.
  this->region_ = TypeParam();
  this->rw_mapping_ = WritableSharedMemoryMapping();
  EXPECT_TRUE(IsMemoryFilledWithByte(mapping.memory(), kRegionSize, '#'));
}

TYPED_TEST(SharedMemoryRegionTest, MapUnmapMap) {
  this->rw_mapping_ = WritableSharedMemoryMapping();

  typename TypeParam::MappingType mapping = this->region_.Map();
  ASSERT_TRUE(mapping.IsValid());
  EXPECT_TRUE(IsMemoryFilledWithByte(mapping.memory(), kRegionSize, 'G'));
}

TYPED_TEST(SharedMemoryRegionTest, SerializeAndDeserialize) {
  subtle::PlatformSharedMemoryRegion platform_region =
      TypeParam::TakeHandleForSerialization(std::move(this->region_));
  EXPECT_EQ(platform_region.GetGUID(), this->rw_mapping_.guid());
  TypeParam region = TypeParam::Deserialize(std::move(platform_region));
  EXPECT_TRUE(region.IsValid());
  EXPECT_FALSE(this->region_.IsValid());
  typename TypeParam::MappingType mapping = region.Map();
  ASSERT_TRUE(mapping.IsValid());
  EXPECT_TRUE(IsMemoryFilledWithByte(mapping.memory(), kRegionSize, 'G'));

  // Verify that the second mapping reflects changes in the first.
  memset(this->rw_mapping_.memory(), '#', kRegionSize);
  EXPECT_EQ(memcmp(this->rw_mapping_.memory(), mapping.memory(), kRegionSize),
            0);
}

// Map() will return addresses which are aligned to the platform page size, this
// varies from platform to platform though.  Since we'd like to advertise a
// minimum alignment that callers can count on, test for it here.
TYPED_TEST(SharedMemoryRegionTest, MapMinimumAlignment) {
  EXPECT_EQ(0U,
            reinterpret_cast<uintptr_t>(this->rw_mapping_.memory()) &
                (subtle::PlatformSharedMemoryRegion::kMapMinimumAlignment - 1));
}

TYPED_TEST(SharedMemoryRegionTest, MapSize) {
  EXPECT_EQ(this->rw_mapping_.size(), kRegionSize);
  EXPECT_GE(this->rw_mapping_.mapped_size(), kRegionSize);
}

TYPED_TEST(SharedMemoryRegionTest, MapGranularity) {
  EXPECT_LT(this->rw_mapping_.mapped_size(),
            kRegionSize + SysInfo::VMAllocationGranularity());
}

TYPED_TEST(SharedMemoryRegionTest, MapAt) {
  const size_t kPageSize = SysInfo::VMAllocationGranularity();
  ASSERT_TRUE(kPageSize >= sizeof(uint32_t));
  ASSERT_EQ(kPageSize % sizeof(uint32_t), 0U);
  const size_t kDataSize = kPageSize * 2;
  const size_t kCount = kDataSize / sizeof(uint32_t);

  TypeParam region;
  WritableSharedMemoryMapping rw_mapping;
  std::tie(region, rw_mapping) = CreateMappedRegion<TypeParam>(kDataSize);
  ASSERT_TRUE(region.IsValid());
  ASSERT_TRUE(rw_mapping.IsValid());
  uint32_t* ptr = static_cast<uint32_t*>(rw_mapping.memory());

  for (size_t i = 0; i < kCount; ++i)
    ptr[i] = i;

  rw_mapping = WritableSharedMemoryMapping();
  off_t bytes_offset = kPageSize;
  typename TypeParam::MappingType mapping =
      region.MapAt(bytes_offset, kDataSize - bytes_offset);
  ASSERT_TRUE(mapping.IsValid());

  off_t int_offset = bytes_offset / sizeof(uint32_t);
  const uint32_t* ptr2 = static_cast<const uint32_t*>(mapping.memory());
  for (size_t i = int_offset; i < kCount; ++i) {
    EXPECT_EQ(ptr2[i - int_offset], i);
  }
}

TYPED_TEST(SharedMemoryRegionTest, MapAtNotAlignedOffsetFails) {
  const size_t kDataSize = SysInfo::VMAllocationGranularity();

  TypeParam region;
  WritableSharedMemoryMapping rw_mapping;
  std::tie(region, rw_mapping) = CreateMappedRegion<TypeParam>(kDataSize);
  ASSERT_TRUE(region.IsValid());
  ASSERT_TRUE(rw_mapping.IsValid());
  off_t offset = kDataSize / 2;
  typename TypeParam::MappingType mapping =
      region.MapAt(offset, kDataSize - offset);
  EXPECT_FALSE(mapping.IsValid());
}

TYPED_TEST(SharedMemoryRegionTest, MapZeroBytesFails) {
  typename TypeParam::MappingType mapping = this->region_.MapAt(0, 0);
  EXPECT_FALSE(mapping.IsValid());
}

TYPED_TEST(SharedMemoryRegionTest, MapMoreBytesThanRegionSizeFails) {
  size_t region_real_size = this->region_.GetSize();
  typename TypeParam::MappingType mapping =
      this->region_.MapAt(0, region_real_size + 1);
  EXPECT_FALSE(mapping.IsValid());
}

template <typename DuplicatableSharedMemoryRegion>
class DuplicatableSharedMemoryRegionTest
    : public SharedMemoryRegionTest<DuplicatableSharedMemoryRegion> {};

typedef ::testing::Types<UnsafeSharedMemoryRegion, ReadOnlySharedMemoryRegion>
    DuplicatableRegionTypes;
TYPED_TEST_CASE(DuplicatableSharedMemoryRegionTest, DuplicatableRegionTypes);

TYPED_TEST(DuplicatableSharedMemoryRegionTest, Duplicate) {
  TypeParam dup_region = this->region_.Duplicate();
  EXPECT_EQ(this->region_.GetGUID(), dup_region.GetGUID());
  typename TypeParam::MappingType mapping = dup_region.Map();
  ASSERT_TRUE(mapping.IsValid());
  EXPECT_NE(this->rw_mapping_.memory(), mapping.memory());
  EXPECT_EQ(this->rw_mapping_.guid(), mapping.guid());
  EXPECT_TRUE(IsMemoryFilledWithByte(mapping.memory(), kRegionSize, 'G'));
}

class ReadOnlySharedMemoryRegionTest : public ::testing::Test {
 public:
  ReadOnlySharedMemoryRegion GetInitiallyReadOnlyRegion(size_t size) {
    MappedReadOnlyRegion mapped_region =
        ReadOnlySharedMemoryRegion::Create(size);
    ReadOnlySharedMemoryRegion region = std::move(mapped_region.region);
    return region;
  }

  ReadOnlySharedMemoryRegion GetConvertedToReadOnlyRegion(size_t size) {
    WritableSharedMemoryRegion region =
        WritableSharedMemoryRegion::Create(kRegionSize);
    ReadOnlySharedMemoryRegion ro_region =
        WritableSharedMemoryRegion::ConvertToReadOnly(std::move(region));
    return ro_region;
  }
};

TEST_F(ReadOnlySharedMemoryRegionTest,
       InitiallyReadOnlyRegionCannotBeMappedAsWritable) {
  ReadOnlySharedMemoryRegion region = GetInitiallyReadOnlyRegion(kRegionSize);
  ASSERT_TRUE(region.IsValid());

  EXPECT_TRUE(CheckReadOnlyPlatformSharedMemoryRegionForTesting(
      ReadOnlySharedMemoryRegion::TakeHandleForSerialization(
          std::move(region))));
}

TEST_F(ReadOnlySharedMemoryRegionTest,
       ConvertedToReadOnlyRegionCannotBeMappedAsWritable) {
  ReadOnlySharedMemoryRegion region = GetConvertedToReadOnlyRegion(kRegionSize);
  ASSERT_TRUE(region.IsValid());

  EXPECT_TRUE(CheckReadOnlyPlatformSharedMemoryRegionForTesting(
      ReadOnlySharedMemoryRegion::TakeHandleForSerialization(
          std::move(region))));
}

TEST_F(ReadOnlySharedMemoryRegionTest,
       InitiallyReadOnlyRegionProducedMappingWriteDeathTest) {
  ReadOnlySharedMemoryRegion region = GetInitiallyReadOnlyRegion(kRegionSize);
  ASSERT_TRUE(region.IsValid());
  ReadOnlySharedMemoryMapping mapping = region.Map();
  ASSERT_TRUE(mapping.IsValid());
  void* memory_ptr = const_cast<void*>(mapping.memory());
  EXPECT_DEATH_IF_SUPPORTED(memset(memory_ptr, 'G', kRegionSize), "");
}

TEST_F(ReadOnlySharedMemoryRegionTest,
       ConvertedToReadOnlyRegionProducedMappingWriteDeathTest) {
  ReadOnlySharedMemoryRegion region = GetConvertedToReadOnlyRegion(kRegionSize);
  ASSERT_TRUE(region.IsValid());
  ReadOnlySharedMemoryMapping mapping = region.Map();
  ASSERT_TRUE(mapping.IsValid());
  void* memory_ptr = const_cast<void*>(mapping.memory());
  EXPECT_DEATH_IF_SUPPORTED(memset(memory_ptr, 'G', kRegionSize), "");
}

class UnsafeSharedMemoryRegionTest : public ::testing::Test {};

TEST_F(UnsafeSharedMemoryRegionTest, CreateFromHandleTest) {
  SharedMemory shm;

  auto region = UnsafeSharedMemoryRegion::CreateFromHandle(shm.TakeHandle());
  ASSERT_FALSE(region.IsValid());

  shm.CreateAndMapAnonymous(10);
  region = UnsafeSharedMemoryRegion::CreateFromHandle(shm.TakeHandle());
  ASSERT_TRUE(region.IsValid());
}

}  // namespace base
