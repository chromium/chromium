// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/containers/span_reader.h"

#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using testing::Optional;

namespace base {
namespace {

TEST(SpanReaderTest, Construct) {
  std::array<const int, 5u> kArray = {1, 2, 3, 4, 5};

  auto r = SpanReader(base::span(kArray));
  EXPECT_EQ(r.remaining(), 5u);
  EXPECT_EQ(r.remaining_span().data(), &kArray[0u]);
  EXPECT_EQ(r.remaining_span().size(), 5u);
}

TEST(SpanReaderTest, Skip) {
  std::array<const int, 5u> kArray = {1, 2, 3, 4, 5};

  auto r = SpanReader(base::span(kArray));
  EXPECT_EQ(r.num_read(), 0u);
  EXPECT_FALSE(r.Skip(6u));
  EXPECT_THAT(r.Skip(2u), Optional(base::span(kArray).first(2u)));
  EXPECT_EQ(r.num_read(), 2u);
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

TEST(SpanReaderTest, ReadCopy) {
  std::array<const int, 5u> kArray = {1, 2, 3, 4, 5};

  auto r = SpanReader(base::span(kArray));
  {
    std::array<int, 2u> s;
    EXPECT_TRUE(r.ReadCopy(s));
    EXPECT_TRUE(s == base::span(kArray).subspan(0u, 2u));
    EXPECT_EQ(r.remaining(), 3u);
  }
  {
    std::array<int, 5u> s;
    EXPECT_FALSE(r.ReadCopy(s));
    EXPECT_EQ(r.remaining(), 3u);
  }
  {
    std::array<int, 1u> s;
    EXPECT_TRUE(r.ReadCopy(s));
    EXPECT_TRUE(s == base::span(kArray).subspan(2u, 1u));
    EXPECT_EQ(r.remaining(), 2u);
  }
  {
    std::array<int, 2u> s;
    EXPECT_TRUE(r.ReadCopy(s));
    EXPECT_TRUE(s == base::span(kArray).subspan(3u, 2u));
    EXPECT_EQ(r.remaining(), 0u);
  }
}

TEST(SpanReaderTest, ReadBigEndian_Unsigned) {
  const std::array<uint8_t, 5u> kArray = {1, 2, 3, 4, 5};
  const std::array<uint8_t, 9u> kBigArray = {1, 2, 3, 4, 5, 6, 7, 8, 9};

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
  {
    uint64_t val;

    auto r = SpanReader(base::span(kBigArray));
    EXPECT_TRUE(r.Skip(1u));
    EXPECT_TRUE(r.ReadU64BigEndian(val));
    EXPECT_EQ(r.remaining(), 0u);
    EXPECT_EQ(val, 0x0203040506070809llu);
  }
}

TEST(SpanReaderTest, ReadBigEndian_Signed) {
  const std::array<uint8_t, 5u> kArray = {1, 2, 3, 4, 5};
  const std::array<uint8_t, 9u> kBigArray = {1, 2, 3, 4, 5, 6, 7, 8, 9};

  {
    int8_t val;

    auto r = SpanReader(base::span(kArray));
    EXPECT_TRUE(r.Skip(1u));
    EXPECT_TRUE(r.ReadI8BigEndian(val));
    EXPECT_EQ(r.remaining(), 3u);
    EXPECT_EQ(val, 0x02);
  }
  {
    int16_t val;

    auto r = SpanReader(base::span(kArray));
    EXPECT_TRUE(r.Skip(1u));
    EXPECT_TRUE(r.ReadI16BigEndian(val));
    EXPECT_EQ(r.remaining(), 2u);
    EXPECT_EQ(val, 0x0203);
  }
  {
    int32_t val;

    auto r = SpanReader(base::span(kArray));
    EXPECT_TRUE(r.Skip(1u));
    EXPECT_TRUE(r.ReadI32BigEndian(val));
    EXPECT_EQ(r.remaining(), 0u);
    EXPECT_EQ(val, 0x02030405);
  }
  {
    int64_t val;

    auto r = SpanReader(base::span(kBigArray));
    EXPECT_TRUE(r.Skip(1u));
    EXPECT_TRUE(r.ReadI64BigEndian(val));
    EXPECT_EQ(r.remaining(), 0u);
    EXPECT_EQ(val, 0x0203040506070809ll);
  }
}

TEST(SpanReaderTest, ReadLittleEndian_Unsigned) {
  const std::array<uint8_t, 5u> kArray = {1, 2, 3, 4, 5};
  const std::array<uint8_t, 9u> kBigArray = {1, 2, 3, 4, 5, 6, 7, 8, 9};

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
  {
    uint64_t val;

    auto r = SpanReader(base::span(kBigArray));
    EXPECT_TRUE(r.Skip(1u));
    EXPECT_TRUE(r.ReadU64LittleEndian(val));
    EXPECT_EQ(r.remaining(), 0u);
    EXPECT_EQ(val, 0x0908070605040302llu);
  }
}

TEST(SpanReaderTest, ReadLittleEndian_Signed) {
  const std::array<uint8_t, 5u> kArray = {1, 2, 3, 4, 5};
  const std::array<uint8_t, 9u> kBigArray = {1, 2, 3, 4, 5, 6, 7, 8, 9};

  {
    int8_t val;

    auto r = SpanReader(base::span(kArray));
    EXPECT_TRUE(r.Skip(1u));
    EXPECT_TRUE(r.ReadI8LittleEndian(val));
    EXPECT_EQ(r.remaining(), 3u);
    EXPECT_EQ(val, 0x02);
  }
  {
    int16_t val;

    auto r = SpanReader(base::span(kArray));
    EXPECT_TRUE(r.Skip(1u));
    EXPECT_TRUE(r.ReadI16LittleEndian(val));
    EXPECT_EQ(r.remaining(), 2u);
    EXPECT_EQ(val, 0x0302);
  }
  {
    int32_t val;

    auto r = SpanReader(base::span(kArray));
    EXPECT_TRUE(r.Skip(1u));
    EXPECT_TRUE(r.ReadI32LittleEndian(val));
    EXPECT_EQ(r.remaining(), 0u);
    EXPECT_EQ(val, 0x05040302);
  }
  {
    int64_t val;

    auto r = SpanReader(base::span(kBigArray));
    EXPECT_TRUE(r.Skip(1u));
    EXPECT_TRUE(r.ReadI64LittleEndian(val));
    EXPECT_EQ(r.remaining(), 0u);
    EXPECT_EQ(val, 0x0908070605040302ll);
  }
}

TEST(SpanReaderTest, ReadNativeEndian_Unsigned) {
  const std::array<uint8_t, 5u> kArray = {1, 2, 3, 4, 5};
  const std::array<uint8_t, 9u> kBigArray = {1, 2, 3, 4, 5, 6, 7, 8, 9};

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
  {
    uint64_t val;

    auto r = SpanReader(base::span(kBigArray));
    EXPECT_TRUE(r.Skip(1u));
    EXPECT_TRUE(r.ReadU64NativeEndian(val));
    EXPECT_EQ(r.remaining(), 0u);
    EXPECT_EQ(val, 0x0908070605040302llu);
  }
}

TEST(SpanReaderTest, ReadNativeEndian_Signed) {
  const std::array<uint8_t, 5u> kArray = {1, 2, 3, 4, 5};
  const std::array<uint8_t, 9u> kBigArray = {1, 2, 3, 4, 5, 6, 7, 8, 9};

  {
    int8_t val;

    auto r = SpanReader(base::span(kArray));
    EXPECT_TRUE(r.Skip(1u));
    EXPECT_TRUE(r.ReadI8NativeEndian(val));
    EXPECT_EQ(r.remaining(), 3u);
    EXPECT_EQ(val, 0x02);
  }
  {
    int16_t val;

    auto r = SpanReader(base::span(kArray));
    EXPECT_TRUE(r.Skip(1u));
    EXPECT_TRUE(r.ReadI16NativeEndian(val));
    EXPECT_EQ(r.remaining(), 2u);
    EXPECT_EQ(val, 0x0302);
  }
  {
    int32_t val;

    auto r = SpanReader(base::span(kArray));
    EXPECT_TRUE(r.Skip(1u));
    EXPECT_TRUE(r.ReadI32NativeEndian(val));
    EXPECT_EQ(r.remaining(), 0u);
    EXPECT_EQ(val, 0x05040302);
  }
  {
    int64_t val;

    auto r = SpanReader(base::span(kBigArray));
    EXPECT_TRUE(r.Skip(1u));
    EXPECT_TRUE(r.ReadI64NativeEndian(val));
    EXPECT_EQ(r.remaining(), 0u);
    EXPECT_EQ(val, 0x0908070605040302ll);
  }
}

TEST(SpanReaderTest, ReadChar) {
  std::array<const uint8_t, 5u> kArray = {1, 2, 3, 4, 5};

  auto r = SpanReader(base::span(kArray));
  EXPECT_EQ(r.num_read(), 0u);

  char c;
  EXPECT_TRUE(r.ReadChar(c));
  EXPECT_EQ(c, char{1});
  EXPECT_TRUE(r.Skip(3u));
  EXPECT_TRUE(r.ReadChar(c));
  EXPECT_EQ(c, char{5});
  EXPECT_FALSE(r.ReadChar(c));
  EXPECT_EQ(c, char{5});
}

}  // namespace
}  // namespace base
