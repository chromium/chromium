// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/memory/shared_memory_hooks.h"

#include <optional>

#include "testing/gtest/include/gtest/gtest.h"

namespace base {

class SharedMemoryHooksTest : public ::testing::Test {
 protected:
  void TearDown() override { SetCreateHooks(nullptr, nullptr, nullptr); }

  void SetCreateHooks(
      ReadOnlySharedMemoryRegion::CreateFunction* read_only_hook,
      UnsafeSharedMemoryRegion::CreateFunction* unsafe_hook,
      WritableSharedMemoryRegion::CreateFunction* writable_hook) {
    SharedMemoryHooks::SetCreateHooks(read_only_hook, unsafe_hook,
                                      writable_hook);
  }
};

std::optional<size_t> requested_read_only_shmem_size;
std::optional<size_t> requested_unsafe_shmem_size;
std::optional<size_t> requested_writable_shmem_size;

MappedReadOnlyRegion ReadOnlyShmemCreateHook(size_t size, SharedMemoryMapper* mapper) {
  requested_read_only_shmem_size = size;
  return {};
}

UnsafeSharedMemoryRegion UnsafeShmemCreateHook(size_t size) {
  requested_unsafe_shmem_size = size;
  return {};
}

WritableSharedMemoryRegion WritableShmemCreateHook(size_t size) {
  requested_writable_shmem_size = size;
  return {};
}

TEST_F(SharedMemoryHooksTest, Basic) {
  {
    auto region = ReadOnlySharedMemoryRegion::Create(3);
    EXPECT_TRUE(region.IsValid());
    EXPECT_FALSE(requested_read_only_shmem_size.has_value());
  }

  {
    auto region = UnsafeSharedMemoryRegion::Create(25);
    EXPECT_TRUE(region.IsValid());
    EXPECT_FALSE(requested_unsafe_shmem_size.has_value());
  }

  {
    auto region = WritableSharedMemoryRegion::Create(777);
    EXPECT_TRUE(region.IsValid());
    EXPECT_FALSE(requested_writable_shmem_size.has_value());
  }

  SetCreateHooks(&ReadOnlyShmemCreateHook, &UnsafeShmemCreateHook,
                 &WritableShmemCreateHook);

  {
    auto region = ReadOnlySharedMemoryRegion::Create(3);
    EXPECT_FALSE(region.IsValid());
    EXPECT_EQ(3u, *requested_read_only_shmem_size);
  }

  {
    auto region = UnsafeSharedMemoryRegion::Create(25);
    EXPECT_FALSE(region.IsValid());
    EXPECT_EQ(25u, *requested_unsafe_shmem_size);
  }

  {
    auto region = WritableSharedMemoryRegion::Create(777);
    EXPECT_FALSE(region.IsValid());
    EXPECT_EQ(777u, *requested_writable_shmem_size);
  }
}

}  // namespace base
