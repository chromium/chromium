// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/40284755): Remove this and spanify to fix the errors.
#pragma allow_unsafe_buffers
#endif

#include "base/memory/protected_memory.h"

#include <stddef.h>
#include <stdint.h>

#include <climits>
#include <type_traits>

#include "base/memory/protected_memory_buildflags.h"
#include "base/synchronization/lock.h"
#include "base/test/gtest_util.h"
#include "build/build_config.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace base {
namespace {

struct Data {
  Data() = default;
  constexpr Data(int16_t f, int32_t b) : foo(f), bar(b) {}
  int16_t foo = 0;
  int32_t bar = -1;
};

struct DataWithNonTrivialConstructor {
  explicit DataWithNonTrivialConstructor(int f) : foo(f) {}
  int foo;
};

static_assert(
    !std::is_trivially_constructible_v<DataWithNonTrivialConstructor>);

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

DEFINE_PROTECTED_DATA ProtectedMemory<int> g_explicit_initialization;

TEST(ProtectedMemoryTest, ExplicitInitializationWithExplicitValue) {
  static ProtectedMemoryInitializer initializer_explicit_value(
      g_explicit_initialization, 4);

  EXPECT_EQ(*g_explicit_initialization, 4);
}

DEFINE_PROTECTED_DATA ProtectedMemory<int>
    g_explicit_initialization_with_default_value;

TEST(ProtectedMemoryTest, VerifyExplicitInitializationWithDefaultValue) {
  static ProtectedMemoryInitializer initializer_explicit_value(
      g_explicit_initialization_with_default_value);

  EXPECT_EQ(*g_explicit_initialization_with_default_value, int());
}

DEFINE_PROTECTED_DATA
ProtectedMemory<DataWithNonTrivialConstructor>
    g_lazily_initialized_with_explicit_initialization;

TEST(ProtectedMemoryTest, ExplicitLazyInitializationWithExplicitValue) {
  static ProtectedMemoryInitializer initializer_explicit_value(
      g_lazily_initialized_with_explicit_initialization, 4);

  EXPECT_EQ(g_lazily_initialized_with_explicit_initialization->foo, 4);
}

DEFINE_PROTECTED_DATA
ProtectedMemory<DataWithNonTrivialConstructor> g_uninitialized;

TEST(ProtectedMemoryDeathTest, AccessWithoutInitialization) {
  EXPECT_CHECK_DEATH_WITH(g_uninitialized.operator*(), "");
  EXPECT_CHECK_DEATH_WITH(g_uninitialized.operator->(), "");
}

#if BUILDFLAG(PROTECTED_MEMORY_ENABLED)
DEFINE_PROTECTED_DATA ProtectedMemory<Data> g_initialized;

TEST(ProtectedMemoryTest, VerifySetValue) {
  static ProtectedMemoryInitializer initializer_explicit_value(g_initialized);
  ASSERT_NE(g_initialized->foo, 5);
  EXPECT_EQ(g_initialized->bar, -1);
  {
    base::AutoWritableMemory writer(g_initialized);
    writer.GetProtectedDataPtr()->foo = 5;
  }
  EXPECT_EQ(g_initialized->foo, 5);
  EXPECT_EQ(g_initialized->bar, -1);
}

DEFINE_PROTECTED_DATA ProtectedMemory<Data> g_not_writable;

TEST(ProtectedMemoryDeathTest, AccessWithoutWriteAccessCrashes) {
  static ProtectedMemoryInitializer initializer_explicit_value(g_not_writable);
  VerifyInstanceIsNotWriteable(g_not_writable);
}

TEST(ProtectedMemoryDeathTest, FailsIfDefinedOutsideOfProtectMemoryRegion) {
  ProtectedMemory<Data> data;
  EXPECT_CHECK_DEATH({ AutoWritableMemory writer(data); });
}

#endif  // BUILDFLAG(PROTECTED_MEMORY_ENABLED)

}  // namespace
}  // namespace base
