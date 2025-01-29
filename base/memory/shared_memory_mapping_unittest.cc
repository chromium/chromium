// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/memory/shared_memory_mapping.h"

#include <stdint.h>

#include <algorithm>
#include <atomic>
#include <limits>
#include <memory>
#include <type_traits>

#include "base/containers/span.h"
#include "base/memory/read_only_shared_memory_region.h"
#include "base/memory/writable_shared_memory_region.h"
#include "build/build_config.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace base {

namespace {
template <typename T>
using ElementType = std::pointer_traits<T>::element_type;
}

class SharedMemoryMappingTest : public ::testing::Test {
 protected:
  void CreateMapping(size_t size) {
    auto result = ReadOnlySharedMemoryRegion::Create(size);
    ASSERT_TRUE(result.IsValid());
    write_mapping_ = std::move(result.mapping);
    read_mapping_ = result.region.Map();
    ASSERT_TRUE(read_mapping_.IsValid());
  }

  WritableSharedMemoryMapping write_mapping_;
  ReadOnlySharedMemoryMapping read_mapping_;
};

TEST_F(SharedMemoryMappingTest, Invalid) {
  EXPECT_EQ(nullptr, write_mapping_.GetMemoryAs<uint8_t>());
  EXPECT_EQ(nullptr, read_mapping_.GetMemoryAs<uint8_t>());
  EXPECT_TRUE(write_mapping_.GetMemoryAsSpan<uint8_t>().empty());
  EXPECT_TRUE(read_mapping_.GetMemoryAsSpan<uint8_t>().empty());
  EXPECT_TRUE(write_mapping_.GetMemoryAsSpan<uint8_t>(1).empty());
  EXPECT_TRUE(read_mapping_.GetMemoryAsSpan<uint8_t>(1).empty());
}

TEST_F(SharedMemoryMappingTest, Scalar) {
  CreateMapping(sizeof(uint32_t));

  uint32_t* write_ptr = write_mapping_.GetMemoryAs<uint32_t>();
  ASSERT_NE(nullptr, write_ptr);

  const uint32_t* read_ptr = read_mapping_.GetMemoryAs<uint32_t>();
  ASSERT_NE(nullptr, read_ptr);

  *write_ptr = 0u;
  EXPECT_EQ(0u, *read_ptr);

  *write_ptr = 0x12345678u;
  EXPECT_EQ(0x12345678u, *read_ptr);
}

TEST_F(SharedMemoryMappingTest, SpanWithAutoDeducedElementCount) {
  CreateMapping(sizeof(uint8_t) * 8);

  span<uint8_t> write_span = write_mapping_.GetMemoryAsSpan<uint8_t>();
  ASSERT_EQ(8u, write_span.size());

  span<const uint32_t> read_span = read_mapping_.GetMemoryAsSpan<uint32_t>();
  ASSERT_EQ(2u, read_span.size());

  std::ranges::fill(write_span, 0);
  EXPECT_EQ(0u, read_span[0]);
  EXPECT_EQ(0u, read_span[1]);

  for (size_t i = 0; i < write_span.size(); ++i) {
    write_span[i] = i + 1;
  }
  EXPECT_EQ(0x04030201u, read_span[0]);
  EXPECT_EQ(0x08070605u, read_span[1]);
}

TEST_F(SharedMemoryMappingTest, SpanWithExplicitElementCount) {
  CreateMapping(sizeof(uint8_t) * 8);

  span<uint8_t> write_span = write_mapping_.GetMemoryAsSpan<uint8_t>(8);
  ASSERT_EQ(8u, write_span.size());

  span<uint8_t> write_span_2 = write_mapping_.GetMemoryAsSpan<uint8_t>(4);
  ASSERT_EQ(4u, write_span_2.size());

  span<const uint32_t> read_span = read_mapping_.GetMemoryAsSpan<uint32_t>(2);
  ASSERT_EQ(2u, read_span.size());

  span<const uint32_t> read_span_2 = read_mapping_.GetMemoryAsSpan<uint32_t>(1);
  ASSERT_EQ(1u, read_span_2.size());

  std::ranges::fill(write_span, 0);
  EXPECT_EQ(0u, read_span[0]);
  EXPECT_EQ(0u, read_span[1]);
  EXPECT_EQ(0u, read_span_2[0]);

  for (size_t i = 0; i < write_span.size(); ++i) {
    write_span[i] = i + 1;
  }
  EXPECT_EQ(0x04030201u, read_span[0]);
  EXPECT_EQ(0x08070605u, read_span[1]);
  EXPECT_EQ(0x04030201u, read_span_2[0]);

  std::ranges::fill(write_span_2, 0);
  EXPECT_EQ(0u, read_span[0]);
  EXPECT_EQ(0x08070605u, read_span[1]);
  EXPECT_EQ(0u, read_span_2[0]);
}

TEST_F(SharedMemoryMappingTest, SpanWithZeroElementCount) {
  CreateMapping(sizeof(uint8_t) * 8);

  EXPECT_TRUE(write_mapping_.GetMemoryAsSpan<uint8_t>(0).empty());

  EXPECT_TRUE(read_mapping_.GetMemoryAsSpan<uint8_t>(0).empty());
}

TEST_F(SharedMemoryMappingTest, ConstCorrectness) {
  // All memory accessors for read-only mappings should return const T.
  ReadOnlySharedMemoryMapping ro;

  static_assert(std::is_const_v<ElementType<decltype(ro.data())>>);
  static_assert(std::is_const_v<ElementType<decltype(ro.begin())>>);
  static_assert(std::is_const_v<ElementType<decltype(ro.end())>>);
  static_assert(std::is_const_v<ElementType<decltype(ro.memory())>>);
  static_assert(
      std::is_const_v<ElementType<decltype(ro.GetMemoryAs<uint8_t>())>>);
  static_assert(
      std::is_const_v<ElementType<decltype(ro.GetMemoryAs<const uint8_t>())>>);
  static_assert(
      std::is_const_v<decltype(ro.GetMemoryAsSpan<uint8_t>())::element_type>);
  static_assert(std::is_const_v<
                decltype(ro.GetMemoryAsSpan<const uint8_t>())::element_type>);
  static_assert(
      std::is_const_v<decltype(ro.GetMemoryAsSpan<uint8_t>(1))::element_type>);
  static_assert(std::is_const_v<decltype(ro.GetMemoryAsSpan<const uint8_t>(
                    1))::element_type>);

  // Making the mapping const should still allow all accessors to be called.
  const ReadOnlySharedMemoryMapping cro;
  static_assert(std::is_const_v<ElementType<decltype(cro.data())>>);
  static_assert(std::is_const_v<ElementType<decltype(cro.begin())>>);
  static_assert(std::is_const_v<ElementType<decltype(cro.end())>>);
  static_assert(std::is_const_v<ElementType<decltype(cro.memory())>>);
  static_assert(
      std::is_const_v<ElementType<decltype(cro.GetMemoryAs<uint8_t>())>>);
  static_assert(
      std::is_const_v<ElementType<decltype(cro.GetMemoryAs<const uint8_t>())>>);
  static_assert(
      std::is_const_v<decltype(cro.GetMemoryAsSpan<uint8_t>())::element_type>);
  static_assert(std::is_const_v<
                decltype(cro.GetMemoryAsSpan<const uint8_t>())::element_type>);
  static_assert(
      std::is_const_v<decltype(cro.GetMemoryAsSpan<uint8_t>(1))::element_type>);
  static_assert(std::is_const_v<decltype(cro.GetMemoryAsSpan<const uint8_t>(
                    1))::element_type>);

  // Accessors for writable mappings should be non-const unless requested.
  WritableSharedMemoryMapping rw;
  static_assert(!std::is_const_v<ElementType<decltype(rw.data())>>);
  static_assert(!std::is_const_v<ElementType<decltype(rw.begin())>>);
  static_assert(!std::is_const_v<ElementType<decltype(rw.end())>>);
  static_assert(!std::is_const_v<ElementType<decltype(rw.memory())>>);
  static_assert(
      !std::is_const_v<ElementType<decltype(rw.GetMemoryAs<uint8_t>())>>);
  static_assert(
      std::is_const_v<ElementType<decltype(rw.GetMemoryAs<const uint8_t>())>>);
  static_assert(
      !std::is_const_v<decltype(rw.GetMemoryAsSpan<uint8_t>())::element_type>);
  static_assert(std::is_const_v<
                decltype(rw.GetMemoryAsSpan<const uint8_t>())::element_type>);
  static_assert(
      !std::is_const_v<decltype(rw.GetMemoryAsSpan<uint8_t>(1))::element_type>);
  static_assert(std::is_const_v<decltype(rw.GetMemoryAsSpan<const uint8_t>(
                    1))::element_type>);

  // Making the mapping const should still allow all accessors to be called, but
  // they should now return const T.
  const WritableSharedMemoryMapping crw;
  static_assert(std::is_const_v<ElementType<decltype(crw.data())>>);
  static_assert(std::is_const_v<ElementType<decltype(crw.begin())>>);
  static_assert(std::is_const_v<ElementType<decltype(crw.end())>>);
  static_assert(std::is_const_v<ElementType<decltype(crw.memory())>>);
  static_assert(
      std::is_const_v<ElementType<decltype(crw.GetMemoryAs<uint8_t>())>>);
  static_assert(
      std::is_const_v<ElementType<decltype(crw.GetMemoryAs<const uint8_t>())>>);
  static_assert(
      std::is_const_v<decltype(crw.GetMemoryAsSpan<uint8_t>())::element_type>);
  static_assert(std::is_const_v<
                decltype(crw.GetMemoryAsSpan<const uint8_t>())::element_type>);
  static_assert(
      std::is_const_v<decltype(crw.GetMemoryAsSpan<uint8_t>(1))::element_type>);
  static_assert(std::is_const_v<decltype(crw.GetMemoryAsSpan<const uint8_t>(
                    1))::element_type>);
}

TEST_F(SharedMemoryMappingTest, TooBigScalar) {
  CreateMapping(sizeof(uint8_t));

  EXPECT_EQ(nullptr, write_mapping_.GetMemoryAs<uint32_t>());

  EXPECT_EQ(nullptr, read_mapping_.GetMemoryAs<uint32_t>());
}

TEST_F(SharedMemoryMappingTest, TooBigSpanWithAutoDeducedElementCount) {
  CreateMapping(sizeof(uint8_t));

  EXPECT_TRUE(write_mapping_.GetMemoryAsSpan<uint32_t>().empty());

  EXPECT_TRUE(read_mapping_.GetMemoryAsSpan<uint32_t>().empty());
}

TEST_F(SharedMemoryMappingTest, TooBigSpanWithExplicitElementCount) {
  CreateMapping(sizeof(uint8_t));

  // Deliberately pick element counts such that a naive bounds calculation would
  // overflow.
  EXPECT_TRUE(write_mapping_
                  .GetMemoryAsSpan<uint32_t>(std::numeric_limits<size_t>::max())
                  .empty());

  EXPECT_TRUE(read_mapping_
                  .GetMemoryAsSpan<uint32_t>(std::numeric_limits<size_t>::max())
                  .empty());
}

TEST_F(SharedMemoryMappingTest, Atomic) {
  CreateMapping(sizeof(std::atomic<uint32_t>));

  auto* write_ptr = write_mapping_.GetMemoryAs<std::atomic<uint32_t>>();
  ASSERT_NE(nullptr, write_ptr);

  // Placement new to initialize the std::atomic in place.
  new (write_ptr) std::atomic<uint32_t>;

  const auto* read_ptr = read_mapping_.GetMemoryAs<std::atomic<uint32_t>>();
  ASSERT_NE(nullptr, read_ptr);

  write_ptr->store(0u, std::memory_order_relaxed);
  EXPECT_EQ(0u, read_ptr->load(std::memory_order_relaxed));

  write_ptr->store(0x12345678u, std::memory_order_relaxed);
  EXPECT_EQ(0x12345678u, read_ptr->load(std::memory_order_relaxed));
}

TEST_F(SharedMemoryMappingTest, TooBigAtomic) {
  CreateMapping(sizeof(std::atomic<uint8_t>));

  EXPECT_EQ(nullptr, write_mapping_.GetMemoryAs<std::atomic<uint32_t>>());

  EXPECT_EQ(nullptr, read_mapping_.GetMemoryAs<std::atomic<uint32_t>>());
}

// TODO(dcheng): This test is temporarily disabled on iOS. iOS devices allow
// the creation of a 1GB shared memory region, but don't allow the region to be
// mapped.
#if !BUILDFLAG(IS_IOS)
// TODO(crbug.com/40846204) Fix flakiness and re-enable on Linux and ChromeOS.
#if BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_CHROMEOS)
#define MAYBE_TotalMappedSizeLimit DISABLED_TotalMappedSizeLimit
#else
#define MAYBE_TotalMappedSizeLimit TotalMappedSizeLimit
#endif  // BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_CHROMEOS)
TEST_F(SharedMemoryMappingTest, MAYBE_TotalMappedSizeLimit) {
  // Nothing interesting to test if the address space isn't 64 bits, since
  // there's no real limit enforced on 32 bits other than complete address
  // space exhaustion.
  // Also exclude NaCl since pointers are 32 bits on all architectures:
  // https://bugs.chromium.org/p/nativeclient/issues/detail?id=1162
#if defined(ARCH_CPU_64_BITS) && !BUILDFLAG(IS_NACL)
  auto region = WritableSharedMemoryRegion::Create(1024 * 1024 * 1024);
  ASSERT_TRUE(region.IsValid());
  // The limit is 32GB of mappings on 64-bit platforms, so the final mapping
  // should fail.
  std::vector<WritableSharedMemoryMapping> mappings(32);
  for (size_t i = 0; i < mappings.size(); ++i) {
    SCOPED_TRACE(i);
    auto& mapping = mappings[i];
    mapping = region.Map();
    EXPECT_EQ(&mapping != &mappings.back(), mapping.IsValid());
  }
#endif  // defined(ARCH_CPU_64_BITS)
}
#endif  // !BUILDFLAG(IS_IOS)

}  // namespace base
