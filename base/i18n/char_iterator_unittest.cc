// Copyright 2011 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/i18n/char_iterator.h"

#include "base/strings/utf_string_conversions.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace base {
namespace i18n {

// This test string contains 4 characters:
//   x
//   u with circumflex - 2 bytes in UTF8, 1 codeword in UTF16
//   math double-struck A - 4 bytes in UTF8, 2 codewords in UTF16
//   z
static const char* const kTestString = "x\u00FB\U0001D538z";

TEST(CharIteratorsTest, TestUTF8) {
  std::string empty;
  UTF8CharIterator empty_iter(empty);
  EXPECT_TRUE(empty_iter.end());
  EXPECT_EQ(0u, empty_iter.array_pos());
  EXPECT_EQ(0u, empty_iter.char_pos());
  EXPECT_FALSE(empty_iter.Advance());

  std::string str("s\303\273r");  // [u with circumflex]
  UTF8CharIterator iter(str);
  EXPECT_FALSE(iter.end());
  EXPECT_EQ(0u, iter.array_pos());
  EXPECT_EQ(0u, iter.char_pos());
  EXPECT_EQ('s', iter.get());
  EXPECT_TRUE(iter.Advance());

  EXPECT_FALSE(iter.end());
  EXPECT_EQ(1u, iter.array_pos());
  EXPECT_EQ(1u, iter.char_pos());
  EXPECT_EQ(251, iter.get());
  EXPECT_TRUE(iter.Advance());

  EXPECT_FALSE(iter.end());
  EXPECT_EQ(3u, iter.array_pos());
  EXPECT_EQ(2u, iter.char_pos());
  EXPECT_EQ('r', iter.get());
  EXPECT_TRUE(iter.Advance());

  EXPECT_TRUE(iter.end());
  EXPECT_EQ(4u, iter.array_pos());
  EXPECT_EQ(3u, iter.char_pos());

  // Don't care what it returns, but this shouldn't crash
  iter.get();

  EXPECT_FALSE(iter.Advance());
}

TEST(CharIteratorsTest, TestUTF16_Empty) {
  std::u16string empty;
  UTF16CharIterator empty_iter(empty);
  EXPECT_TRUE(empty_iter.end());
  EXPECT_TRUE(empty_iter.start());
  EXPECT_EQ(0u, empty_iter.array_pos());
  EXPECT_EQ(0, empty_iter.char_offset());
  EXPECT_FALSE(empty_iter.Advance());

  // These shouldn't crash.
  empty_iter.get();
  empty_iter.NextCodePoint();
  empty_iter.PreviousCodePoint();
}

TEST(CharIteratorsTest, TestUTF16) {
  std::u16string str = UTF8ToUTF16(kTestString);
  UTF16CharIterator iter(str);
  EXPECT_FALSE(iter.end());
  EXPECT_TRUE(iter.start());
  EXPECT_EQ(0u, iter.array_pos());
  EXPECT_EQ(0, iter.char_offset());
  EXPECT_EQ('x', iter.get());
  // This shouldn't crash.
  iter.PreviousCodePoint();
  EXPECT_EQ(0xFB, iter.NextCodePoint());
  EXPECT_TRUE(iter.Advance());

  EXPECT_FALSE(iter.end());
  EXPECT_FALSE(iter.start());
  EXPECT_EQ(1u, iter.array_pos());
  EXPECT_EQ(1, iter.char_offset());
  EXPECT_EQ(0xFB, iter.get());
  EXPECT_EQ('x', iter.PreviousCodePoint());
  EXPECT_EQ(0x1D538, iter.NextCodePoint());
  EXPECT_TRUE(iter.Advance());

  EXPECT_FALSE(iter.end());
  EXPECT_FALSE(iter.start());
  EXPECT_EQ(2u, iter.array_pos());
  EXPECT_EQ(2, iter.char_offset());
  EXPECT_EQ(0x1D538, iter.get());
  EXPECT_EQ(0xFB, iter.PreviousCodePoint());
  EXPECT_EQ('z', iter.NextCodePoint());
  EXPECT_TRUE(iter.Advance());

  EXPECT_FALSE(iter.end());
  EXPECT_FALSE(iter.start());
  EXPECT_EQ(4u, iter.array_pos());
  EXPECT_EQ(3, iter.char_offset());
  EXPECT_EQ('z', iter.get());
  EXPECT_EQ(0x1D538, iter.PreviousCodePoint());
  // This shouldn't crash.
  iter.NextCodePoint();
  EXPECT_TRUE(iter.Advance());

  EXPECT_TRUE(iter.end());
  EXPECT_FALSE(iter.start());
  EXPECT_EQ(5u, iter.array_pos());
  EXPECT_EQ(4, iter.char_offset());
  EXPECT_EQ('z', iter.PreviousCodePoint());

  // Don't care what it returns, but these shouldn't crash
  iter.get();
  iter.NextCodePoint();

  EXPECT_FALSE(iter.Advance());
}

TEST(CharIteratorsTest, TestUTF16_Rewind) {
  std::u16string str = UTF8ToUTF16(kTestString);

  // It is valid for the starting array index to be on the terminating null
  // character; in fact, this is where end() reports true. So we'll start on the
  // terminator for this test so we can check the behavior of end().
  UTF16CharIterator iter = UTF16CharIterator::UpperBound(str, str.length());
  EXPECT_TRUE(iter.end());
  EXPECT_FALSE(iter.start());
  // This is the index of the terminating null character, and the length of the
  // string in char16s.
  EXPECT_EQ(5u, iter.array_pos());
  EXPECT_EQ(0, iter.char_offset());
  EXPECT_EQ('z', iter.PreviousCodePoint());
  // Don't care what it returns, but these shouldn't crash
  iter.get();
  iter.NextCodePoint();
  EXPECT_TRUE(iter.Rewind());

  EXPECT_FALSE(iter.end());
  EXPECT_FALSE(iter.start());
  EXPECT_EQ(4u, iter.array_pos());
  EXPECT_EQ(-1, iter.char_offset());
  EXPECT_EQ('z', iter.get());
  EXPECT_EQ(0x1D538, iter.PreviousCodePoint());
  // This shouldn't crash.
  iter.NextCodePoint();
  EXPECT_TRUE(iter.Rewind());

  EXPECT_FALSE(iter.end());
  EXPECT_FALSE(iter.start());
  EXPECT_EQ(2u, iter.array_pos());
  EXPECT_EQ(-2, iter.char_offset());
  EXPECT_EQ(0x1D538, iter.get());
  EXPECT_EQ(0xFB, iter.PreviousCodePoint());
  EXPECT_EQ('z', iter.NextCodePoint());
  EXPECT_TRUE(iter.Rewind());

  EXPECT_FALSE(iter.end());
  EXPECT_FALSE(iter.start());
  EXPECT_EQ(1u, iter.array_pos());
  EXPECT_EQ(-3, iter.char_offset());
  EXPECT_EQ(0xFB, iter.get());
  EXPECT_EQ('x', iter.PreviousCodePoint());
  EXPECT_EQ(0x1D538, iter.NextCodePoint());
  EXPECT_TRUE(iter.Rewind());

  EXPECT_FALSE(iter.end());
  EXPECT_TRUE(iter.start());
  EXPECT_EQ(0u, iter.array_pos());
  EXPECT_EQ(-4, iter.char_offset());
  EXPECT_EQ('x', iter.get());
  EXPECT_EQ(0xFB, iter.NextCodePoint());
  // This shouldn't crash.
  iter.PreviousCodePoint();

  EXPECT_FALSE(iter.Rewind());
}

TEST(CharIteratorsTest, TestUTF16_UpperBound) {
  std::u16string str = UTF8ToUTF16(kTestString);
  ASSERT_EQ(0u, UTF16CharIterator::UpperBound(str, 0).array_pos());
  ASSERT_EQ(1u, UTF16CharIterator::UpperBound(str, 1).array_pos());
  ASSERT_EQ(2u, UTF16CharIterator::UpperBound(str, 2).array_pos());
  ASSERT_EQ(4u, UTF16CharIterator::UpperBound(str, 3).array_pos());
  ASSERT_EQ(4u, UTF16CharIterator::UpperBound(str, 4).array_pos());
  ASSERT_EQ(5u, UTF16CharIterator::UpperBound(str, 5).array_pos());
}

TEST(CharIteratorsTest, TestUTF16_LowerBound) {
  std::u16string str = UTF8ToUTF16(kTestString);
  ASSERT_EQ(0u, UTF16CharIterator::LowerBound(str, 0).array_pos());
  ASSERT_EQ(1u, UTF16CharIterator::LowerBound(str, 1).array_pos());
  ASSERT_EQ(2u, UTF16CharIterator::LowerBound(str, 2).array_pos());
  ASSERT_EQ(2u, UTF16CharIterator::LowerBound(str, 3).array_pos());
  ASSERT_EQ(4u, UTF16CharIterator::LowerBound(str, 4).array_pos());
  ASSERT_EQ(5u, UTF16CharIterator::LowerBound(str, 5).array_pos());
}

}  // namespace i18n
}  // namespace base
