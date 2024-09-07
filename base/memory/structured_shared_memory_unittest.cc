// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/memory/structured_shared_memory.h"

#include <atomic>
#include <optional>
#include <utility>

#include "base/containers/span.h"
#include "base/memory/platform_shared_memory_handle.h"
#include "base/memory/read_only_shared_memory_region.h"
#include "base/memory/shared_memory_mapper.h"
#include "base/test/gtest_util.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace base {

namespace {

// A SharedMemoryMapper that always fails to map memory.
class FailingSharedMemoryMapper final : public SharedMemoryMapper {
 public:
  std::optional<span<uint8_t>> Map(subtle::PlatformSharedMemoryHandle handle,
                                   bool write_allowed,
                                   uint64_t offset,
                                   size_t size) final {
    return std::nullopt;
  }

  void Unmap(span<uint8_t> mapping) final {}
};

TEST(StructuredSharedMemoryTest, ReadWrite) {
  auto writable_memory = StructuredSharedMemory<double>::Create();
  ASSERT_TRUE(writable_memory.has_value());
  EXPECT_EQ(writable_memory->WritablePtr(), &writable_memory->WritableRef());
  EXPECT_EQ(writable_memory->WritablePtr(), writable_memory->ReadOnlyPtr());
  EXPECT_EQ(writable_memory->ReadOnlyPtr(), &writable_memory->ReadOnlyRef());
  EXPECT_EQ(writable_memory->ReadOnlyRef(), 0.0);

  auto read_only_memory = StructuredSharedMemory<double>::MapReadOnlyRegion(
      writable_memory->TakeReadOnlyRegion());
  ASSERT_TRUE(read_only_memory.has_value());
  EXPECT_EQ(read_only_memory->ReadOnlyPtr(), &read_only_memory->ReadOnlyRef());
  EXPECT_EQ(read_only_memory->ReadOnlyRef(), 0.0);

  writable_memory->WritableRef() += 0.5;
  EXPECT_EQ(read_only_memory->ReadOnlyRef(), 0.5);
}

TEST(StructuredSharedMemoryTest, Initialize) {
  // Default initialize.
  auto writable_memory = StructuredSharedMemory<double>::Create();
  ASSERT_TRUE(writable_memory.has_value());
  EXPECT_EQ(writable_memory->ReadOnlyRef(), 0.0);

  // Initialize from same type.
  writable_memory = StructuredSharedMemory<double>::Create(1.2);
  ASSERT_TRUE(writable_memory.has_value());
  EXPECT_EQ(writable_memory->ReadOnlyRef(), 1.2);

  // Initialize from compatible type.
  writable_memory = StructuredSharedMemory<double>::Create(3);
  ASSERT_TRUE(writable_memory.has_value());
  EXPECT_EQ(writable_memory->ReadOnlyRef(), 3);
}

TEST(StructuredSharedMemoryTest, MapFailure) {
  // Fail to map writable memory.
  FailingSharedMemoryMapper failing_mapper;
  auto writable_memory =
      StructuredSharedMemory<double>::CreateWithCustomMapper(&failing_mapper);
  EXPECT_FALSE(writable_memory.has_value());

  // Initialize from same type, but fail.
  writable_memory = StructuredSharedMemory<double>::CreateWithCustomMapper(
      1.2, &failing_mapper);
  EXPECT_FALSE(writable_memory.has_value());

  // Initialize from compatible type, but fail.
  writable_memory = StructuredSharedMemory<double>::CreateWithCustomMapper(
      3, &failing_mapper);
  EXPECT_FALSE(writable_memory.has_value());

  // Fail to create read-only region (bad handle).
  ReadOnlySharedMemoryRegion region;
  EXPECT_FALSE(region.IsValid());
  auto read_only_memory =
      StructuredSharedMemory<double>::MapReadOnlyRegion(std::move(region));
  EXPECT_FALSE(read_only_memory.has_value());

  // Valid handle for read-only region, but fail to map memory.
  writable_memory = StructuredSharedMemory<double>::Create();
  ASSERT_TRUE(writable_memory.has_value());
  region = writable_memory->TakeReadOnlyRegion();
  EXPECT_TRUE(region.IsValid());
  read_only_memory = StructuredSharedMemory<double>::MapReadOnlyRegion(
      std::move(region), &failing_mapper);
  EXPECT_FALSE(read_only_memory.has_value());
}

TEST(StructuredSharedMemoryDeathTest, DuplicateRegion) {
  auto writable_memory = StructuredSharedMemory<double>::Create();
  ASSERT_TRUE(writable_memory.has_value());
  ReadOnlySharedMemoryRegion region =
      writable_memory->DuplicateReadOnlyRegion();
  ReadOnlySharedMemoryRegion region2 =
      writable_memory->DuplicateReadOnlyRegion();
  EXPECT_EQ(region.GetGUID(), region2.GetGUID());
  ReadOnlySharedMemoryRegion region3 = writable_memory->TakeReadOnlyRegion();
  EXPECT_EQ(region.GetGUID(), region3.GetGUID());
  // Region is no longer valid.
  EXPECT_CHECK_DEATH(writable_memory->DuplicateReadOnlyRegion());
  EXPECT_CHECK_DEATH(writable_memory->TakeReadOnlyRegion());
}

TEST(StructuredSharedMemoryTest, AtomicReadWrite) {
  auto writable_memory = AtomicSharedMemory<int>::Create();
  ASSERT_TRUE(writable_memory.has_value());
  EXPECT_EQ(writable_memory->WritablePtr(), &writable_memory->WritableRef());
  EXPECT_EQ(writable_memory->WritablePtr(), writable_memory->ReadOnlyPtr());
  EXPECT_EQ(writable_memory->ReadOnlyPtr(), &writable_memory->ReadOnlyRef());
  EXPECT_EQ(writable_memory->ReadOnlyRef().load(std::memory_order_relaxed), 0);

  auto read_only_memory = AtomicSharedMemory<int>::MapReadOnlyRegion(
      writable_memory->TakeReadOnlyRegion());
  ASSERT_TRUE(read_only_memory.has_value());
  EXPECT_EQ(read_only_memory->ReadOnlyPtr(), &read_only_memory->ReadOnlyRef());
  EXPECT_EQ(read_only_memory->ReadOnlyRef().load(std::memory_order_relaxed), 0);

  writable_memory->WritableRef().store(1, std::memory_order_relaxed);
  EXPECT_EQ(read_only_memory->ReadOnlyRef().load(std::memory_order_relaxed), 1);
}

TEST(StructuredSharedMemoryTest, AtomicInitialize) {
  auto writable_memory = AtomicSharedMemory<int>::Create();
  ASSERT_TRUE(writable_memory.has_value());
  EXPECT_EQ(writable_memory->ReadOnlyRef().load(std::memory_order_relaxed), 0);

  writable_memory = AtomicSharedMemory<int>::Create(1);
  ASSERT_TRUE(writable_memory.has_value());
  EXPECT_EQ(writable_memory->ReadOnlyRef().load(std::memory_order_relaxed), 1);
}

}  // namespace

}  // namespace base
