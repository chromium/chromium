// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/memory/protected_memory.h"

#include <stddef.h>
#include <stdint.h>

#include <climits>

#include "base/memory/protected_memory_buildflags.h"
#include "base/synchronization/lock.h"
#include "base/test/gtest_util.h"
#include "base/thread_annotations.h"
#include "build/build_config.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace base {

namespace {

struct Data {
  Data() = default;
  int foo = 0;
};

#if BUILDFLAG(PROTECTED_MEMORY_ENABLED)
void VerifyByteSequenceIsNotWriteable(unsigned char* const byte_pattern,
                                      const size_t number_of_bits,
                                      const size_t bit_increment) {
  const auto check_bit_not_writeable = [=](const size_t bit_index) {
    const size_t byte_index = bit_index / CHAR_BIT;
    const size_t local_bit_index = bit_index % CHAR_BIT;

    EXPECT_CHECK_DEATH_WITH(
        byte_pattern[byte_index] ^= (0x1 << local_bit_index), "")
        << " at bit " << bit_index << " of " << number_of_bits;
  };

  // Check the boundary bits explicitly to ensure we cover these.
  if (number_of_bits >= 1) {
    check_bit_not_writeable(0);
  }

  if (number_of_bits >= 2) {
    check_bit_not_writeable(number_of_bits - 1);
  }

  // Now check the bits in between at the requested increment.
  for (size_t bit_index = bit_increment; bit_index < (number_of_bits - 1);
       bit_index += bit_increment) {
    check_bit_not_writeable(bit_index);
  }
}

template <typename T>
void VerifyInstanceIsNotWriteable(T& instance, const size_t bit_increment = 3) {
  VerifyByteSequenceIsNotWriteable(
      reinterpret_cast<unsigned char*>(std::addressof(instance)),
      sizeof(T) * CHAR_BIT, bit_increment);
}
#endif  // BUILDFLAG(PROTECTED_MEMORY_ENABLED)

PROTECTED_MEMORY_SECTION ProtectedMemory<int> g_int_data;
PROTECTED_MEMORY_SECTION ProtectedMemory<Data> g_structured_data;

}  // namespace

TEST(ProtectedMemoryTest, Initializer) {
  ProtectedMemory<int>::Initializer initializer(&g_int_data, 4);
  EXPECT_EQ(*g_int_data, 4);
}

TEST(ProtectedMemoryTest, Basic) {
  AutoWritableMemory writer(g_structured_data);
  writer.GetProtectedDataPtr()->foo = 5;
  EXPECT_EQ(g_structured_data->foo, 5);
}

PROTECTED_MEMORY_SECTION ProtectedMemory<double> g_double_data;

// Verify that we can successfully create AutoWritableMemory for independent
// data.
TEST(ProtectedMemoryTest, VerifySimultaneousLocksOnDifferentData) {
  AutoWritableMemory writer_for_structured_data(g_structured_data);
  AutoWritableMemory writer_for_int_data(g_int_data);
  AutoWritableMemory writer_for_double_data(g_double_data);

  writer_for_structured_data.GetProtectedData().foo += 1;
  writer_for_int_data.GetProtectedData() += 1;
  writer_for_double_data.GetProtectedData() += 1;
}

#if BUILDFLAG(PROTECTED_MEMORY_ENABLED)
TEST(ProtectedMemoryTest, AssertMemoryIsReadOnly) {
  ASSERT_TRUE(internal::IsMemoryReadOnly(&g_structured_data->foo));
  { AutoWritableMemory writer(g_structured_data); }
  ASSERT_TRUE(internal::IsMemoryReadOnly(&g_structured_data->foo));
}

TEST(ProtectedMemoryDeathTest, VerifyTerminationOnAccess) {
  VerifyInstanceIsNotWriteable(g_structured_data.data_);
}

TEST(ProtectedMemoryTest, FailsIfDefinedOutsideOfProtectMemoryRegion) {
  ProtectedMemory<Data> data;
  EXPECT_CHECK_DEATH({ AutoWritableMemory writer(data); });
}
#endif  // BUILDFLAG(PROTECTED_MEMORY_ENABLED)

}  // namespace base
