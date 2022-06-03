// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/memory/shared_memory_mapping.h"

#include <stdint.h>

#include <limits>

#include "base/containers/span.h"
#include "base/memory/read_only_shared_memory_region.h"
#include "base/memory/writable_shared_memory_region.h"
#include "base/ranges/algorithm.h"
#include "base/test/metrics/histogram_tester.h"
#include "build/build_config.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace base {

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
  EXPECT_NE(nullptr, write_ptr);

  const uint32_t* read_ptr = read_mapping_.GetMemoryAs<uint32_t>();
  EXPECT_NE(nullptr, read_ptr);

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

  ranges::fill(write_span, 0);
  EXPECT_EQ(0u, read_span[0]);
  EXPECT_EQ(0u, read_span[1]);

  for (size_t i = 0; i < write_span.size(); ++i)
    write_span[i] = i + 1;
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

  ranges::fill(write_span, 0);
  EXPECT_EQ(0u, read_span[0]);
  EXPECT_EQ(0u, read_span[1]);
  EXPECT_EQ(0u, read_span_2[0]);

  for (size_t i = 0; i < write_span.size(); ++i)
    write_span[i] = i + 1;
  EXPECT_EQ(0x04030201u, read_span[0]);
  EXPECT_EQ(0x08070605u, read_span[1]);
  EXPECT_EQ(0x04030201u, read_span_2[0]);

  ranges::fill(write_span_2, 0);
  EXPECT_EQ(0u, read_span[0]);
  EXPECT_EQ(0x08070605u, read_span[1]);
  EXPECT_EQ(0u, read_span_2[0]);
}

TEST_F(SharedMemoryMappingTest, SpanWithZeroElementCount) {
  CreateMapping(sizeof(uint8_t) * 8);

  EXPECT_TRUE(write_mapping_.GetMemoryAsSpan<uint8_t>(0).empty());

  EXPECT_TRUE(read_mapping_.GetMemoryAsSpan<uint8_t>(0).empty());
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

// TODO(dcheng): This test is temporarily disabled on iOS. iOS devices allow
// the creation of a 1GB shared memory region, but don't allow the region to be
// mapped.
#if !defined(OS_IOS)
TEST_F(SharedMemoryMappingTest, TotalMappedSizeLimit) {
  // Nothing interesting to test if the address space isn't 64 bits, since
  // there's no real limit enforced on 32 bits other than complete address
  // space exhaustion.
  // Also exclude NaCl since pointers are 32 bits on all architectures:
  // https://bugs.chromium.org/p/nativeclient/issues/detail?id=1162
#if defined(ARCH_CPU_64_BITS) && !defined(OS_NACL)
  base::HistogramTester histogram_tester;
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
  EXPECT_THAT(
      histogram_tester.GetAllSamples("SharedMemory.MapBlockedForSecurity"),
      ::testing::ElementsAre(Bucket(0, 31), Bucket(1, 1)));
#endif  // defined(ARCH_CPU_64_BITS)
}
#endif  // !defined(OS_IOS)

}  // namespace base
