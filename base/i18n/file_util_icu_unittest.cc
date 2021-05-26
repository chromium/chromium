// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/i18n/file_util_icu.h"

#include <stddef.h>

#include "base/files/file_util.h"
#include "base/logging.h"
#include "base/strings/utf_string_conversions.h"
#include "build/build_config.h"
#include "build/chromeos_buildflags.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "testing/platform_test.h"

namespace base {
namespace i18n {

// file_util winds up using autoreleased objects on the Mac, so this needs
// to be a PlatformTest
class FileUtilICUTest : public PlatformTest {
};

#if defined(OS_POSIX) && !defined(OS_APPLE)

// On linux, file path is parsed and filtered as UTF-8.
static const struct GoodBadPairLinux {
  const char* bad_name;
  const char* good_name;
} kLinuxIllegalCharacterCases[] = {
  {"bad*\\/file:name?.jpg", "bad---file-name-.jpg"},
  {"**********::::.txt", "--------------.txt"},
  {"\xe9\xf0zzzz.\xff", "\xe9\xf0zzzz.\xff"},
  {" _ ", "-_-"},
  {".", "-"},
  {" .( ). ", "-.( ).-"},
  {"     ", "-   -"},
};

TEST_F(FileUtilICUTest, ReplaceIllegalCharactersInPathLinuxTest) {
  for (auto i : kLinuxIllegalCharacterCases) {
    std::string bad_name(i.bad_name);
    ReplaceIllegalCharactersInPath(&bad_name, '-');
    EXPECT_EQ(i.good_name, bad_name);
  }
}

#endif

// For Mac & Windows, which both do Unicode validation on filenames. These
// characters are given as UTF-16 strings since its more convenient to specify
// unicode characters. For Mac they should be converted to UTF-8, for Windows to
// wide.
static const struct FileUtilICUTestCases {
  const char16_t* bad_name;
  const char16_t* good_name_with_dash;
  const char16_t* good_name_with_space;
} kIllegalCharacterCases[] = {
    {u"bad*file:name?.jpg", u"bad-file-name-.jpg", u"bad file name .jpg"},
    {u"**********::::.txt", u"--------------.txt", u"_.txt"},
    // We can't use UCNs (universal character names) for C0/C1 characters and
    // U+007F, but \x escape is interpreted by MSVC and gcc as we intend.
    {u"bad\x0003\x0091 file\u200E\u200Fname.png", u"bad-- file--name.png",
     u"bad   file  name.png"},
    {u"bad*file\\?name.jpg", u"bad-file--name.jpg", u"bad file  name.jpg"},
    {u"\t  bad*file\\name/.jpg", u"-  bad-file-name-.jpg",
     u"bad file name .jpg"},
    {u"this_file_name is okay!.mp3", u"this_file_name is okay!.mp3",
     u"this_file_name is okay!.mp3"},
    {u"\u4E00\uAC00.mp3", u"\u4E00\uAC00.mp3", u"\u4E00\uAC00.mp3"},
    {u"\u0635\u200C\u0644.mp3", u"\u0635-\u0644.mp3", u"\u0635 \u0644.mp3"},
    {u"\U00010330\U00010331.mp3", u"\U00010330\U00010331.mp3",
     u"\U00010330\U00010331.mp3"},
    // Unassigned codepoints are ok.
    {u"\u0378\U00040001.mp3", u"\u0378\U00040001.mp3", u"\u0378\U00040001.mp3"},
    // Non-characters are not allowed.
    {u"bad\uFFFFfile\U0010FFFEname.jpg", u"bad-file-name.jpg",
     u"bad file name.jpg"},
    {u"bad\uFDD0file\uFDEFname.jpg", u"bad-file-name.jpg",
     u"bad file name.jpg"},
    // CVE-2014-9390
    {u"(\u200C.\u200D.\u200E.\u200F.\u202A.\u202B.\u202C.\u202D.\u202E.\u206A."
     u"\u206B.\u206C.\u206D.\u206F.\uFEFF)",
     u"(-.-.-.-.-.-.-.-.-.-.-.-.-.-.-)", u"( . . . . . . . . . . . . . . )"},
    {u"config~1", u"config-1", u"config 1"},
    {u" _ ", u"-_-", u"_"},
    {u" ", u"-", u"_ _"},
    {u"\u2008.(\u2007).\u3000", u"-.(\u2007).-", u"(\u2007)"},
    {u"     ", u"-   -", u"_     _"},
    {u".    ", u"-   -", u"_.    _"}};

#if defined(OS_WIN) || defined(OS_APPLE) || defined(OS_POSIX)

TEST_F(FileUtilICUTest, ReplaceIllegalCharactersInPathTest) {
  for (auto i : kIllegalCharacterCases) {
#if defined(OS_WIN)
    std::wstring bad_name = UTF16ToWide(i.bad_name);
    ReplaceIllegalCharactersInPath(&bad_name, '-');
    EXPECT_EQ(UTF16ToWide(i.good_name_with_dash), bad_name);
#else
    std::string bad_name = UTF16ToUTF8(i.bad_name);
    ReplaceIllegalCharactersInPath(&bad_name, '-');
    EXPECT_EQ(UTF16ToUTF8(i.good_name_with_dash), bad_name);
#endif
  }
}

TEST_F(FileUtilICUTest, ReplaceIllegalCharactersInPathWithIllegalEndCharTest) {
  for (auto i : kIllegalCharacterCases) {
#if defined(OS_WIN)
    std::wstring bad_name = UTF16ToWide(i.bad_name);
    ReplaceIllegalCharactersInPath(&bad_name, ' ');
    EXPECT_EQ(UTF16ToWide(i.good_name_with_space), bad_name);
#else
    std::string bad_name(UTF16ToUTF8(i.bad_name));
    ReplaceIllegalCharactersInPath(&bad_name, ' ');
    EXPECT_EQ(UTF16ToUTF8(i.good_name_with_space), bad_name);
#endif
  }
}

#endif

TEST_F(FileUtilICUTest, IsFilenameLegalTest) {
  EXPECT_TRUE(IsFilenameLegal(std::u16string()));

  for (const auto& test_case : kIllegalCharacterCases) {
    std::u16string bad_name = test_case.bad_name;
    std::u16string good_name = test_case.good_name_with_dash;

    EXPECT_TRUE(IsFilenameLegal(good_name)) << good_name;
    if (good_name != bad_name)
      EXPECT_FALSE(IsFilenameLegal(bad_name)) << bad_name;
  }
}

#if BUILDFLAG(IS_CHROMEOS_ASH)
static const struct normalize_name_encoding_test_cases {
  const char* original_path;
  const char* normalized_path;
} kNormalizeFileNameEncodingTestCases[] = {
  { "foo_na\xcc\x88me.foo", "foo_n\xc3\xa4me.foo"},
  { "foo_dir_na\xcc\x88me/foo_na\xcc\x88me.foo",
    "foo_dir_na\xcc\x88me/foo_n\xc3\xa4me.foo"},
  { "", ""},
  { "foo_dir_na\xcc\x88me/", "foo_dir_n\xc3\xa4me"}
};

TEST_F(FileUtilICUTest, NormalizeFileNameEncoding) {
  for (size_t i = 0; i < size(kNormalizeFileNameEncodingTestCases); i++) {
    FilePath path(kNormalizeFileNameEncodingTestCases[i].original_path);
    NormalizeFileNameEncoding(&path);
    EXPECT_EQ(FilePath(kNormalizeFileNameEncodingTestCases[i].normalized_path),
              path);
  }
}

#endif

}  // namespace i18n
}  // namespace base
