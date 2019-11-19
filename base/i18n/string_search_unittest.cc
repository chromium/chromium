// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <stddef.h>

#include <string>

#include "base/i18n/rtl.h"
#include "base/i18n/string_search.h"
#include "base/strings/string16.h"
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

  EXPECT_MATCH_IGNORE_CASE(ASCIIToUTF16("hello"), ASCIIToUTF16("hello world"),
                           0U, 5U);

  EXPECT_MISS_IGNORE_CASE(ASCIIToUTF16("h    e l l o"),
                          ASCIIToUTF16("h   e l l o"));

  EXPECT_MATCH_IGNORE_CASE(ASCIIToUTF16("aabaaa"), ASCIIToUTF16("aaabaabaaa"),
                           4U, 6U);

  EXPECT_MISS_IGNORE_CASE(ASCIIToUTF16("searching within empty string"),
                          string16());

  EXPECT_MATCH_IGNORE_CASE(string16(),
                           ASCIIToUTF16("searching for empty string"), 0U, 0U);

  EXPECT_MATCH_IGNORE_CASE(ASCIIToUTF16("case insensitivity"),
                           ASCIIToUTF16("CaSe InSeNsItIvItY"), 0U, 18U);

  EXPECT_MATCH_SENSITIVE(ASCIIToUTF16("aabaaa"), ASCIIToUTF16("aaabaabaaa"), 4U,
                         6U);

  EXPECT_MISS_SENSITIVE(ASCIIToUTF16("searching within empty string"),
                        string16());

  EXPECT_MATCH_SENSITIVE(string16(), ASCIIToUTF16("searching for empty string"),
                         0U, 0U);

  EXPECT_MISS_SENSITIVE(ASCIIToUTF16("case insensitivity"),
                        ASCIIToUTF16("CaSe InSeNsItIvItY"));

  if (locale_is_posix)
    SetICUDefaultLocale(default_locale.data());
}

TEST(StringSearchTest, UnicodeLocaleIndependent) {
  // Base characters
  const string16 e_base = WideToUTF16(L"e");
  const string16 E_base = WideToUTF16(L"E");
  const string16 a_base = WideToUTF16(L"a");

  // Composed characters
  const string16 e_with_acute_accent = WideToUTF16(L"\u00e9");
  const string16 E_with_acute_accent = WideToUTF16(L"\u00c9");
  const string16 e_with_grave_accent = WideToUTF16(L"\u00e8");
  const string16 E_with_grave_accent = WideToUTF16(L"\u00c8");
  const string16 a_with_acute_accent = WideToUTF16(L"\u00e1");

  // Decomposed characters
  const string16 e_with_acute_combining_mark = WideToUTF16(L"e\u0301");
  const string16 E_with_acute_combining_mark = WideToUTF16(L"E\u0301");
  const string16 e_with_grave_combining_mark = WideToUTF16(L"e\u0300");
  const string16 E_with_grave_combining_mark = WideToUTF16(L"E\u0300");
  const string16 a_with_acute_combining_mark = WideToUTF16(L"a\u0301");

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
  const string16 a_base = WideToUTF16(L"a");

  // Composed characters
  const string16 a_with_ring = WideToUTF16(L"\u00e5");

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

  EXPECT_MATCH_IGNORE_CASE_BACKWARDS(ASCIIToUTF16("ab"), ASCIIToUTF16("ABAB"),
                                     2U, 2U);
  EXPECT_MATCH_SENSITIVE_BACKWARDS(ASCIIToUTF16("ab"), ASCIIToUTF16("abab"), 2U,
                                   2U);
  EXPECT_MISS_SENSITIVE_BACKWARDS(ASCIIToUTF16("ab"), ASCIIToUTF16("ABAB"));

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
  FixedPatternStringSearch query1(ASCIIToUTF16("foo"), true);
  EXPECT_TRUE(query1.Search(ASCIIToUTF16("12foo34"), &index, &length, true));
  EXPECT_EQ(2U, index);
  EXPECT_EQ(3U, length);
  EXPECT_FALSE(query1.Search(ASCIIToUTF16("bye"), &index, &length, true));
  EXPECT_FALSE(query1.Search(ASCIIToUTF16("FOO"), &index, &length, true));
  EXPECT_TRUE(query1.Search(ASCIIToUTF16("foobarfoo"), &index, &length, true));
  EXPECT_EQ(0U, index);
  EXPECT_EQ(3U, length);
  EXPECT_TRUE(query1.Search(ASCIIToUTF16("foobarfoo"), &index, &length, false));
  EXPECT_EQ(6U, index);
  EXPECT_EQ(3U, length);

  // Search "hello" over multiple texts.
  FixedPatternStringSearchIgnoringCaseAndAccents query2(ASCIIToUTF16("hello"));
  EXPECT_TRUE(query2.Search(ASCIIToUTF16("12hello34"), &index, &length));
  EXPECT_EQ(2U, index);
  EXPECT_EQ(5U, length);
  EXPECT_FALSE(query2.Search(ASCIIToUTF16("bye"), &index, &length));
  EXPECT_TRUE(query2.Search(ASCIIToUTF16("hELLo"), &index, &length));
  EXPECT_EQ(0U, index);
  EXPECT_EQ(5U, length);

  if (locale_is_posix)
    SetICUDefaultLocale(default_locale.data());
}

}  // namespace i18n
}  // namespace base
