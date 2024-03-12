// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/big_endian.h"

#include <stdint.h>

#include <limits>

#include "base/strings/string_piece.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace base {

TEST(BigEndianReaderTest, ReadsValues) {
  std::array<uint8_t, 21u> data = {0,   1,   2,    3,    4,    5,    6,
                                   7,   8,   9,    0xA,  0xB,  0xC,  0xD,
                                   0xE, 0xF, 0x1A, 0x2B, 0x3C, 0x4D, 0x5E};
  uint8_t buf[2];
  uint8_t u8;
  uint16_t u16;
  uint32_t u32;
  uint64_t u64;
  base::StringPiece piece;

  {
    BigEndianReader reader(data);
    EXPECT_TRUE(reader.Skip(4));
    EXPECT_EQ(reader.remaining_bytes().data(), &data[4u]);
    EXPECT_EQ(reader.remaining(), sizeof(data) - 4);
    EXPECT_TRUE(reader.ReadU8(&u8));
    EXPECT_EQ(0x4, u8);
    EXPECT_TRUE(reader.ReadU16(&u16));
    EXPECT_EQ(0x0506, u16);
    EXPECT_TRUE(reader.ReadU32(&u32));
    EXPECT_EQ(0x0708090Au, u32);
    EXPECT_TRUE(reader.ReadU64(&u64));
    EXPECT_EQ(0x0B0C0D0E0F1A2B3Cllu, u64);
    base::StringPiece expected(reinterpret_cast<const char*>(reader.ptr()), 2);
    EXPECT_TRUE(reader.ReadPiece(&piece, 2));
    EXPECT_EQ(2u, piece.size());
    EXPECT_EQ(expected.data(), piece.data());
  }

  {
    BigEndianReader reader(data);
    // Fixed size span.
    auto s1 = reader.ReadSpan<2u>().value();
    static_assert(std::same_as<decltype(s1), base::span<const uint8_t, 2u>>);
    EXPECT_EQ(s1.data(), &data[0u]);
    EXPECT_EQ(s1.size(), 2u);

    // Dynamic size span.
    auto s2 = reader.ReadSpan(2u).value();
    static_assert(std::same_as<decltype(s2), base::span<const uint8_t>>);
    EXPECT_EQ(s2.data(), &data[2u]);
    EXPECT_EQ(s2.size(), 2u);

    buf[0] = buf[1] = uint8_t{0};

    // Fixed size span.
    EXPECT_TRUE(reader.ReadBytes(buf));
    EXPECT_EQ(buf[0], 4u);
    EXPECT_EQ(buf[1], 5u);

    // Dynamic size span.
    EXPECT_TRUE(reader.ReadBytes(span<uint8_t>(buf)));
    EXPECT_EQ(buf[0], 6u);
    EXPECT_EQ(buf[1], 7u);

    EXPECT_EQ(reader.remaining_bytes().data(), &data[8u]);
  }
}

TEST(BigEndianReaderTest, ReadsLengthPrefixedValues) {
  {
    uint8_t u8_prefixed_data[] = {8,   8,   9,    0xA,  0xB,  0xC,  0xD,
                                  0xE, 0xF, 0x1A, 0x2B, 0x3C, 0x4D, 0x5E};
    BigEndianReader reader(u8_prefixed_data, sizeof(u8_prefixed_data));

    base::StringPiece piece;
    ASSERT_TRUE(reader.ReadU8LengthPrefixed(&piece));
    // |reader| should skip both a u8 and the length-8 length-prefixed field.
    EXPECT_EQ(reader.ptr(), u8_prefixed_data + 9);
    EXPECT_EQ(piece.size(), 8u);
    EXPECT_EQ(reinterpret_cast<const uint8_t*>(piece.data()),
              u8_prefixed_data + 1);
  }

  {
    uint8_t u16_prefixed_data[] = {0,    8,    0xD,  0xE,  0xF,
                                   0x1A, 0x2B, 0x3C, 0x4D, 0x5E};
    BigEndianReader reader(u16_prefixed_data, sizeof(u16_prefixed_data));
    base::StringPiece piece;
    ASSERT_TRUE(reader.ReadU16LengthPrefixed(&piece));
    // |reader| should skip both a u16 and the length-8 length-prefixed field.
    EXPECT_EQ(reader.ptr(), u16_prefixed_data + 10);
    EXPECT_EQ(piece.size(), 8u);
    EXPECT_EQ(reinterpret_cast<const uint8_t*>(piece.data()),
              u16_prefixed_data + 2);

    // With no data left, we shouldn't be able to
    // read another u8 length prefix (or a u16 length prefix,
    // for that matter).
    EXPECT_FALSE(reader.ReadU8LengthPrefixed(&piece));
    EXPECT_FALSE(reader.ReadU16LengthPrefixed(&piece));
  }

  {
    // Make sure there's no issue reading a zero-value length prefix.
    uint8_t u16_prefixed_data[3] = {};
    BigEndianReader reader(u16_prefixed_data, sizeof(u16_prefixed_data));
    base::StringPiece piece;
    ASSERT_TRUE(reader.ReadU16LengthPrefixed(&piece));
    EXPECT_EQ(reader.ptr(), u16_prefixed_data + 2);
    EXPECT_EQ(reinterpret_cast<const uint8_t*>(piece.data()),
              u16_prefixed_data + 2);
    EXPECT_EQ(piece.size(), 0u);
  }
}

TEST(BigEndianReaderTest, LengthPrefixedReadsFailGracefully) {
  // We can't read 0xF (or, for that matter, 0xF8) bytes after the length
  // prefix: there isn't enough data.
  uint8_t data[] = {0xF, 8,   9,    0xA,  0xB,  0xC,  0xD,
                    0xE, 0xF, 0x1A, 0x2B, 0x3C, 0x4D, 0x5E};
  BigEndianReader reader(data, sizeof(data));
  base::StringPiece piece;
  EXPECT_FALSE(reader.ReadU8LengthPrefixed(&piece));
  EXPECT_EQ(data, reader.ptr());

  EXPECT_FALSE(reader.ReadU16LengthPrefixed(&piece));
  EXPECT_EQ(data, reader.ptr());
}

TEST(BigEndianReaderTest, RespectsLength) {
  uint8_t data[8];
  uint8_t buf[2];
  uint8_t u8;
  uint16_t u16;
  uint32_t u32;
  uint64_t u64;
  base::StringPiece piece;
  BigEndianReader reader(data, sizeof(data));
  // 8 left
  EXPECT_FALSE(reader.Skip(9));
  EXPECT_TRUE(reader.Skip(1));
  // 7 left
  EXPECT_FALSE(reader.ReadU64(&u64));
  EXPECT_TRUE(reader.Skip(4));
  // 3 left
  EXPECT_FALSE(reader.ReadU32(&u32));
  EXPECT_FALSE(reader.ReadPiece(&piece, 4));
  EXPECT_TRUE(reader.Skip(2));
  // 1 left
  EXPECT_FALSE(reader.ReadU16(&u16));
  EXPECT_FALSE(reader.ReadBytes(span<uint8_t>(buf)));
  EXPECT_FALSE(reader.ReadBytes(span<uint8_t, 2u>(buf)));
  EXPECT_TRUE(reader.Skip(1));
  // 0 left
  EXPECT_FALSE(reader.ReadU8(&u8));
  EXPECT_EQ(0u, reader.remaining());
}

TEST(BigEndianReaderTest, SafePointerMath) {
  uint8_t data[3];
  BigEndianReader reader(data);
  // The test should fail without ever dereferencing the |dummy_buf| pointer.
  uint8_t* dummy_buf = reinterpret_cast<uint8_t*>(0xdeadbeef);
  // Craft an extreme length value that would cause |reader.data() + len| to
  // overflow.
  size_t extreme_length = std::numeric_limits<size_t>::max() - 1;
  base::StringPiece piece;
  EXPECT_FALSE(reader.Skip(extreme_length));
  EXPECT_FALSE(reader.ReadBytes(
      // SAFETY: This will create Undefined Behaviour if the invalid length is
      // ever added to the pointer. However span() constructor does not do so,
      // and ReadBytes() checks the size before using the pointer and returns
      // false.
      UNSAFE_BUFFERS(span(dummy_buf, extreme_length))));
  EXPECT_FALSE(reader.ReadPiece(&piece, extreme_length));
}

TEST(BigEndianWriterTest, WritesValues) {
  uint8_t expected[] = {0,   0,   2,   3,   4,   5,   6,    7,    8,   9,
                        0xA, 0xB, 0xC, 0xD, 0xE, 0xF, 0x1A, 0x2B, 0x3C};
  uint8_t data[sizeof(expected)];
  uint8_t buf[] = {0x2, 0x3};
  memset(data, 0, sizeof(data));
  BigEndianWriter writer(data);

  EXPECT_TRUE(writer.Skip(2));
  EXPECT_TRUE(writer.WriteBytes(buf, sizeof(buf)));
  EXPECT_TRUE(writer.WriteU8(0x4));
  EXPECT_TRUE(writer.WriteU16(0x0506));
  EXPECT_TRUE(writer.WriteU32(0x0708090A));
  EXPECT_TRUE(writer.WriteU64(0x0B0C0D0E0F1A2B3Cllu));
  EXPECT_EQ(0, memcmp(expected, data, sizeof(expected)));
}

TEST(BigEndianWriterTest, RespectsLength) {
  uint8_t data[8];
  uint8_t buf[2];
  uint8_t u8 = 0;
  uint16_t u16 = 0;
  uint32_t u32 = 0;
  uint64_t u64 = 0;
  BigEndianWriter writer(data);
  // 8 left
  EXPECT_FALSE(writer.Skip(9));
  EXPECT_TRUE(writer.Skip(1));
  // 7 left
  EXPECT_FALSE(writer.WriteU64(u64));
  EXPECT_TRUE(writer.Skip(4));
  // 3 left
  EXPECT_FALSE(writer.WriteU32(u32));
  EXPECT_TRUE(writer.Skip(2));
  // 1 left
  EXPECT_FALSE(writer.WriteU16(u16));
  EXPECT_FALSE(writer.WriteBytes(buf, 2));
  EXPECT_TRUE(writer.Skip(1));
  // 0 left
  EXPECT_FALSE(writer.WriteU8(u8));
  EXPECT_EQ(0u, writer.remaining());
}

TEST(BigEndianWriterTest, SafePointerMath) {
  uint8_t data[3];
  BigEndianWriter writer(data);
  // The test should fail without ever dereferencing the |dummy_buf| pointer.
  const uint8_t* dummy_buf = reinterpret_cast<const uint8_t*>(0xdeadbeef);
  // Craft an extreme length value that would cause |reader.data() + len| to
  // overflow.
  size_t extreme_length = std::numeric_limits<size_t>::max() - 1;
  EXPECT_FALSE(writer.Skip(extreme_length));
  EXPECT_FALSE(writer.WriteBytes(dummy_buf, extreme_length));
}

}  // namespace base
