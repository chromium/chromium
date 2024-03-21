// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/containers/span_writer.h"

#include "testing/gtest/include/gtest/gtest.h"

namespace base {
namespace {

TEST(SpanWriterTest, Construct) {
  std::array<int, 5u> kArray = {1, 2, 3, 4, 5};

  auto r = SpanWriter(base::span(kArray));
  EXPECT_EQ(r.remaining(), 5u);
  EXPECT_EQ(r.remaining_span().data(), &kArray[0u]);
  EXPECT_EQ(r.remaining_span().size(), 5u);
}

TEST(SpanWriterTest, Write) {
  // Dynamic size.
  {
    std::array<int, 5u> kArray = {1, 2, 3, 4, 5};
    auto r = SpanWriter(base::span(kArray));

    EXPECT_TRUE(r.Write(base::span({9, 8}).subspan(0u)));
    EXPECT_EQ(r.remaining(), 3u);
    EXPECT_EQ(kArray, base::span({9, 8, 3, 4, 5}));

    EXPECT_TRUE(r.Write(base::span<int>()));
    EXPECT_EQ(r.remaining(), 3u);
    EXPECT_EQ(kArray, base::span({9, 8, 3, 4, 5}));

    EXPECT_FALSE(r.Write(base::span({7, 6, -1, -1}).subspan(0u)));
    EXPECT_EQ(r.remaining(), 3u);
    EXPECT_EQ(kArray, base::span({9, 8, 3, 4, 5}));

    EXPECT_TRUE(r.Write(base::span({7, 6, -1}).subspan(0u)));
    EXPECT_EQ(r.remaining(), 0u);
    EXPECT_EQ(kArray, base::span({9, 8, 7, 6, -1}));

    EXPECT_TRUE(r.Write(base::span<int>()));
    EXPECT_EQ(r.remaining(), 0u);
    EXPECT_EQ(kArray, base::span({9, 8, 7, 6, -1}));
  }

  // Fixed size with mutable input.
  {
    std::array<int, 5u> kArray = {1, 2, 3, 4, 5};
    auto r = SpanWriter(base::span(kArray));

    EXPECT_TRUE(r.Write(base::span({9, 8})));
    EXPECT_EQ(r.remaining(), 3u);
    EXPECT_EQ(kArray, base::span({9, 8, 3, 4, 5}));

    EXPECT_TRUE(r.Write(base::span<int, 0u>()));
    EXPECT_EQ(r.remaining(), 3u);
    EXPECT_EQ(kArray, base::span({9, 8, 3, 4, 5}));

    EXPECT_FALSE(r.Write(base::span({7, 6, -1, -1})));
    EXPECT_EQ(r.remaining(), 3u);
    EXPECT_EQ(kArray, base::span({9, 8, 3, 4, 5}));

    EXPECT_TRUE(r.Write(base::span({7, 6, -1})));
    EXPECT_EQ(r.remaining(), 0u);
    EXPECT_EQ(kArray, base::span({9, 8, 7, 6, -1}));

    EXPECT_TRUE(r.Write(base::span<int, 0u>()));
    EXPECT_EQ(r.remaining(), 0u);
    EXPECT_EQ(kArray, base::span({9, 8, 7, 6, -1}));
  }

  // Fixed size with const input.
  {
    std::array<int, 5u> kArray = {1, 2, 3, 4, 5};
    auto r = SpanWriter(base::span(kArray));

    std::array<const int, 2u> kConstArray = {9, 8};
    EXPECT_TRUE(r.Write(base::span(kConstArray)));
    EXPECT_EQ(r.remaining(), 3u);
    EXPECT_EQ(kArray, base::span({9, 8, 3, 4, 5}));
  }
}

TEST(SpanWriterTest, Skip) {
  std::array<int, 5u> kArray = {1, 2, 3, 4, 5};

  auto r = SpanWriter(base::span(kArray));
  EXPECT_TRUE(r.Skip(2u));
  EXPECT_EQ(r.remaining(), 3u);
  EXPECT_EQ(r.remaining_span(), base::span({3, 4, 5}));

  EXPECT_FALSE(r.Skip(12u));
  EXPECT_EQ(r.remaining(), 3u);
  EXPECT_EQ(r.remaining_span(), base::span({3, 4, 5}));
}

TEST(SpanWriterTest, WriteNativeEndian) {
  std::array<uint8_t, 5u> kArray = {uint8_t{1}, uint8_t{2}, uint8_t{3},
                                    uint8_t{4}, uint8_t{5}};

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

  std::array<uint8_t, 9u> kBigArray = {uint8_t{1}, uint8_t{1}, uint8_t{1},
                                       uint8_t{1}, uint8_t{1}, uint8_t{1},
                                       uint8_t{1}, uint8_t{1}, uint8_t{1}};

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

TEST(SpanWriterTest, WriteLittleEndian) {
  std::array<uint8_t, 5u> kArray = {uint8_t{1}, uint8_t{2}, uint8_t{3},
                                    uint8_t{4}, uint8_t{5}};

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

  std::array<uint8_t, 9u> kBigArray = {uint8_t{1}, uint8_t{1}, uint8_t{1},
                                       uint8_t{1}, uint8_t{1}, uint8_t{1},
                                       uint8_t{1}, uint8_t{1}, uint8_t{1}};

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

TEST(SpanWriterTest, WriteBigEndian) {
  std::array<uint8_t, 5u> kArray = {uint8_t{1}, uint8_t{2}, uint8_t{3},
                                    uint8_t{4}, uint8_t{5}};

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

  std::array<uint8_t, 9u> kBigArray = {uint8_t{1}, uint8_t{1}, uint8_t{1},
                                       uint8_t{1}, uint8_t{1}, uint8_t{1},
                                       uint8_t{1}, uint8_t{1}, uint8_t{1}};

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
