// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/strings/pattern.h"
#include "base/strings/utf_string_conversions.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace base {

TEST(StringUtilTest, MatchPatternTest) {
  EXPECT_TRUE(MatchPattern("www.google.com", "*.com"));
  EXPECT_TRUE(MatchPattern("www.google.com", "*"));
  EXPECT_FALSE(MatchPattern("www.google.com", "www*.g*.org"));
  EXPECT_TRUE(MatchPattern("Hello", "H?l?o"));
  EXPECT_FALSE(MatchPattern("www.google.com", "http://*)"));
  EXPECT_FALSE(MatchPattern("www.msn.com", "*.COM"));
  EXPECT_TRUE(MatchPattern("Hello*1234", "He??o\\*1*"));
  EXPECT_FALSE(MatchPattern("", "*.*"));
  EXPECT_TRUE(MatchPattern("", "*"));
  EXPECT_TRUE(MatchPattern("", "?"));
  EXPECT_TRUE(MatchPattern("", ""));
  EXPECT_FALSE(MatchPattern("Hello", ""));
  EXPECT_TRUE(MatchPattern("Hello*", "Hello*"));
  EXPECT_TRUE(MatchPattern("abcd", "*???"));
  EXPECT_FALSE(MatchPattern("abcd", "???"));
  EXPECT_TRUE(MatchPattern("abcb", "a*b"));
  EXPECT_FALSE(MatchPattern("abcb", "a?b"));

  // Test UTF8 matching.
  EXPECT_TRUE(MatchPattern("heart: \xe2\x99\xa0", "*\xe2\x99\xa0"));
  EXPECT_TRUE(MatchPattern("heart: \xe2\x99\xa0.", "heart: ?."));
  EXPECT_TRUE(MatchPattern("hearts: \xe2\x99\xa0\xe2\x99\xa0", "*"));
  // Invalid sequences should be handled as a single invalid character.
  EXPECT_TRUE(MatchPattern("invalid: \xef\xbf\xbe", "invalid: ?"));
  // If the pattern has invalid characters, it shouldn't match anything.
  EXPECT_FALSE(MatchPattern("\xf4\x90\x80\x80", "\xf4\x90\x80\x80"));

  // Test UTF16 character matching.
  EXPECT_TRUE(MatchPattern(u"www.google.com", u"*.com"));
  EXPECT_TRUE(MatchPattern(u"Hello*1234", u"He??o\\*1*"));

  // Some test cases that might cause naive implementations to exhibit
  // exponential run time or fail.
  EXPECT_TRUE(MatchPattern("Hello", "He********************************o"));
  EXPECT_TRUE(MatchPattern("123456789012345678", "?????????????????*"));
  EXPECT_TRUE(MatchPattern("aaaaaaaaaaab", "a*a*a*a*a*a*a*a*a*a*a*b"));
}

}  // namespace base
