// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/memory/protected_memory.h"
#include "base/synchronization/lock.h"
#include "base/test/gtest_util.h"
#include "base/thread_annotations.h"
#include "build/build_config.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace base {

namespace {

struct Data {
  Data() = default;
  explicit Data(int foo_) : foo(foo_) {}
  int foo;
};

}  // namespace

PROTECTED_MEMORY_SECTION ProtectedMemory<int> g_init;

TEST(ProtectedMemoryTest, Initializer) {
  static ProtectedMemory<int>::Initializer I(&g_init, 4);
  EXPECT_EQ(*g_init, 4);
}

PROTECTED_MEMORY_SECTION ProtectedMemory<Data> g_data;

TEST(ProtectedMemoryTest, Basic) {
  AutoWritableMemory writer(g_data);
  writer.GetProtectedDataPtr()->foo = 5;
  EXPECT_EQ(g_data->foo, 5);
}

#if BUILDFLAG(PROTECTED_MEMORY_ENABLED)
TEST(ProtectedMemoryTest, AssertMemoryIsReadOnly) {
  internal::AssertMemoryIsReadOnly(&g_data->foo);
  { AutoWritableMemory writer(g_data); }
  internal::AssertMemoryIsReadOnly(&g_data->foo);

  ProtectedMemory<Data> writable_data;
  EXPECT_CHECK_DEATH(
      { internal::AssertMemoryIsReadOnly(&writable_data->foo); });
}

TEST(ProtectedMemoryTest, FailsIfDefinedOutsideOfProtectMemoryRegion) {
  ProtectedMemory<Data> data;
  EXPECT_CHECK_DEATH({ AutoWritableMemory writer(data); });
}
#endif  // BUILDFLAG(PROTECTED_MEMORY_ENABLED)

}  // namespace base
