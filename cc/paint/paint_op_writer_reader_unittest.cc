// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/paint/paint_op_reader.h"
#include "cc/paint/paint_op_writer.h"

#include "cc/test/test_options_provider.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace cc {

TEST(PaintOpWriterReaderTest, SizeT) {
  static_assert(PaintOpWriter::SerializedSize<int>() == 4u);
  static_assert(PaintOpWriter::SerializedSize<size_t>() == 8u);
  static_assert(PaintOpWriter::SerializedSize(static_cast<size_t>(0u)) == 8u);

  char buffer[128];
  TestOptionsProvider options_provider;
  memset(buffer, 0xa5, std::size(buffer));
  PaintOpWriter writer(buffer, std::size(buffer),
                       options_provider.serialize_options(),
                       /*enable_security_constraints*/ true);
  int i = 0x5555aaaa;
  size_t s1 = static_cast<size_t>(0x123456789abcdef0L);
  size_t s2 = static_cast<size_t>(0xfedcba9876543210L);
  writer.WriteSize(s1);
  writer.Write(i);
  writer.WriteSize(s2);
  EXPECT_EQ(20u, writer.size());

  PaintOpReader reader(buffer, writer.size(),
                       options_provider.deserialize_options(),
                       /*enable_security_constraints*/ true);
  int read_i;
  size_t read_s1, read_s2;
  reader.ReadSize(&read_s1);
  reader.Read(&read_i);
  reader.ReadSize(&read_s2);
  EXPECT_EQ(i, read_i);
  EXPECT_EQ(s1, read_s1);
  EXPECT_EQ(s2, read_s2);
  EXPECT_TRUE(reader.valid());
  EXPECT_EQ(0u, reader.remaining_bytes());

  reader.ReadSize(&read_s2);
  EXPECT_FALSE(reader.valid());
}

}  // namespace cc
