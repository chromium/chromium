// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/containers/span_reader.h"

#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using testing::Optional;

namespace base {
namespace {

// Tests that SpanReader methods are usable in constexpr contexts.
constexpr std::array<uint8_t, 5u> kConstArray = {1, 2, 3, 4, 5};
constexpr std::array<uint8_t, 9u> kConstBigArray = {1, 2, 3, 4, 5, 6, 7, 8, 9};
constexpr std::array<uint8_t, 4u> kConstFloatArray = {0x00, 0x00, 0x80, 0x3f};
constexpr std::array<uint8_t, 8u> kConstDoubleArray = {0x00, 0x00, 0x00, 0x00,
                                                       0x00, 0x00, 0xf0, 0x3f};

constexpr size_t TestRemaining() {
  auto r = SpanReader(span(kConstArray));
  return r.remaining();
}
static_assert(TestRemaining() == 5u);

constexpr size_t TestNumRead() {
  auto r = SpanReader(span(kConstArray));
  r.Skip(2u);
  return r.num_read();
}
static_assert(TestNumRead() == 2u);

constexpr auto TestSkip() {
  auto r = SpanReader(span(kConstArray));
  return r.Skip(2u);
}
static_assert(TestSkip().has_value());
static_assert(TestSkip().value().size() == 2u);
static_assert(TestSkip().value().data() == &kConstArray[0u]);

constexpr bool TestRemainingSpan() {
  auto r = SpanReader(span(kConstArray));
  r.Skip(1u);
  return r.remaining_span().data() == &kConstArray[1u];
}
static_assert(TestRemainingSpan());

constexpr auto TestReadU8BigEndian() {
  auto r = SpanReader(span(kConstArray));
  return r.ReadU8BigEndian();
}
static_assert(TestReadU8BigEndian().has_value());
static_assert(TestReadU8BigEndian().value() == 0x01u);

constexpr auto TestReadU8LittleEndian() {
  auto r = SpanReader(span(kConstArray));
  return r.ReadU8LittleEndian();
}
static_assert(TestReadU8LittleEndian().has_value());
static_assert(TestReadU8LittleEndian().value() == 0x01u);

constexpr auto TestReadU8NativeEndian() {
  auto r = SpanReader(span(kConstArray));
  return r.ReadU8NativeEndian();
}
static_assert(TestReadU8NativeEndian().has_value());
static_assert(TestReadU8NativeEndian().value() == 0x01u);

constexpr auto TestReadU16BigEndian() {
  auto r = SpanReader(span(kConstArray));
  r.Skip(1u);
  return r.ReadU16BigEndian();
}
static_assert(TestReadU16BigEndian().has_value());
static_assert(TestReadU16BigEndian().value() == 0x0203u);

constexpr auto TestReadU16LittleEndian() {
  auto r = SpanReader(span(kConstArray));
  r.Skip(1u);
  return r.ReadU16LittleEndian();
}
static_assert(TestReadU16LittleEndian().has_value());
static_assert(TestReadU16LittleEndian().value() == 0x0302u);

constexpr auto TestReadU16NativeEndian() {
  auto r = SpanReader(span(kConstArray));
  r.Skip(1u);
  return r.ReadU16NativeEndian();
}
static_assert(TestReadU16NativeEndian().has_value());
static_assert(TestReadU16NativeEndian().value() == 0x0302u);

constexpr auto TestReadU32BigEndian() {
  auto r = SpanReader(span(kConstArray));
  r.Skip(1u);
  return r.ReadU32BigEndian();
}
static_assert(TestReadU32BigEndian().has_value());
static_assert(TestReadU32BigEndian().value() == 0x02030405u);

constexpr auto TestReadU32LittleEndian() {
  auto r = SpanReader(span(kConstArray));
  r.Skip(1u);
  return r.ReadU32LittleEndian();
}
static_assert(TestReadU32LittleEndian().has_value());
static_assert(TestReadU32LittleEndian().value() == 0x05040302u);

constexpr auto TestReadU32NativeEndian() {
  auto r = SpanReader(span(kConstArray));
  r.Skip(1u);
  return r.ReadU32NativeEndian();
}
static_assert(TestReadU32NativeEndian().has_value());
static_assert(TestReadU32NativeEndian().value() == 0x05040302u);

constexpr auto TestReadU64BigEndian() {
  auto r = SpanReader(span(kConstBigArray));
  r.Skip(1u);
  return r.ReadU64BigEndian();
}
static_assert(TestReadU64BigEndian().has_value());
static_assert(TestReadU64BigEndian().value() == 0x0203040506070809llu);

constexpr auto TestReadU64LittleEndian() {
  auto r = SpanReader(span(kConstBigArray));
  r.Skip(1u);
  return r.ReadU64LittleEndian();
}
static_assert(TestReadU64LittleEndian().has_value());
static_assert(TestReadU64LittleEndian().value() == 0x0908070605040302llu);

constexpr auto TestReadU64NativeEndian() {
  auto r = SpanReader(span(kConstBigArray));
  r.Skip(1u);
  return r.ReadU64NativeEndian();
}
static_assert(TestReadU64NativeEndian().has_value());
static_assert(TestReadU64NativeEndian().value() == 0x0908070605040302llu);

constexpr auto TestReadI8BigEndian() {
  auto r = SpanReader(span(kConstArray));
  return r.ReadI8BigEndian();
}
static_assert(TestReadI8BigEndian().has_value());
static_assert(TestReadI8BigEndian().value() == 0x01);

constexpr auto TestReadI8LittleEndian() {
  auto r = SpanReader(span(kConstArray));
  return r.ReadI8LittleEndian();
}
static_assert(TestReadI8LittleEndian().has_value());
static_assert(TestReadI8LittleEndian().value() == 0x01);

constexpr auto TestReadI8NativeEndian() {
  auto r = SpanReader(span(kConstArray));
  return r.ReadI8NativeEndian();
}
static_assert(TestReadI8NativeEndian().has_value());
static_assert(TestReadI8NativeEndian().value() == 0x01);

constexpr auto TestReadI16BigEndian() {
  auto r = SpanReader(span(kConstArray));
  r.Skip(1u);
  return r.ReadI16BigEndian();
}
static_assert(TestReadI16BigEndian().has_value());
static_assert(TestReadI16BigEndian().value() == 0x0203);

constexpr auto TestReadI16LittleEndian() {
  auto r = SpanReader(span(kConstArray));
  r.Skip(1u);
  return r.ReadI16LittleEndian();
}
static_assert(TestReadI16LittleEndian().has_value());
static_assert(TestReadI16LittleEndian().value() == 0x0302);

constexpr auto TestReadI16NativeEndian() {
  auto r = SpanReader(span(kConstArray));
  r.Skip(1u);
  return r.ReadI16NativeEndian();
}
static_assert(TestReadI16NativeEndian().has_value());
static_assert(TestReadI16NativeEndian().value() == 0x0302);

constexpr auto TestReadI32BigEndian() {
  auto r = SpanReader(span(kConstArray));
  r.Skip(1u);
  return r.ReadI32BigEndian();
}
static_assert(TestReadI32BigEndian().has_value());
static_assert(TestReadI32BigEndian().value() == 0x02030405);

constexpr auto TestReadI32LittleEndian() {
  auto r = SpanReader(span(kConstArray));
  r.Skip(1u);
  return r.ReadI32LittleEndian();
}
static_assert(TestReadI32LittleEndian().has_value());
static_assert(TestReadI32LittleEndian().value() == 0x05040302);

constexpr auto TestReadI32NativeEndian() {
  auto r = SpanReader(span(kConstArray));
  r.Skip(1u);
  return r.ReadI32NativeEndian();
}
static_assert(TestReadI32NativeEndian().has_value());
static_assert(TestReadI32NativeEndian().value() == 0x05040302);

constexpr auto TestReadI64BigEndian() {
  auto r = SpanReader(span(kConstBigArray));
  r.Skip(1u);
  return r.ReadI64BigEndian();
}
static_assert(TestReadI64BigEndian().has_value());
static_assert(TestReadI64BigEndian().value() == 0x0203040506070809ll);

constexpr auto TestReadI64LittleEndian() {
  auto r = SpanReader(span(kConstBigArray));
  r.Skip(1u);
  return r.ReadI64LittleEndian();
}
static_assert(TestReadI64LittleEndian().has_value());
static_assert(TestReadI64LittleEndian().value() == 0x0908070605040302ll);

constexpr auto TestReadI64NativeEndian() {
  auto r = SpanReader(span(kConstBigArray));
  r.Skip(1u);
  return r.ReadI64NativeEndian();
}
static_assert(TestReadI64NativeEndian().has_value());
static_assert(TestReadI64NativeEndian().value() == 0x0908070605040302ll);

constexpr auto TestReadChar() {
  auto r = SpanReader(span(kConstArray));
  return r.ReadChar();
}
static_assert(TestReadChar().has_value());
static_assert(TestReadChar().value() == char{1});

constexpr auto TestReadFloatNativeEndian() {
  auto r = SpanReader(span(kConstFloatArray));
  return r.ReadFloatNativeEndian();
}
static_assert(TestReadFloatNativeEndian().has_value());
static_assert(TestReadFloatNativeEndian().value() == 1.0f);

constexpr auto TestReadDoubleNativeEndian() {
  auto r = SpanReader(span(kConstDoubleArray));
  return r.ReadDoubleNativeEndian();
}
static_assert(TestReadDoubleNativeEndian().has_value());
static_assert(TestReadDoubleNativeEndian().value() == 1.0);

TEST(SpanReaderTest, Construct) {
  std::array<const int, 5u> kArray = {1, 2, 3, 4, 5};

  auto r = SpanReader(span(kArray));
  EXPECT_EQ(r.remaining(), 5u);
  EXPECT_EQ(r.remaining_span().data(), &kArray[0u]);
  EXPECT_EQ(r.remaining_span().size(), 5u);
}

TEST(SpanReaderTest, Skip) {
  std::array<const int, 5u> kArray = {1, 2, 3, 4, 5};

  auto r = SpanReader(span(kArray));
  EXPECT_EQ(r.num_read(), 0u);
  EXPECT_FALSE(r.Skip(6u));
  EXPECT_THAT(r.Skip(2u), Optional(span(kArray).first(2u)));
  EXPECT_EQ(r.num_read(), 2u);
}

TEST(SpanReaderTest, Read) {
  std::array<const int, 5u> kArray = {1, 2, 3, 4, 5};

  auto r = SpanReader(span(kArray));
  EXPECT_EQ(r.num_read(), 0u);
  {
    auto o = r.Read(2u);
    static_assert(std::same_as<decltype(*o), span<const int>&>);
    ASSERT_TRUE(o.has_value());
    EXPECT_TRUE(*o == span(kArray).subspan(0u, 2u));
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
    EXPECT_TRUE(*o == span(kArray).subspan(2u, 1u));
    EXPECT_EQ(r.remaining(), 2u);
    EXPECT_EQ(r.num_read(), 3u);
  }
  {
    auto o = r.Read(2u);
    static_assert(std::same_as<decltype(*o), span<const int>&>);
    ASSERT_TRUE(o.has_value());
    EXPECT_TRUE(*o == span(kArray).subspan(3u, 2u));
    EXPECT_EQ(r.remaining(), 0u);
    EXPECT_EQ(r.num_read(), 5u);
  }
}

TEST(SpanReaderTest, ReadFixed) {
  std::array<const int, 5u> kArray = {1, 2, 3, 4, 5};

  auto r = SpanReader(span(kArray));
  {
    auto o = r.Read<2u>();
    static_assert(std::same_as<decltype(*o), span<const int, 2u>&>);
    ASSERT_TRUE(o.has_value());
    EXPECT_TRUE(*o == span(kArray).subspan(0u, 2u));
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
    EXPECT_TRUE(*o == span(kArray).subspan(2u, 1u));
    EXPECT_EQ(r.remaining(), 2u);
  }
  {
    auto o = r.Read<2u>();
    static_assert(std::same_as<decltype(*o), span<const int, 2u>&>);
    ASSERT_TRUE(o.has_value());
    EXPECT_TRUE(*o == span(kArray).subspan(3u, 2u));
    EXPECT_EQ(r.remaining(), 0u);
  }
}

TEST(SpanReaderTest, ReadInto) {
  std::array<const int, 5u> kArray = {1, 2, 3, 4, 5};

  auto r = SpanReader(span(kArray));
  {
    span<const int> s;
    EXPECT_TRUE(r.ReadInto(2u, s));
    EXPECT_TRUE(s == span(kArray).subspan(0u, 2u));
    EXPECT_EQ(r.remaining(), 3u);
  }
  {
    span<const int> s;
    EXPECT_FALSE(r.ReadInto(5u, s));
    EXPECT_EQ(r.remaining(), 3u);
  }
  {
    span<const int> s;
    EXPECT_TRUE(r.ReadInto(1u, s));
    EXPECT_TRUE(s == span(kArray).subspan(2u, 1u));
    EXPECT_EQ(r.remaining(), 2u);
  }
  {
    span<const int> s;
    EXPECT_TRUE(r.ReadInto(2u, s));
    EXPECT_TRUE(s == span(kArray).subspan(3u, 2u));
    EXPECT_EQ(r.remaining(), 0u);
  }
}

TEST(SpanReaderTest, ReadCopy) {
  std::array<const int, 5u> kArray = {1, 2, 3, 4, 5};

  auto r = SpanReader(span(kArray));
  {
    std::array<int, 2u> s;
    EXPECT_TRUE(r.ReadCopy(s));
    EXPECT_TRUE(s == span(kArray).subspan(0u, 2u));
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
    EXPECT_TRUE(s == span(kArray).subspan(2u, 1u));
    EXPECT_EQ(r.remaining(), 2u);
  }
  {
    std::array<int, 2u> s;
    EXPECT_TRUE(r.ReadCopy(s));
    EXPECT_TRUE(s == span(kArray).subspan(3u, 2u));
    EXPECT_EQ(r.remaining(), 0u);
  }
}

TEST(SpanReaderTest, ReadBigEndian_Unsigned) {
  const std::array<uint8_t, 5u> kArray = {1, 2, 3, 4, 5};
  const std::array<uint8_t, 9u> kBigArray = {1, 2, 3, 4, 5, 6, 7, 8, 9};

  {
    uint8_t val;

    auto r = SpanReader(span(kArray));
    EXPECT_TRUE(r.Skip(1u));
    EXPECT_TRUE(r.ReadU8BigEndian(val));
    EXPECT_EQ(r.remaining(), 3u);
    EXPECT_EQ(val, 0x02u);
  }
  {
    uint16_t val;

    auto r = SpanReader(span(kArray));
    EXPECT_TRUE(r.Skip(1u));
    EXPECT_TRUE(r.ReadU16BigEndian(val));
    EXPECT_EQ(r.remaining(), 2u);
    EXPECT_EQ(val, 0x0203u);
  }
  {
    uint32_t val;

    auto r = SpanReader(span(kArray));
    EXPECT_TRUE(r.Skip(1u));
    EXPECT_TRUE(r.ReadU32BigEndian(val));
    EXPECT_EQ(r.remaining(), 0u);
    EXPECT_EQ(val, 0x02030405u);
  }
  {
    uint64_t val;

    auto r = SpanReader(span(kBigArray));
    EXPECT_TRUE(r.Skip(1u));
    EXPECT_TRUE(r.ReadU64BigEndian(val));
    EXPECT_EQ(r.remaining(), 0u);
    EXPECT_EQ(val, 0x0203040506070809llu);
  }
  {
    auto r = SpanReader(span(kArray));
    EXPECT_TRUE(r.Skip(1u));
    EXPECT_THAT(r.ReadU8BigEndian(), Optional(0x02u));
  }
  {
    auto r = SpanReader(span(kArray));
    EXPECT_TRUE(r.Skip(1u));
    EXPECT_THAT(r.ReadU16BigEndian(), Optional(0x0203u));
  }
  {
    auto r = SpanReader(span(kArray));
    EXPECT_TRUE(r.Skip(1u));
    EXPECT_THAT(r.ReadU32BigEndian(), Optional(0x02030405u));
  }
  {
    auto r = SpanReader(span(kBigArray));
    EXPECT_TRUE(r.Skip(1u));
    EXPECT_THAT(r.ReadU64BigEndian(), Optional(0x0203040506070809llu));
  }
}

TEST(SpanReaderTest, ReadBigEndian_Signed) {
  const std::array<uint8_t, 5u> kArray = {1, 2, 3, 4, 5};
  const std::array<uint8_t, 9u> kBigArray = {1, 2, 3, 4, 5, 6, 7, 8, 9};

  {
    int8_t val;

    auto r = SpanReader(span(kArray));
    EXPECT_TRUE(r.Skip(1u));
    EXPECT_TRUE(r.ReadI8BigEndian(val));
    EXPECT_EQ(r.remaining(), 3u);
    EXPECT_EQ(val, 0x02);
  }
  {
    int16_t val;

    auto r = SpanReader(span(kArray));
    EXPECT_TRUE(r.Skip(1u));
    EXPECT_TRUE(r.ReadI16BigEndian(val));
    EXPECT_EQ(r.remaining(), 2u);
    EXPECT_EQ(val, 0x0203);
  }
  {
    int32_t val;

    auto r = SpanReader(span(kArray));
    EXPECT_TRUE(r.Skip(1u));
    EXPECT_TRUE(r.ReadI32BigEndian(val));
    EXPECT_EQ(r.remaining(), 0u);
    EXPECT_EQ(val, 0x02030405);
  }
  {
    int64_t val;

    auto r = SpanReader(span(kBigArray));
    EXPECT_TRUE(r.Skip(1u));
    EXPECT_TRUE(r.ReadI64BigEndian(val));
    EXPECT_EQ(r.remaining(), 0u);
    EXPECT_EQ(val, 0x0203040506070809ll);
  }
  {
    auto r = SpanReader(span(kArray));
    EXPECT_TRUE(r.Skip(1u));
    EXPECT_THAT(r.ReadI8BigEndian(), Optional(0x02));
  }
  {
    auto r = SpanReader(span(kArray));
    EXPECT_TRUE(r.Skip(1u));
    EXPECT_THAT(r.ReadI16BigEndian(), Optional(0x0203));
  }
  {
    auto r = SpanReader(span(kArray));
    EXPECT_TRUE(r.Skip(1u));
    EXPECT_THAT(r.ReadI32BigEndian(), Optional(0x02030405));
  }
  {
    auto r = SpanReader(span(kBigArray));
    EXPECT_TRUE(r.Skip(1u));
    EXPECT_THAT(r.ReadI64BigEndian(), Optional(0x0203040506070809ll));
  }
}

TEST(SpanReaderTest, ReadLittleEndian_Unsigned) {
  const std::array<uint8_t, 5u> kArray = {1, 2, 3, 4, 5};
  const std::array<uint8_t, 9u> kBigArray = {1, 2, 3, 4, 5, 6, 7, 8, 9};

  {
    uint8_t val;

    auto r = SpanReader(span(kArray));
    EXPECT_TRUE(r.Skip(1u));
    EXPECT_TRUE(r.ReadU8LittleEndian(val));
    EXPECT_EQ(r.remaining(), 3u);
    EXPECT_EQ(val, 0x02u);
  }
  {
    uint16_t val;

    auto r = SpanReader(span(kArray));
    EXPECT_TRUE(r.Skip(1u));
    EXPECT_TRUE(r.ReadU16LittleEndian(val));
    EXPECT_EQ(r.remaining(), 2u);
    EXPECT_EQ(val, 0x0302u);
  }
  {
    uint32_t val;

    auto r = SpanReader(span(kArray));
    EXPECT_TRUE(r.Skip(1u));
    EXPECT_TRUE(r.ReadU32LittleEndian(val));
    EXPECT_EQ(r.remaining(), 0u);
    EXPECT_EQ(val, 0x05040302u);
  }
  {
    uint64_t val;

    auto r = SpanReader(span(kBigArray));
    EXPECT_TRUE(r.Skip(1u));
    EXPECT_TRUE(r.ReadU64LittleEndian(val));
    EXPECT_EQ(r.remaining(), 0u);
    EXPECT_EQ(val, 0x0908070605040302llu);
  }
  {
    auto r = SpanReader(span(kArray));
    EXPECT_TRUE(r.Skip(1u));
    EXPECT_THAT(r.ReadU8LittleEndian(), Optional(0x02u));
  }
  {
    auto r = SpanReader(span(kArray));
    EXPECT_TRUE(r.Skip(1u));
    EXPECT_THAT(r.ReadU16LittleEndian(), Optional(0x0302u));
  }
  {
    auto r = SpanReader(span(kArray));
    EXPECT_TRUE(r.Skip(1u));
    EXPECT_THAT(r.ReadU32LittleEndian(), Optional(0x05040302u));
  }
  {
    auto r = SpanReader(span(kBigArray));
    EXPECT_TRUE(r.Skip(1u));
    EXPECT_THAT(r.ReadU64LittleEndian(), Optional(0x0908070605040302llu));
  }
}

TEST(SpanReaderTest, ReadLittleEndian_Signed) {
  const std::array<uint8_t, 5u> kArray = {1, 2, 3, 4, 5};
  const std::array<uint8_t, 9u> kBigArray = {1, 2, 3, 4, 5, 6, 7, 8, 9};

  {
    int8_t val;

    auto r = SpanReader(span(kArray));
    EXPECT_TRUE(r.Skip(1u));
    EXPECT_TRUE(r.ReadI8LittleEndian(val));
    EXPECT_EQ(r.remaining(), 3u);
    EXPECT_EQ(val, 0x02);
  }
  {
    int16_t val;

    auto r = SpanReader(span(kArray));
    EXPECT_TRUE(r.Skip(1u));
    EXPECT_TRUE(r.ReadI16LittleEndian(val));
    EXPECT_EQ(r.remaining(), 2u);
    EXPECT_EQ(val, 0x0302);
  }
  {
    int32_t val;

    auto r = SpanReader(span(kArray));
    EXPECT_TRUE(r.Skip(1u));
    EXPECT_TRUE(r.ReadI32LittleEndian(val));
    EXPECT_EQ(r.remaining(), 0u);
    EXPECT_EQ(val, 0x05040302);
  }
  {
    int64_t val;

    auto r = SpanReader(span(kBigArray));
    EXPECT_TRUE(r.Skip(1u));
    EXPECT_TRUE(r.ReadI64LittleEndian(val));
    EXPECT_EQ(r.remaining(), 0u);
    EXPECT_EQ(val, 0x0908070605040302ll);
  }
  {
    auto r = SpanReader(span(kArray));
    EXPECT_TRUE(r.Skip(1u));
    EXPECT_THAT(r.ReadI8LittleEndian(), Optional(0x02));
  }
  {
    auto r = SpanReader(span(kArray));
    EXPECT_TRUE(r.Skip(1u));
    EXPECT_THAT(r.ReadI16LittleEndian(), Optional(0x0302));
  }
  {
    auto r = SpanReader(span(kArray));
    EXPECT_TRUE(r.Skip(1u));
    EXPECT_THAT(r.ReadI32LittleEndian(), Optional(0x05040302));
  }
  {
    auto r = SpanReader(span(kBigArray));
    EXPECT_TRUE(r.Skip(1u));
    EXPECT_THAT(r.ReadI64LittleEndian(), Optional(0x0908070605040302ll));
  }
}

TEST(SpanReaderTest, ReadNativeEndian_Unsigned) {
  const std::array<uint8_t, 5u> kArray = {1, 2, 3, 4, 5};
  const std::array<uint8_t, 9u> kBigArray = {1, 2, 3, 4, 5, 6, 7, 8, 9};

  {
    uint8_t val;

    auto r = SpanReader(span(kArray));
    EXPECT_TRUE(r.Skip(1u));
    EXPECT_TRUE(r.ReadU8NativeEndian(val));
    EXPECT_EQ(r.remaining(), 3u);
    EXPECT_EQ(val, 0x02u);
  }
  {
    uint16_t val;

    auto r = SpanReader(span(kArray));
    EXPECT_TRUE(r.Skip(1u));
    EXPECT_TRUE(r.ReadU16NativeEndian(val));
    EXPECT_EQ(r.remaining(), 2u);
    EXPECT_EQ(val, 0x0302u);
  }
  {
    uint32_t val;

    auto r = SpanReader(span(kArray));
    EXPECT_TRUE(r.Skip(1u));
    EXPECT_TRUE(r.ReadU32NativeEndian(val));
    EXPECT_EQ(r.remaining(), 0u);
    EXPECT_EQ(val, 0x05040302u);
  }
  {
    uint64_t val;

    auto r = SpanReader(span(kBigArray));
    EXPECT_TRUE(r.Skip(1u));
    EXPECT_TRUE(r.ReadU64NativeEndian(val));
    EXPECT_EQ(r.remaining(), 0u);
    EXPECT_EQ(val, 0x0908070605040302llu);
  }
  {
    auto r = SpanReader(span(kArray));
    EXPECT_TRUE(r.Skip(1u));
    EXPECT_THAT(r.ReadU8NativeEndian(), Optional(0x02u));
  }
  {
    auto r = SpanReader(span(kArray));
    EXPECT_TRUE(r.Skip(1u));
    EXPECT_THAT(r.ReadU16NativeEndian(), Optional(0x0302u));
  }
  {
    auto r = SpanReader(span(kArray));
    EXPECT_TRUE(r.Skip(1u));
    EXPECT_THAT(r.ReadU32NativeEndian(), Optional(0x05040302u));
  }
  {
    auto r = SpanReader(span(kBigArray));
    EXPECT_TRUE(r.Skip(1u));
    EXPECT_THAT(r.ReadU64NativeEndian(), Optional(0x0908070605040302llu));
  }
}

TEST(SpanReaderTest, ReadNativeEndian_Signed) {
  const std::array<uint8_t, 5u> kArray = {1, 2, 3, 4, 5};
  const std::array<uint8_t, 9u> kBigArray = {1, 2, 3, 4, 5, 6, 7, 8, 9};

  {
    int8_t val;

    auto r = SpanReader(span(kArray));
    EXPECT_TRUE(r.Skip(1u));
    EXPECT_TRUE(r.ReadI8NativeEndian(val));
    EXPECT_EQ(r.remaining(), 3u);
    EXPECT_EQ(val, 0x02);
  }
  {
    int16_t val;

    auto r = SpanReader(span(kArray));
    EXPECT_TRUE(r.Skip(1u));
    EXPECT_TRUE(r.ReadI16NativeEndian(val));
    EXPECT_EQ(r.remaining(), 2u);
    EXPECT_EQ(val, 0x0302);
  }
  {
    int32_t val;

    auto r = SpanReader(span(kArray));
    EXPECT_TRUE(r.Skip(1u));
    EXPECT_TRUE(r.ReadI32NativeEndian(val));
    EXPECT_EQ(r.remaining(), 0u);
    EXPECT_EQ(val, 0x05040302);
  }
  {
    int64_t val;

    auto r = SpanReader(span(kBigArray));
    EXPECT_TRUE(r.Skip(1u));
    EXPECT_TRUE(r.ReadI64NativeEndian(val));
    EXPECT_EQ(r.remaining(), 0u);
    EXPECT_EQ(val, 0x0908070605040302ll);
  }
  {
    auto r = SpanReader(span(kArray));
    EXPECT_TRUE(r.Skip(1u));
    EXPECT_THAT(r.ReadI8NativeEndian(), Optional(0x02));
  }
  {
    auto r = SpanReader(span(kArray));
    EXPECT_TRUE(r.Skip(1u));
    EXPECT_THAT(r.ReadI16NativeEndian(), Optional(0x0302));
  }
  {
    auto r = SpanReader(span(kArray));
    EXPECT_TRUE(r.Skip(1u));
    EXPECT_THAT(r.ReadI32NativeEndian(), Optional(0x05040302));
  }
  {
    auto r = SpanReader(span(kBigArray));
    EXPECT_TRUE(r.Skip(1u));
    EXPECT_THAT(r.ReadI64NativeEndian(), Optional(0x0908070605040302ll));
  }
}

TEST(SpanReaderTest, ReadChar) {
  std::array<const uint8_t, 5u> kArray = {1, 2, 3, 4, 5};

  auto r = SpanReader(span(kArray));
  EXPECT_EQ(r.num_read(), 0u);

  char c;
  EXPECT_TRUE(r.ReadChar(c));
  EXPECT_EQ(c, char{1});
  EXPECT_TRUE(r.Skip(3u));
  EXPECT_TRUE(r.ReadChar(c));
  EXPECT_EQ(c, char{5});
  EXPECT_FALSE(r.ReadChar(c));
  EXPECT_EQ(c, char{5});
  r = SpanReader(span(kArray));
  EXPECT_THAT(r.ReadChar(), Optional(char{1}));
}

TEST(SpanReaderTest, ReadFloatAndDouble) {
  // 1.0f in little endian is 00 00 80 3f
  const std::array<uint8_t, 4u> kFloatArray = {0x00, 0x00, 0x80, 0x3f};
  // 1.0 in little endian is 00 00 00 00 00 00 f0 3f
  const std::array<uint8_t, 8u> kDoubleArray = {0x00, 0x00, 0x00, 0x00,
                                                0x00, 0x00, 0xf0, 0x3f};

  auto r = SpanReader(span(kFloatArray));
  EXPECT_THAT(r.ReadFloatNativeEndian(), Optional(1.0f));

  r = SpanReader(span(kFloatArray));
  float f;
  EXPECT_TRUE(r.ReadFloatNativeEndian(f));
  EXPECT_EQ(f, 1.0f);

  r = SpanReader(span(kDoubleArray));
  EXPECT_THAT(r.ReadDoubleNativeEndian(), Optional(1.0));

  r = SpanReader(span(kDoubleArray));
  double d;
  EXPECT_TRUE(r.ReadDoubleNativeEndian(d));
  EXPECT_EQ(d, 1.0);
}

}  // namespace
}  // namespace base
