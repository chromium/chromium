// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/containers/span_reader.h"

#include "testing/gtest/include/gtest/gtest.h"

namespace base {
namespace {

TEST(SpanReaderTest, Construct) {
  std::array<const int, 5u> kArray = {1, 2, 3, 4, 5};

  auto r = SpanReader(base::span(kArray));
  EXPECT_EQ(r.remaining(), 5u);
  EXPECT_EQ(r.remaining_span().data(), &kArray[0u]);
  EXPECT_EQ(r.remaining_span().size(), 5u);
}

TEST(SpanReaderTest, Read) {
  std::array<const int, 5u> kArray = {1, 2, 3, 4, 5};

  auto r = SpanReader(base::span(kArray));
  EXPECT_EQ(r.num_read(), 0u);
  {
    auto o = r.Read(2u);
    static_assert(std::same_as<decltype(*o), span<const int>&>);
    ASSERT_TRUE(o.has_value());
    EXPECT_TRUE(*o == base::span(kArray).subspan(0u, 2u));
    EXPECT_EQ(r.remaining(), 3u);
    EXPECT_EQ(r.num_read(), 2u);
  }
  {
    auto o = r.Read(5u);
    static_assert(std::same_as<decltype(*o), span<const int>&>);
    EXPECT_FALSE(o.has_value());
    EXPECT_EQ(r.remaining(), 3u);
    EXPECT_EQ(r.num_read(), 2u);
  }
  {
    auto o = r.Read(1u);
    static_assert(std::same_as<decltype(*o), span<const int>&>);
    ASSERT_TRUE(o.has_value());
    EXPECT_TRUE(*o == base::span(kArray).subspan(2u, 1u));
    EXPECT_EQ(r.remaining(), 2u);
    EXPECT_EQ(r.num_read(), 3u);
  }
  {
    auto o = r.Read(2u);
    static_assert(std::same_as<decltype(*o), span<const int>&>);
    ASSERT_TRUE(o.has_value());
    EXPECT_TRUE(*o == base::span(kArray).subspan(3u, 2u));
    EXPECT_EQ(r.remaining(), 0u);
    EXPECT_EQ(r.num_read(), 5u);
  }
}

TEST(SpanReaderTest, ReadFixed) {
  std::array<const int, 5u> kArray = {1, 2, 3, 4, 5};

  auto r = SpanReader(base::span(kArray));
  {
    auto o = r.Read<2u>();
    static_assert(std::same_as<decltype(*o), span<const int, 2u>&>);
    ASSERT_TRUE(o.has_value());
    EXPECT_TRUE(*o == base::span(kArray).subspan(0u, 2u));
    EXPECT_EQ(r.remaining(), 3u);
  }
  {
    auto o = r.Read<5u>();
    static_assert(std::same_as<decltype(*o), span<const int, 5u>&>);
    EXPECT_FALSE(o.has_value());
    EXPECT_EQ(r.remaining(), 3u);
  }
  {
    auto o = r.Read<1u>();
    static_assert(std::same_as<decltype(*o), span<const int, 1u>&>);
    ASSERT_TRUE(o.has_value());
    EXPECT_TRUE(*o == base::span(kArray).subspan(2u, 1u));
    EXPECT_EQ(r.remaining(), 2u);
  }
  {
    auto o = r.Read<2u>();
    static_assert(std::same_as<decltype(*o), span<const int, 2u>&>);
    ASSERT_TRUE(o.has_value());
    EXPECT_TRUE(*o == base::span(kArray).subspan(3u, 2u));
    EXPECT_EQ(r.remaining(), 0u);
  }
}

TEST(SpanReaderTest, ReadInto) {
  std::array<const int, 5u> kArray = {1, 2, 3, 4, 5};

  auto r = SpanReader(base::span(kArray));
  {
    base::span<const int> s;
    EXPECT_TRUE(r.ReadInto(2u, s));
    EXPECT_TRUE(s == base::span(kArray).subspan(0u, 2u));
    EXPECT_EQ(r.remaining(), 3u);
  }
  {
    base::span<const int> s;
    EXPECT_FALSE(r.ReadInto(5u, s));
    EXPECT_EQ(r.remaining(), 3u);
  }
  {
    base::span<const int> s;
    EXPECT_TRUE(r.ReadInto(1u, s));
    EXPECT_TRUE(s == base::span(kArray).subspan(2u, 1u));
    EXPECT_EQ(r.remaining(), 2u);
  }
  {
    base::span<const int> s;
    EXPECT_TRUE(r.ReadInto(2u, s));
    EXPECT_TRUE(s == base::span(kArray).subspan(3u, 2u));
    EXPECT_EQ(r.remaining(), 0u);
  }
}

TEST(SpanReaderTest, ReadBigEndian) {
  std::array<uint8_t, 5u> kArray = {uint8_t{1}, uint8_t{2}, uint8_t{3},
                                    uint8_t{4}, uint8_t{5}};

  {
    uint8_t val;

    auto r = SpanReader(base::span(kArray));
    EXPECT_TRUE(r.Skip(1u));
    EXPECT_TRUE(r.ReadU8BigEndian(val));
    EXPECT_EQ(r.remaining(), 3u);
    EXPECT_EQ(val, 0x02u);
  }

  {
    uint16_t val;

    auto r = SpanReader(base::span(kArray));
    EXPECT_TRUE(r.Skip(1u));
    EXPECT_TRUE(r.ReadU16BigEndian(val));
    EXPECT_EQ(r.remaining(), 2u);
    EXPECT_EQ(val, 0x0203u);
  }

  {
    uint32_t val;

    auto r = SpanReader(base::span(kArray));
    EXPECT_TRUE(r.Skip(1u));
    EXPECT_TRUE(r.ReadU32BigEndian(val));
    EXPECT_EQ(r.remaining(), 0u);
    EXPECT_EQ(val, 0x02030405u);
  }

  std::array<uint8_t, 9u> kBigArray = {uint8_t{1}, uint8_t{2}, uint8_t{3},
                                       uint8_t{4}, uint8_t{5}, uint8_t{6},
                                       uint8_t{7}, uint8_t{8}, uint8_t{9}};

  {
    uint64_t val;

    auto r = SpanReader(base::span(kBigArray));
    EXPECT_TRUE(r.Skip(1u));
    EXPECT_TRUE(r.ReadU64BigEndian(val));
    EXPECT_EQ(r.remaining(), 0u);
    EXPECT_EQ(val, 0x0203040506070809llu);
  }
}

TEST(SpanReaderTest, ReadLittleEndian) {
  std::array<uint8_t, 5u> kArray = {uint8_t{1}, uint8_t{2}, uint8_t{3},
                                    uint8_t{4}, uint8_t{5}};

  {
    uint8_t val;

    auto r = SpanReader(base::span(kArray));
    EXPECT_TRUE(r.Skip(1u));
    EXPECT_TRUE(r.ReadU8LittleEndian(val));
    EXPECT_EQ(r.remaining(), 3u);
    EXPECT_EQ(val, 0x02u);
  }

  {
    uint16_t val;

    auto r = SpanReader(base::span(kArray));
    EXPECT_TRUE(r.Skip(1u));
    EXPECT_TRUE(r.ReadU16LittleEndian(val));
    EXPECT_EQ(r.remaining(), 2u);
    EXPECT_EQ(val, 0x0302u);
  }

  {
    uint32_t val;

    auto r = SpanReader(base::span(kArray));
    EXPECT_TRUE(r.Skip(1u));
    EXPECT_TRUE(r.ReadU32LittleEndian(val));
    EXPECT_EQ(r.remaining(), 0u);
    EXPECT_EQ(val, 0x05040302u);
  }

  std::array<uint8_t, 9u> kBigArray = {uint8_t{1}, uint8_t{2}, uint8_t{3},
                                       uint8_t{4}, uint8_t{5}, uint8_t{6},
                                       uint8_t{7}, uint8_t{8}, uint8_t{9}};

  {
    uint64_t val;

    auto r = SpanReader(base::span(kBigArray));
    EXPECT_TRUE(r.Skip(1u));
    EXPECT_TRUE(r.ReadU64LittleEndian(val));
    EXPECT_EQ(r.remaining(), 0u);
    EXPECT_EQ(val, 0x0908070605040302llu);
  }
}

TEST(SpanReaderTest, ReadNativeEndian) {
  std::array<uint8_t, 5u> kArray = {uint8_t{1}, uint8_t{2}, uint8_t{3},
                                    uint8_t{4}, uint8_t{5}};

  {
    uint8_t val;

    auto r = SpanReader(base::span(kArray));
    EXPECT_TRUE(r.Skip(1u));
    EXPECT_TRUE(r.ReadU8NativeEndian(val));
    EXPECT_EQ(r.remaining(), 3u);
    EXPECT_EQ(val, 0x02u);
  }

  {
    uint16_t val;

    auto r = SpanReader(base::span(kArray));
    EXPECT_TRUE(r.Skip(1u));
    EXPECT_TRUE(r.ReadU16NativeEndian(val));
    EXPECT_EQ(r.remaining(), 2u);
    EXPECT_EQ(val, 0x0302u);
  }

  {
    uint32_t val;

    auto r = SpanReader(base::span(kArray));
    EXPECT_TRUE(r.Skip(1u));
    EXPECT_TRUE(r.ReadU32NativeEndian(val));
    EXPECT_EQ(r.remaining(), 0u);
    EXPECT_EQ(val, 0x05040302u);
  }

  std::array<uint8_t, 9u> kBigArray = {uint8_t{1}, uint8_t{2}, uint8_t{3},
                                       uint8_t{4}, uint8_t{5}, uint8_t{6},
                                       uint8_t{7}, uint8_t{8}, uint8_t{9}};

  {
    uint64_t val;

    auto r = SpanReader(base::span(kBigArray));
    EXPECT_TRUE(r.Skip(1u));
    EXPECT_TRUE(r.ReadU64NativeEndian(val));
    EXPECT_EQ(r.remaining(), 0u);
    EXPECT_EQ(val, 0x0908070605040302llu);
  }
}

}  // namespace
}  // namespace base
