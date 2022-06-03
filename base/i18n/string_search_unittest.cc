// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <stddef.h>

#include <string>
#include <vector>

#include "base/cxx17_backports.h"
#include "base/i18n/rtl.h"
#include "base/i18n/string_search.h"
#include "base/strings/utf_string_conversions.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/icu/source/i18n/unicode/usearch.h"

namespace base {
namespace i18n {

#define EXPECT_MATCH_IGNORE_CASE(find_this, in_this, ex_start, ex_len)         \
  {                                                                            \
    size_t index = 0;                                                          \
    size_t length = 0;                                                         \
    EXPECT_TRUE(StringSearchIgnoringCaseAndAccents(find_this, in_this, &index, \
                                                   &length));                  \
    EXPECT_EQ(ex_start, index);                                                \
    EXPECT_EQ(ex_len, length);                                                 \
    index = 0;                                                                 \
    length = 0;                                                                \
    EXPECT_TRUE(                                                               \
        StringSearch(find_this, in_this, &index, &length, false, true));       \
    EXPECT_EQ(ex_start, index);                                                \
    EXPECT_EQ(ex_len, length);                                                 \
  }

#define EXPECT_MATCH_SENSITIVE(find_this, in_this, ex_start, ex_len)    \
  {                                                                     \
    size_t index = 0;                                                   \
    size_t length = 0;                                                  \
    EXPECT_TRUE(                                                        \
        StringSearch(find_this, in_this, &index, &length, true, true)); \
    EXPECT_EQ(ex_start, index);                                         \
    EXPECT_EQ(ex_len, length);                                          \
  }

#define EXPECT_MATCH_IGNORE_CASE_BACKWARDS(find_this, in_this, ex_start,  \
                                           ex_len)                        \
  {                                                                       \
    size_t index = 0;                                                     \
    size_t length = 0;                                                    \
    EXPECT_TRUE(                                                          \
        StringSearch(find_this, in_this, &index, &length, false, false)); \
    EXPECT_EQ(ex_start, index);                                           \
    EXPECT_EQ(ex_len, length);                                            \
  }

#define EXPECT_MATCH_SENSITIVE_BACKWARDS(find_this, in_this, ex_start, ex_len) \
  {                                                                            \
    size_t index = 0;                                                          \
    size_t length = 0;                                                         \
    EXPECT_TRUE(                                                               \
        StringSearch(find_this, in_this, &index, &length, true, false));       \
    EXPECT_EQ(ex_start, index);                                                \
    EXPECT_EQ(ex_len, length);                                                 \
  }

#define EXPECT_MISS_IGNORE_CASE(find_this, in_this)                      \
  {                                                                      \
    size_t index = 0;                                                    \
    size_t length = 0;                                                   \
    EXPECT_FALSE(StringSearchIgnoringCaseAndAccents(find_this, in_this,  \
                                                    &index, &length));   \
    index = 0;                                                           \
    length = 0;                                                          \
    EXPECT_FALSE(                                                        \
        StringSearch(find_this, in_this, &index, &length, false, true)); \
  }

#define EXPECT_MISS_SENSITIVE(find_this, in_this)                       \
  {                                                                     \
    size_t index = 0;                                                   \
    size_t length = 0;                                                  \
    EXPECT_FALSE(                                                       \
        StringSearch(find_this, in_this, &index, &length, true, true)); \
  }

#define EXPECT_MISS_IGNORE_CASE_BACKWARDS(find_this, in_this)             \
  {                                                                       \
    size_t index = 0;                                                     \
    size_t length = 0;                                                    \
    EXPECT_FALSE(                                                         \
        StringSearch(find_this, in_this, &index, &length, false, false)); \
  }

#define EXPECT_MISS_SENSITIVE_BACKWARDS(find_this, in_this)              \
  {                                                                      \
    size_t index = 0;                                                    \
    size_t length = 0;                                                   \
    EXPECT_FALSE(                                                        \
        StringSearch(find_this, in_this, &index, &length, true, false)); \
  }

// Note on setting default locale for testing: The current default locale on
// the Mac trybot is en_US_POSIX, with which primary-level collation strength
// string search is case-sensitive, when normally it should be
// case-insensitive. In other locales (including en_US which English speakers
// in the U.S. use), this search would be case-insensitive as expected.

TEST(StringSearchTest, ASCII) {
  std::string default_locale(uloc_getDefault());
  bool locale_is_posix = (default_locale == "en_US_POSIX");
  if (locale_is_posix)
    SetICUDefaultLocale("en_US");

  EXPECT_MATCH_IGNORE_CASE(u"hello", u"hello world", 0U, 5U);

  EXPECT_MISS_IGNORE_CASE(u"h    e l l o", u"h   e l l o");

  EXPECT_MATCH_IGNORE_CASE(u"aabaaa", u"aaabaabaaa", 4U, 6U);

  EXPECT_MISS_IGNORE_CASE(u"searching within empty string", std::u16string());

  EXPECT_MATCH_IGNORE_CASE(std::u16string(), u"searching for empty string", 0U,
                           0U);

  EXPECT_MATCH_IGNORE_CASE(u"case insensitivity", u"CaSe InSeNsItIvItY", 0U,
                           18U);

  EXPECT_MATCH_SENSITIVE(u"aabaaa", u"aaabaabaaa", 4U, 6U);

  EXPECT_MISS_SENSITIVE(u"searching within empty string", std::u16string());

  EXPECT_MATCH_SENSITIVE(std::u16string(), u"searching for empty string", 0U,
                         0U);

  EXPECT_MISS_SENSITIVE(u"case insensitivity", u"CaSe InSeNsItIvItY");

  if (locale_is_posix)
    SetICUDefaultLocale(default_locale.data());
}

TEST(StringSearchTest, UnicodeLocaleIndependent) {
  // Base characters
  const std::u16string e_base = u"e";
  const std::u16string E_base = u"E";
  const std::u16string a_base = u"a";

  // Composed characters
  const std::u16string e_with_acute_accent = u"\u00e9";
  const std::u16string E_with_acute_accent = u"\u00c9";
  const std::u16string e_with_grave_accent = u"\u00e8";
  const std::u16string E_with_grave_accent = u"\u00c8";
  const std::u16string a_with_acute_accent = u"\u00e1";

  // Decomposed characters
  const std::u16string e_with_acute_combining_mark = u"e\u0301";
  const std::u16string E_with_acute_combining_mark = u"E\u0301";
  const std::u16string e_with_grave_combining_mark = u"e\u0300";
  const std::u16string E_with_grave_combining_mark = u"E\u0300";
  const std::u16string a_with_acute_combining_mark = u"a\u0301";

  std::string default_locale(uloc_getDefault());
  bool locale_is_posix = (default_locale == "en_US_POSIX");
  if (locale_is_posix)
    SetICUDefaultLocale("en_US");

  EXPECT_MATCH_IGNORE_CASE(e_base, e_with_acute_accent, 0U,
                           e_with_acute_accent.size());

  EXPECT_MATCH_IGNORE_CASE(e_with_acute_accent, e_base, 0U, e_base.size());

  EXPECT_MATCH_IGNORE_CASE(e_base, e_with_acute_combining_mark, 0U,
                           e_with_acute_combining_mark.size());

  EXPECT_MATCH_IGNORE_CASE(e_with_acute_combining_mark, e_base, 0U,
                           e_base.size());

  EXPECT_MATCH_IGNORE_CASE(e_with_acute_combining_mark, e_with_acute_accent, 0U,
                           e_with_acute_accent.size());

  EXPECT_MATCH_IGNORE_CASE(e_with_acute_accent, e_with_acute_combining_mark, 0U,
                           e_with_acute_combining_mark.size());

  EXPECT_MATCH_IGNORE_CASE(e_with_acute_combining_mark,
                           e_with_grave_combining_mark, 0U,
                           e_with_grave_combining_mark.size());

  EXPECT_MATCH_IGNORE_CASE(e_with_grave_combining_mark,
                           e_with_acute_combining_mark, 0U,
                           e_with_acute_combining_mark.size());

  EXPECT_MATCH_IGNORE_CASE(e_with_acute_combining_mark, e_with_grave_accent, 0U,
                           e_with_grave_accent.size());

  EXPECT_MATCH_IGNORE_CASE(e_with_grave_accent, e_with_acute_combining_mark, 0U,
                           e_with_acute_combining_mark.size());

  EXPECT_MATCH_IGNORE_CASE(E_with_acute_accent, e_with_acute_accent, 0U,
                           e_with_acute_accent.size());

  EXPECT_MATCH_IGNORE_CASE(E_with_grave_accent, e_with_acute_accent, 0U,
                           e_with_acute_accent.size());

  EXPECT_MATCH_IGNORE_CASE(E_with_acute_combining_mark, e_with_grave_accent, 0U,
                           e_with_grave_accent.size());

  EXPECT_MATCH_IGNORE_CASE(E_with_grave_combining_mark, e_with_acute_accent, 0U,
                           e_with_acute_accent.size());

  EXPECT_MATCH_IGNORE_CASE(E_base, e_with_grave_accent, 0U,
                           e_with_grave_accent.size());

  EXPECT_MISS_IGNORE_CASE(a_with_acute_accent, e_with_acute_accent);

  EXPECT_MISS_IGNORE_CASE(a_with_acute_combining_mark,
                          e_with_acute_combining_mark);

  EXPECT_MISS_SENSITIVE(e_base, e_with_acute_accent);

  EXPECT_MISS_SENSITIVE(e_with_acute_accent, e_base);

  EXPECT_MISS_SENSITIVE(e_base, e_with_acute_combining_mark);

  EXPECT_MISS_SENSITIVE(e_with_acute_combining_mark, e_base);

  EXPECT_MATCH_SENSITIVE(e_with_acute_combining_mark, e_with_acute_accent, 0U,
                         1U);

  EXPECT_MATCH_SENSITIVE(e_with_acute_accent, e_with_acute_combining_mark, 0U,
                         2U);

  EXPECT_MISS_SENSITIVE(e_with_acute_combining_mark,
                        e_with_grave_combining_mark);

  EXPECT_MISS_SENSITIVE(e_with_grave_combining_mark,
                        e_with_acute_combining_mark);

  EXPECT_MISS_SENSITIVE(e_with_acute_combining_mark, e_with_grave_accent);

  EXPECT_MISS_SENSITIVE(e_with_grave_accent, e_with_acute_combining_mark);

  EXPECT_MISS_SENSITIVE(E_with_acute_accent, e_with_acute_accent);

  EXPECT_MISS_SENSITIVE(E_with_grave_accent, e_with_acute_accent);

  EXPECT_MISS_SENSITIVE(E_with_acute_combining_mark, e_with_grave_accent);

  EXPECT_MISS_SENSITIVE(E_with_grave_combining_mark, e_with_acute_accent);

  EXPECT_MISS_SENSITIVE(E_base, e_with_grave_accent);

  EXPECT_MISS_SENSITIVE(a_with_acute_accent, e_with_acute_accent);

  EXPECT_MISS_SENSITIVE(a_with_acute_combining_mark,
                        e_with_acute_combining_mark);

  EXPECT_MATCH_SENSITIVE(a_with_acute_combining_mark,
                         a_with_acute_combining_mark, 0U, 2U);

  if (locale_is_posix)
    SetICUDefaultLocale(default_locale.data());
}

TEST(StringSearchTest, UnicodeLocaleDependent) {
  // Base characters
  const std::u16string a_base = u"a";

  // Composed characters
  const std::u16string a_with_ring = u"\u00e5";

  EXPECT_TRUE(StringSearchIgnoringCaseAndAccents(a_base, a_with_ring, nullptr,
                                                 nullptr));
  EXPECT_TRUE(StringSearch(a_base, a_with_ring, nullptr, nullptr, false, true));

  const char* default_locale = uloc_getDefault();
  SetICUDefaultLocale("da");

  EXPECT_FALSE(StringSearchIgnoringCaseAndAccents(a_base, a_with_ring, nullptr,
                                                  nullptr));
  EXPECT_FALSE(
      StringSearch(a_base, a_with_ring, nullptr, nullptr, false, true));

  SetICUDefaultLocale(default_locale);
}

TEST(StringSearchTest, SearchBackwards) {
  std::string default_locale(uloc_getDefault());
  bool locale_is_posix = (default_locale == "en_US_POSIX");
  if (locale_is_posix)
    SetICUDefaultLocale("en_US");

  EXPECT_MATCH_IGNORE_CASE_BACKWARDS(u"ab", u"ABAB", 2U, 2U);
  EXPECT_MATCH_SENSITIVE_BACKWARDS(u"ab", u"abab", 2U, 2U);
  EXPECT_MISS_SENSITIVE_BACKWARDS(u"ab", u"ABAB");

  if (locale_is_posix)
    SetICUDefaultLocale(default_locale.data());
}

TEST(StringSearchTest, FixedPatternMultipleSearch) {
  std::string default_locale(uloc_getDefault());
  bool locale_is_posix = (default_locale == "en_US_POSIX");
  if (locale_is_posix)
    SetICUDefaultLocale("en_US");

  size_t index = 0;
  size_t length = 0;

  // Search "foo" over multiple texts.
  FixedPatternStringSearch query1(u"foo", true);
  EXPECT_TRUE(query1.Search(u"12foo34", &index, &length, true));
  EXPECT_EQ(2U, index);
  EXPECT_EQ(3U, length);
  EXPECT_FALSE(query1.Search(u"bye", &index, &length, true));
  EXPECT_FALSE(query1.Search(u"FOO", &index, &length, true));
  EXPECT_TRUE(query1.Search(u"foobarfoo", &index, &length, true));
  EXPECT_EQ(0U, index);
  EXPECT_EQ(3U, length);
  EXPECT_TRUE(query1.Search(u"foobarfoo", &index, &length, false));
  EXPECT_EQ(6U, index);
  EXPECT_EQ(3U, length);

  // Search "hello" over multiple texts.
  FixedPatternStringSearchIgnoringCaseAndAccents query2(u"hello");
  EXPECT_TRUE(query2.Search(u"12hello34", &index, &length));
  EXPECT_EQ(2U, index);
  EXPECT_EQ(5U, length);
  EXPECT_FALSE(query2.Search(u"bye", &index, &length));
  EXPECT_TRUE(query2.Search(u"hELLo", &index, &length));
  EXPECT_EQ(0U, index);
  EXPECT_EQ(5U, length);

  if (locale_is_posix)
    SetICUDefaultLocale(default_locale.data());
}

TEST(StringSearchTest, RepeatingStringSearch) {
  struct MatchResult {
    int match_index;
    int match_length;
  };

  std::string default_locale(uloc_getDefault());
  bool locale_is_posix = (default_locale == "en_US_POSIX");
  if (locale_is_posix)
    SetICUDefaultLocale("en_US");

  const char16_t kPattern[] = u"fox";
  const char16_t kTarget[] = u"The quick brown fox jumped over the lazy Fox";

  // Case sensitive.
  {
    const MatchResult kExpectation[] = {{16, 3}};

    RepeatingStringSearch searcher(kPattern, kTarget, /*case_sensitive=*/true);
    std::vector<MatchResult> results;
    int match_index;
    int match_length;
    while (searcher.NextMatchResult(match_index, match_length)) {
      results.push_back(
          {.match_index = match_index, .match_length = match_length});
    }

    ASSERT_EQ(base::size(kExpectation), results.size());
    for (size_t i = 0; i < results.size(); ++i) {
      EXPECT_EQ(results[i].match_index, kExpectation[i].match_index);
      EXPECT_EQ(results[i].match_length, kExpectation[i].match_length);
    }
  }

  // Case insensitive.
  {
    const MatchResult kExpectation[] = {{16, 3}, {41, 3}};

    RepeatingStringSearch searcher(kPattern, kTarget, /*case_sensitive=*/false);
    std::vector<MatchResult> results;
    int match_index;
    int match_length;
    while (searcher.NextMatchResult(match_index, match_length)) {
      results.push_back(
          {.match_index = match_index, .match_length = match_length});
    }

    ASSERT_EQ(base::size(kExpectation), results.size());
    for (size_t i = 0; i < results.size(); ++i) {
      EXPECT_EQ(results[i].match_index, kExpectation[i].match_index);
      EXPECT_EQ(results[i].match_length, kExpectation[i].match_length);
    }
  }

  if (locale_is_posix)
    SetICUDefaultLocale(default_locale.data());
}

}  // namespace i18n
}  // namespace base
