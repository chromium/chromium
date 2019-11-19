// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/big_endian.h"

#include <stdint.h>

#include "base/strings/string_piece.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace base {

TEST(BigEndianReaderTest, ReadsValues) {
  char data[] = { 0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 0xA, 0xB, 0xC, 0xD, 0xE, 0xF,
                  0x1A, 0x2B, 0x3C, 0x4D, 0x5E };
  char buf[2];
  uint8_t u8;
  uint16_t u16;
  uint32_t u32;
  uint64_t u64;
  base::StringPiece piece;
  BigEndianReader reader(data, sizeof(data));

  EXPECT_TRUE(reader.Skip(2));
  EXPECT_EQ(data + 2, reader.ptr());
  EXPECT_EQ(reader.remaining(), sizeof(data) - 2);
  EXPECT_TRUE(reader.ReadBytes(buf, sizeof(buf)));
  EXPECT_EQ(0x2, buf[0]);
  EXPECT_EQ(0x3, buf[1]);
  EXPECT_TRUE(reader.ReadU8(&u8));
  EXPECT_EQ(0x4, u8);
  EXPECT_TRUE(reader.ReadU16(&u16));
  EXPECT_EQ(0x0506, u16);
  EXPECT_TRUE(reader.ReadU32(&u32));
  EXPECT_EQ(0x0708090Au, u32);
  EXPECT_TRUE(reader.ReadU64(&u64));
  EXPECT_EQ(0x0B0C0D0E0F1A2B3Cllu, u64);
  base::StringPiece expected(reader.ptr(), 2);
  EXPECT_TRUE(reader.ReadPiece(&piece, 2));
  EXPECT_EQ(2u, piece.size());
  EXPECT_EQ(expected.data(), piece.data());
}

TEST(BigEndianReaderTest, ReadsLengthPrefixedValues) {
  {
    char u8_prefixed_data[] = {8,   8,   9,    0xA,  0xB,  0xC,  0xD,
                               0xE, 0xF, 0x1A, 0x2B, 0x3C, 0x4D, 0x5E};
    BigEndianReader reader(u8_prefixed_data, sizeof(u8_prefixed_data));

    base::StringPiece piece;
    ASSERT_TRUE(reader.ReadU8LengthPrefixed(&piece));
    // |reader| should skip both a u8 and the length-8 length-prefixed field.
    EXPECT_EQ(reader.ptr(), u8_prefixed_data + 9);
    EXPECT_EQ(piece.size(), 8u);
    EXPECT_EQ(piece.data(), u8_prefixed_data + 1);
  }

  {
    char u16_prefixed_data[] = {0,    8,    0xD,  0xE,  0xF,
                                0x1A, 0x2B, 0x3C, 0x4D, 0x5E};
    BigEndianReader reader(u16_prefixed_data, sizeof(u16_prefixed_data));
    base::StringPiece piece;
    ASSERT_TRUE(reader.ReadU16LengthPrefixed(&piece));
    // |reader| should skip both a u16 and the length-8 length-prefixed field.
    EXPECT_EQ(reader.ptr(), u16_prefixed_data + 10);
    EXPECT_EQ(piece.size(), 8u);
    EXPECT_EQ(piece.data(), u16_prefixed_data + 2);

    // With no data left, we shouldn't be able to
    // read another u8 length prefix (or a u16 length prefix,
    // for that matter).
    EXPECT_FALSE(reader.ReadU8LengthPrefixed(&piece));
    EXPECT_FALSE(reader.ReadU16LengthPrefixed(&piece));
  }

  {
    // Make sure there's no issue reading a zero-value length prefix.
    char u16_prefixed_data[3] = {};
    BigEndianReader reader(u16_prefixed_data, sizeof(u16_prefixed_data));
    base::StringPiece piece;
    ASSERT_TRUE(reader.ReadU16LengthPrefixed(&piece));
    EXPECT_EQ(reader.ptr(), u16_prefixed_data + 2);
    EXPECT_EQ(piece.data(), u16_prefixed_data + 2);
    EXPECT_EQ(piece.size(), 0u);
  }
}

TEST(BigEndianReaderTest, LengthPrefixedReadsFailGracefully) {
  // We can't read 0xF (or, for that matter, 0xF8) bytes after the length
  // prefix: there isn't enough data.
  char data[] = {0xF, 8,   9,    0xA,  0xB,  0xC,  0xD,
                 0xE, 0xF, 0x1A, 0x2B, 0x3C, 0x4D, 0x5E};
  BigEndianReader reader(data, sizeof(data));
  base::StringPiece piece;
  EXPECT_FALSE(reader.ReadU8LengthPrefixed(&piece));
  EXPECT_EQ(data, reader.ptr());

  EXPECT_FALSE(reader.ReadU16LengthPrefixed(&piece));
  EXPECT_EQ(data, reader.ptr());
}

TEST(BigEndianReaderTest, RespectsLength) {
  char data[8];
  char buf[2];
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
  EXPECT_FALSE(reader.ReadBytes(buf, 2));
  EXPECT_TRUE(reader.Skip(1));
  // 0 left
  EXPECT_FALSE(reader.ReadU8(&u8));
  EXPECT_EQ(0u, reader.remaining());
}

TEST(BigEndianWriterTest, WritesValues) {
  char expected[] = { 0, 0, 2, 3, 4, 5, 6, 7, 8, 9, 0xA, 0xB, 0xC, 0xD, 0xE,
                      0xF, 0x1A, 0x2B, 0x3C };
  char data[sizeof(expected)];
  char buf[] = { 0x2, 0x3 };
  memset(data, 0, sizeof(data));
  BigEndianWriter writer(data, sizeof(data));

  EXPECT_TRUE(writer.Skip(2));
  EXPECT_TRUE(writer.WriteBytes(buf, sizeof(buf)));
  EXPECT_TRUE(writer.WriteU8(0x4));
  EXPECT_TRUE(writer.WriteU16(0x0506));
  EXPECT_TRUE(writer.WriteU32(0x0708090A));
  EXPECT_TRUE(writer.WriteU64(0x0B0C0D0E0F1A2B3Cllu));
  EXPECT_EQ(0, memcmp(expected, data, sizeof(expected)));
}

TEST(BigEndianWriterTest, RespectsLength) {
  char data[8];
  char buf[2];
  uint8_t u8 = 0;
  uint16_t u16 = 0;
  uint32_t u32 = 0;
  uint64_t u64 = 0;
  BigEndianWriter writer(data, sizeof(data));
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

}  // namespace base
