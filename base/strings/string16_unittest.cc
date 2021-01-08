// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <sstream>
#include <unordered_set>

#include "base/strings/string16.h"

#include "base/strings/utf_string_conversions.h"
#include "build/build_config.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace base {

TEST(String16Test, String16Literal) {
  static constexpr char16 kHelloWorld[] = STRING16_LITERAL("Hello, World");
  constexpr StringPiece16 kPiece = kHelloWorld;
  static_assert(kHelloWorld == kPiece, "");
  static_assert(kHelloWorld == kPiece.data(), "");

  string16 hello_world = kHelloWorld;
  EXPECT_EQ(kHelloWorld, hello_world);
}

// We define a custom operator<< for string16 so we can use it with logging.
// This tests that conversion.
TEST(String16Test, OutputStream) {
  // Basic stream test.
  {
    std::ostringstream stream;
    stream << "Empty '" << string16() << "' standard '"
           << string16(ASCIIToUTF16("Hello, world")) << "'";
    EXPECT_STREQ("Empty '' standard 'Hello, world'",
                 stream.str().c_str());
  }

  // Interesting edge cases.
  {
    // These should each get converted to the invalid character: EF BF BD.
    string16 initial_surrogate;
    initial_surrogate.push_back(0xd800);
    string16 final_surrogate;
    final_surrogate.push_back(0xdc00);

    // Old italic A = U+10300, will get converted to: F0 90 8C 80 'z'.
    string16 surrogate_pair;
    surrogate_pair.push_back(0xd800);
    surrogate_pair.push_back(0xdf00);
    surrogate_pair.push_back('z');

    // Will get converted to the invalid char + 's': EF BF BD 's'.
    string16 unterminated_surrogate;
    unterminated_surrogate.push_back(0xd800);
    unterminated_surrogate.push_back('s');

    std::ostringstream stream;
    stream << initial_surrogate << "," << final_surrogate << ","
           << surrogate_pair << "," << unterminated_surrogate;

    EXPECT_STREQ("\xef\xbf\xbd,\xef\xbf\xbd,\xf0\x90\x8c\x80z,\xef\xbf\xbds",
                 stream.str().c_str());
  }
}

TEST(String16Test, Hash) {
  string16 str1 = ASCIIToUTF16("hello");
  string16 str2 = ASCIIToUTF16("world");

  std::unordered_set<string16> set;

  set.insert(str1);
  EXPECT_EQ(1u, set.count(str1));
  EXPECT_EQ(0u, set.count(str2));
}

}  // namespace base
