// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/containers/span_writer.h"

#include <array>
#include <memory>

#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace base {
namespace {

using testing::ElementsAre;
using testing::Optional;
using testing::Pointee;

TEST(SpanWriterTest, Construct) {
  std::array<int, 5u> kArray = {1, 2, 3, 4, 5};

  auto r = SpanWriter(base::span(kArray));
  EXPECT_EQ(r.remaining(), 5u);
  EXPECT_EQ(r.remaining_span().data(), &kArray[0u]);
  EXPECT_EQ(r.remaining_span().size(), 5u);
}

TEST(SpanWriterTest, WriteSpan) {
  // Dynamic size.
  {
    std::array<int, 5u> kArray = {1, 2, 3, 4, 5};
    auto r = SpanWriter(base::span(kArray));
    EXPECT_EQ(r.num_written(), 0u);

    EXPECT_TRUE(r.Write(base::span({9, 8}).subspan(0u)));
    EXPECT_EQ(r.remaining(), 3u);
    EXPECT_EQ(r.num_written(), 2u);
    EXPECT_EQ(kArray, base::span({9, 8, 3, 4, 5}));

    EXPECT_TRUE(r.Write(base::span<int>()));
    EXPECT_EQ(r.remaining(), 3u);
    EXPECT_EQ(r.num_written(), 2u);
    EXPECT_EQ(kArray, base::span({9, 8, 3, 4, 5}));

    EXPECT_FALSE(r.Write(base::span({7, 6, -1, -1}).subspan(0u)));
    EXPECT_EQ(r.remaining(), 3u);
    EXPECT_EQ(r.num_written(), 2u);
    EXPECT_EQ(kArray, base::span({9, 8, 3, 4, 5}));

    EXPECT_TRUE(r.Write(base::span({7, 6, -1}).subspan(0u)));
    EXPECT_EQ(r.remaining(), 0u);
    EXPECT_EQ(r.num_written(), 5u);
    EXPECT_EQ(kArray, base::span({9, 8, 7, 6, -1}));

    EXPECT_TRUE(r.Write(base::span<int>()));
    EXPECT_EQ(r.remaining(), 0u);
    EXPECT_EQ(r.num_written(), 5u);
    EXPECT_EQ(kArray, base::span({9, 8, 7, 6, -1}));
  }

  // Fixed size with mutable input.
  {
    std::array<int, 5u> kArray = {1, 2, 3, 4, 5};
    auto r = SpanWriter(base::span(kArray));
    EXPECT_EQ(r.num_written(), 0u);

    EXPECT_TRUE(r.Write(base::span({9, 8})));
    EXPECT_EQ(r.remaining(), 3u);
    EXPECT_EQ(r.num_written(), 2u);
    EXPECT_EQ(kArray, base::span({9, 8, 3, 4, 5}));

    EXPECT_TRUE(r.Write(base::span<int, 0u>()));
    EXPECT_EQ(r.remaining(), 3u);
    EXPECT_EQ(r.num_written(), 2u);
    EXPECT_EQ(kArray, base::span({9, 8, 3, 4, 5}));

    EXPECT_FALSE(r.Write(base::span({7, 6, -1, -1})));
    EXPECT_EQ(r.remaining(), 3u);
    EXPECT_EQ(r.num_written(), 2u);
    EXPECT_EQ(kArray, base::span({9, 8, 3, 4, 5}));

    EXPECT_TRUE(r.Write(base::span({7, 6, -1})));
    EXPECT_EQ(r.remaining(), 0u);
    EXPECT_EQ(r.num_written(), 5u);
    EXPECT_EQ(kArray, base::span({9, 8, 7, 6, -1}));

    EXPECT_TRUE(r.Write(base::span<int, 0u>()));
    EXPECT_EQ(r.remaining(), 0u);
    EXPECT_EQ(r.num_written(), 5u);
    EXPECT_EQ(kArray, base::span({9, 8, 7, 6, -1}));
  }

  // Fixed size with const input.
  {
    std::array<int, 5u> kArray = {1, 2, 3, 4, 5};
    auto r = SpanWriter(base::span(kArray));
    EXPECT_EQ(r.num_written(), 0u);

    std::array<const int, 2u> kConstArray = {9, 8};
    EXPECT_TRUE(r.Write(base::span(kConstArray)));
    EXPECT_EQ(r.remaining(), 3u);
    EXPECT_EQ(r.num_written(), 2u);
    EXPECT_EQ(kArray, base::span({9, 8, 3, 4, 5}));
  }
}

TEST(SpanWriterTest, WriteValue) {
  auto array = std::to_array<int>({1, 2});

  auto r = SpanWriter(span(array));
  EXPECT_TRUE(r.Write(10));
  EXPECT_TRUE(r.Write(20));
  EXPECT_THAT(array, ElementsAre(10, 20));
}

TEST(SpanWriterTest, WriteValueMoveOnly) {
  std::array<std::unique_ptr<int>, 2> array;

  auto r = SpanWriter(span(array));
  EXPECT_TRUE(r.Write(std::make_unique<int>(23)));
  EXPECT_TRUE(r.Write(std::make_unique<int>(88)));
  EXPECT_THAT(array, ElementsAre(testing::Pointee(23), testing::Pointee(88)));
}

TEST(SpanWriterTest, Skip) {
  std::array<int, 5u> kArray = {1, 2, 3, 4, 5};

  auto r = SpanWriter(base::span(kArray));
  auto s = r.Skip(2u);
  static_assert(std::same_as<decltype(s), std::optional<base::span<int>>>);
  EXPECT_THAT(s, Optional(base::span(kArray).first<2u>()));
  EXPECT_EQ(r.remaining(), 3u);
  EXPECT_EQ(r.remaining_span(), base::span({3, 4, 5}));

  EXPECT_FALSE(r.Skip(12u));
  EXPECT_EQ(r.remaining(), 3u);
  EXPECT_EQ(r.remaining_span(), base::span({3, 4, 5}));
}

TEST(SpanWriterTest, SkipFixed) {
  std::array<int, 5u> kArray = {1, 2, 3, 4, 5};

  auto r = SpanWriter(base::span(kArray));
  auto s = r.Skip<2u>();
  static_assert(std::same_as<decltype(s), std::optional<base::span<int, 2>>>);
  EXPECT_THAT(s, Optional(base::span(kArray).first<2u>()));
  EXPECT_EQ(r.remaining(), 3u);
  EXPECT_EQ(r.remaining_span(), base::span({3, 4, 5}));

  EXPECT_FALSE(r.Skip<12u>());
  EXPECT_EQ(r.remaining(), 3u);
  EXPECT_EQ(r.remaining_span(), base::span({3, 4, 5}));
}

TEST(SpanWriterTest, WriteNativeEndian_Unsigned) {
  std::array<uint8_t, 5u> kArray = {1, 2, 3, 4, 5};
  std::array<uint8_t, 9u> kBigArray = {1, 1, 1, 1, 1, 1, 1, 1, 1};

  {
    auto r = SpanWriter(base::span(kArray));
    EXPECT_TRUE(r.Skip(1u));
    EXPECT_TRUE(r.WriteU8NativeEndian(uint8_t{0x09}));
    EXPECT_EQ(r.remaining(), 3u);
    EXPECT_EQ(kArray, base::span({uint8_t{1}, uint8_t{9}, uint8_t{3},
                                  uint8_t{4}, uint8_t{5}}));
  }
  {
    auto r = SpanWriter(base::span(kArray));
    EXPECT_TRUE(r.Skip(1u));
    EXPECT_TRUE(r.WriteU16NativeEndian(uint16_t{0x0809}));
    EXPECT_EQ(r.remaining(), 2u);
    EXPECT_EQ(kArray, base::span({uint8_t{1}, uint8_t{9}, uint8_t{8},
                                  uint8_t{4}, uint8_t{5}}));
  }
  {
    auto r = SpanWriter(base::span(kArray));
    EXPECT_TRUE(r.Skip(1u));
    EXPECT_TRUE(r.WriteU32NativeEndian(0x06070809u));
    EXPECT_EQ(r.remaining(), 0u);
    EXPECT_EQ(kArray, base::span({uint8_t{1}, uint8_t{9}, uint8_t{8},
                                  uint8_t{7}, uint8_t{6}}));
  }
  {
    auto r = SpanWriter(base::span(kBigArray));
    EXPECT_TRUE(r.Skip(1u));
    EXPECT_TRUE(r.WriteU64NativeEndian(0x0203040506070809lu));
    EXPECT_EQ(r.remaining(), 0u);
    EXPECT_EQ(kBigArray, base::span({uint8_t{1}, uint8_t{9}, uint8_t{8},
                                     uint8_t{7}, uint8_t{6}, uint8_t{5},
                                     uint8_t{4}, uint8_t{3}, uint8_t{2}}));
  }
}

TEST(SpanWriterTest, WriteNativeEndian_Signed) {
  std::array<uint8_t, 5u> kArray = {1, 2, 3, 4, 5};
  std::array<uint8_t, 9u> kBigArray = {1, 1, 1, 1, 1, 1, 1, 1, 1};

  {
    auto r = SpanWriter(base::span(kArray));
    EXPECT_TRUE(r.Skip(1u));
    EXPECT_TRUE(r.WriteI8NativeEndian(int8_t{-0x09}));
    EXPECT_EQ(r.remaining(), 3u);
    EXPECT_EQ(kArray, base::span({uint8_t{1}, uint8_t{0xf7}, uint8_t{3},
                                  uint8_t{4}, uint8_t{5}}));
  }
  {
    auto r = SpanWriter(base::span(kArray));
    EXPECT_TRUE(r.Skip(1u));
    EXPECT_TRUE(r.WriteI16NativeEndian(int16_t{-0x0809}));
    EXPECT_EQ(r.remaining(), 2u);
    EXPECT_EQ(kArray, base::span({uint8_t{1}, uint8_t{0xf7}, uint8_t{0xf7},
                                  uint8_t{4}, uint8_t{5}}));
  }
  {
    auto r = SpanWriter(base::span(kArray));
    EXPECT_TRUE(r.Skip(1u));
    EXPECT_TRUE(r.WriteI32NativeEndian(-0x06070809));
    EXPECT_EQ(r.remaining(), 0u);
    EXPECT_EQ(kArray, base::span({uint8_t{1}, uint8_t{0xf7}, uint8_t{0xf7},
                                  uint8_t{0xf8}, uint8_t{0xf9}}));
  }
  {
    auto r = SpanWriter(base::span(kBigArray));
    EXPECT_TRUE(r.Skip(1u));
    EXPECT_TRUE(r.WriteI64NativeEndian(-0x0203040506070809l));
    EXPECT_EQ(r.remaining(), 0u);
    EXPECT_EQ(kBigArray,
              base::span({uint8_t{1}, uint8_t{0xf7}, uint8_t{0xf7},
                          uint8_t{0xf8}, uint8_t{0xf9}, uint8_t{0xfa},
                          uint8_t{0xfb}, uint8_t{0xfc}, uint8_t{0xfd}}));
  }
}

TEST(SpanWriterTest, WriteLittleEndian_Unsigned) {
  std::array<uint8_t, 5u> kArray = {1, 2, 3, 4, 5};
  std::array<uint8_t, 9u> kBigArray = {1, 1, 1, 1, 1, 1, 1, 1, 1};

  {
    auto r = SpanWriter(base::span(kArray));
    EXPECT_TRUE(r.Skip(1u));
    EXPECT_TRUE(r.WriteU8LittleEndian(uint8_t{0x09}));
    EXPECT_EQ(r.remaining(), 3u);
    EXPECT_EQ(kArray, base::span({uint8_t{1}, uint8_t{9}, uint8_t{3},
                                  uint8_t{4}, uint8_t{5}}));
  }
  {
    auto r = SpanWriter(base::span(kArray));
    EXPECT_TRUE(r.Skip(1u));
    EXPECT_TRUE(r.WriteU16LittleEndian(uint16_t{0x0809}));
    EXPECT_EQ(r.remaining(), 2u);
    EXPECT_EQ(kArray, base::span({uint8_t{1}, uint8_t{9}, uint8_t{8},
                                  uint8_t{4}, uint8_t{5}}));
  }
  {
    auto r = SpanWriter(base::span(kArray));
    EXPECT_TRUE(r.Skip(1u));
    EXPECT_TRUE(r.WriteU32LittleEndian(0x06070809u));
    EXPECT_EQ(r.remaining(), 0u);
    EXPECT_EQ(kArray, base::span({uint8_t{1}, uint8_t{9}, uint8_t{8},
                                  uint8_t{7}, uint8_t{6}}));
  }
  {
    auto r = SpanWriter(base::span(kBigArray));
    EXPECT_TRUE(r.Skip(1u));
    EXPECT_TRUE(r.WriteU64LittleEndian(0x0203040506070809lu));
    EXPECT_EQ(r.remaining(), 0u);
    EXPECT_EQ(kBigArray, base::span({uint8_t{1}, uint8_t{9}, uint8_t{8},
                                     uint8_t{7}, uint8_t{6}, uint8_t{5},
                                     uint8_t{4}, uint8_t{3}, uint8_t{2}}));
  }
}

TEST(SpanWriterTest, WriteLittleEndian_Signed) {
  std::array<uint8_t, 5u> kArray = {1, 2, 3, 4, 5};
  std::array<uint8_t, 9u> kBigArray = {1, 1, 1, 1, 1, 1, 1, 1, 1};

  {
    auto r = SpanWriter(base::span(kArray));
    EXPECT_TRUE(r.Skip(1u));
    EXPECT_TRUE(r.WriteI8LittleEndian(int8_t{-0x09}));
    EXPECT_EQ(r.remaining(), 3u);
    EXPECT_EQ(kArray, base::span({uint8_t{1}, uint8_t{0xf7}, uint8_t{3},
                                  uint8_t{4}, uint8_t{5}}));
  }
  {
    auto r = SpanWriter(base::span(kArray));
    EXPECT_TRUE(r.Skip(1u));
    EXPECT_TRUE(r.WriteI16LittleEndian(int16_t{-0x0809}));
    EXPECT_EQ(r.remaining(), 2u);
    EXPECT_EQ(kArray, base::span({uint8_t{1}, uint8_t{0xf7}, uint8_t{0xf7},
                                  uint8_t{4}, uint8_t{5}}));
  }
  {
    auto r = SpanWriter(base::span(kArray));
    EXPECT_TRUE(r.Skip(1u));
    EXPECT_TRUE(r.WriteI32LittleEndian(-0x06070809));
    EXPECT_EQ(r.remaining(), 0u);
    EXPECT_EQ(kArray, base::span({uint8_t{1}, uint8_t{0xf7}, uint8_t{0xf7},
                                  uint8_t{0xf8}, uint8_t{0xf9}}));
  }
  {
    auto r = SpanWriter(base::span(kBigArray));
    EXPECT_TRUE(r.Skip(1u));
    EXPECT_TRUE(r.WriteI64LittleEndian(-0x0203040506070809l));
    EXPECT_EQ(r.remaining(), 0u);
    EXPECT_EQ(kBigArray,
              base::span({uint8_t{1}, uint8_t{0xf7}, uint8_t{0xf7},
                          uint8_t{0xf8}, uint8_t{0xf9}, uint8_t{0xfa},
                          uint8_t{0xfb}, uint8_t{0xfc}, uint8_t{0xfd}}));
  }
}

TEST(SpanWriterTest, WriteBigEndian_Unsigned) {
  std::array<uint8_t, 5u> kArray = {1, 2, 3, 4, 5};
  std::array<uint8_t, 9u> kBigArray = {1, 1, 1, 1, 1, 1, 1, 1, 1};

  {
    auto r = SpanWriter(base::span(kArray));
    EXPECT_TRUE(r.Skip(1u));
    EXPECT_TRUE(r.WriteU8BigEndian(uint8_t{0x09}));
    EXPECT_EQ(r.remaining(), 3u);
    EXPECT_EQ(kArray, base::span({uint8_t{1}, uint8_t{9}, uint8_t{3},
                                  uint8_t{4}, uint8_t{5}}));
  }
  {
    auto r = SpanWriter(base::span(kArray));
    EXPECT_TRUE(r.Skip(1u));
    EXPECT_TRUE(r.WriteU16BigEndian(uint16_t{0x0809}));
    EXPECT_EQ(r.remaining(), 2u);
    EXPECT_EQ(kArray, base::span({uint8_t{1}, uint8_t{8}, uint8_t{9},
                                  uint8_t{4}, uint8_t{5}}));
  }
  {
    auto r = SpanWriter(base::span(kArray));
    EXPECT_TRUE(r.Skip(1u));
    EXPECT_TRUE(r.WriteU32BigEndian(0x06070809u));
    EXPECT_EQ(r.remaining(), 0u);
    EXPECT_EQ(kArray, base::span({uint8_t{1}, uint8_t{6}, uint8_t{7},
                                  uint8_t{8}, uint8_t{9}}));
  }
  {
    auto r = SpanWriter(base::span(kBigArray));
    EXPECT_TRUE(r.Skip(1u));
    EXPECT_TRUE(r.WriteU64BigEndian(0x0203040506070809lu));
    EXPECT_EQ(r.remaining(), 0u);
    EXPECT_EQ(kBigArray, base::span({uint8_t{1}, uint8_t{2}, uint8_t{3},
                                     uint8_t{4}, uint8_t{5}, uint8_t{6},
                                     uint8_t{7}, uint8_t{8}, uint8_t{9}}));
  }
}

TEST(SpanWriterTest, WriteBigEndian_Signed) {
  std::array<uint8_t, 5u> kArray = {1, 2, 3, 4, 5};
  std::array<uint8_t, 9u> kBigArray = {1, 1, 1, 1, 1, 1, 1, 1, 1};

  {
    auto r = SpanWriter(base::span(kArray));
    EXPECT_TRUE(r.Skip(1u));
    EXPECT_TRUE(r.WriteI8BigEndian(int8_t{-0x09}));
    EXPECT_EQ(r.remaining(), 3u);
    EXPECT_EQ(kArray, base::span({uint8_t{1}, uint8_t{0xf7}, uint8_t{3},
                                  uint8_t{4}, uint8_t{5}}));
  }
  {
    auto r = SpanWriter(base::span(kArray));
    EXPECT_TRUE(r.Skip(1u));
    EXPECT_TRUE(r.WriteI16BigEndian(int16_t{-0x0809}));
    EXPECT_EQ(r.remaining(), 2u);
    EXPECT_EQ(kArray, base::span({uint8_t{1}, uint8_t{0xf7}, uint8_t{0xf7},
                                  uint8_t{4}, uint8_t{5}}));
  }
  {
    auto r = SpanWriter(base::span(kArray));
    EXPECT_TRUE(r.Skip(1u));
    EXPECT_TRUE(r.WriteI32BigEndian(-0x06070809));
    EXPECT_EQ(r.remaining(), 0u);
    EXPECT_EQ(kArray, base::span({uint8_t{1}, uint8_t{0xf9}, uint8_t{0xf8},
                                  uint8_t{0xf7}, uint8_t{0xf7}}));
  }
  {
    auto r = SpanWriter(base::span(kBigArray));
    EXPECT_TRUE(r.Skip(1u));
    EXPECT_TRUE(r.WriteI64BigEndian(-0x0203040506070809l));
    EXPECT_EQ(r.remaining(), 0u);
    EXPECT_EQ(kBigArray,
              base::span({uint8_t{1}, uint8_t{0xfd}, uint8_t{0xfc},
                          uint8_t{0xfb}, uint8_t{0xfa}, uint8_t{0xf9},
                          uint8_t{0xf8}, uint8_t{0xf7}, uint8_t{0xf7}}));
  }
}

TEST(SpanWriterTest, Chars) {
  std::array<char, 5u> kArray = {'a', 'b', 'c', 'd', 'e'};

  auto r = SpanWriter(base::span(kArray));
  EXPECT_TRUE(r.Skip(1u));
  EXPECT_TRUE(r.Write(base::span({'f', 'g'})));
  EXPECT_EQ(r.remaining(), 2u);
  EXPECT_EQ(kArray, base::span({'a', 'f', 'g', 'd', 'e'}));
}

}  // namespace
}  // namespace base
