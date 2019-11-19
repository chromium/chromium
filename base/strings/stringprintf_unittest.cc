// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/strings/stringprintf.h"

#include <errno.h>
#include <stddef.h>

#include "base/macros.h"
#include "build/build_config.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace base {

namespace {

// A helper for the StringAppendV test that follows.
//
// Just forwards its args to StringAppendV.
template <class CharT>
static void StringAppendVTestHelper(std::basic_string<CharT>* out,
                                    const CharT* format,
                                    ...) {
  va_list ap;
  va_start(ap, format);
  StringAppendV(out, format, ap);
  va_end(ap);
}

}  // namespace

TEST(StringPrintfTest, StringPrintfEmpty) {
  EXPECT_EQ("", StringPrintf("%s", ""));
}

TEST(StringPrintfTest, StringPrintfMisc) {
  EXPECT_EQ("123hello w", StringPrintf("%3d%2s %1c", 123, "hello", 'w'));
#if defined(OS_WIN)
  EXPECT_EQ(L"123hello w", StringPrintf(L"%3d%2ls %1lc", 123, L"hello", 'w'));
  EXPECT_EQ(u"123hello w", StringPrintf(u"%3d%2ls %1lc", 123, u"hello", 'w'));
#endif
}

TEST(StringPrintfTest, StringAppendfEmptyString) {
  std::string value("Hello");
  StringAppendF(&value, "%s", "");
  EXPECT_EQ("Hello", value);

#if defined(OS_WIN)
  std::wstring valuew(L"Hello");
  StringAppendF(&valuew, L"%ls", L"");
  EXPECT_EQ(L"Hello", valuew);

  std::u16string value16(u"Hello");
  StringAppendF(&value16, u"%ls", u"");
  EXPECT_EQ(u"Hello", value16);
#endif
}

TEST(StringPrintfTest, StringAppendfString) {
  std::string value("Hello");
  StringAppendF(&value, " %s", "World");
  EXPECT_EQ("Hello World", value);

#if defined(OS_WIN)
  std::wstring valuew(L"Hello");
  StringAppendF(&valuew, L" %ls", L"World");
  EXPECT_EQ(L"Hello World", valuew);

  std::u16string value16(u"Hello");
  StringAppendF(&value16, u" %ls", u"World");
  EXPECT_EQ(u"Hello World", value16);
#endif
}

TEST(StringPrintfTest, StringAppendfInt) {
  std::string value("Hello");
  StringAppendF(&value, " %d", 123);
  EXPECT_EQ("Hello 123", value);

#if defined(OS_WIN)
  std::wstring valuew(L"Hello");
  StringAppendF(&valuew, L" %d", 123);
  EXPECT_EQ(L"Hello 123", valuew);

  std::u16string value16(u"Hello");
  StringAppendF(&value16, u" %d", 123);
  EXPECT_EQ(u"Hello 123", value16);
#endif
}

// Make sure that lengths exactly around the initial buffer size are handled
// correctly.
TEST(StringPrintfTest, StringPrintfBounds) {
  const int kSrcLen = 1026;
  char src[kSrcLen];
  std::fill_n(src, kSrcLen, 'A');

  wchar_t srcw[kSrcLen];
  std::fill_n(srcw, kSrcLen, 'A');

  char16_t src16[kSrcLen];
  std::fill_n(src16, kSrcLen, 'A');

  for (int i = 1; i < 3; i++) {
    src[kSrcLen - i] = 0;
    std::string out;
    SStringPrintf(&out, "%s", src);
    EXPECT_STREQ(src, out.c_str());

#if defined(OS_WIN)
    srcw[kSrcLen - i] = 0;
    std::wstring outw;
    SStringPrintf(&outw, L"%ls", srcw);
    EXPECT_STREQ(srcw, outw.c_str());

    src16[kSrcLen - i] = 0;
    std::u16string out16;
    SStringPrintf(&out16, u"%ls", src16);
    // EXPECT_STREQ does not support const char16_t* strings yet.
    // Dispatch to the const wchar_t* overload instead.
    EXPECT_STREQ(reinterpret_cast<const wchar_t*>(src16),
                 reinterpret_cast<const wchar_t*>(out16.c_str()));
#endif
  }
}

// Test very large sprintfs that will cause the buffer to grow.
TEST(StringPrintfTest, Grow) {
  char src[1026];
  for (auto& i : src)
    i = 'A';
  src[1025] = 0;

  const char fmt[] = "%sB%sB%sB%sB%sB%sB%s";

  std::string out;
  SStringPrintf(&out, fmt, src, src, src, src, src, src, src);

  const int kRefSize = 320000;
  char* ref = new char[kRefSize];
#if defined(OS_WIN)
  sprintf_s(ref, kRefSize, fmt, src, src, src, src, src, src, src);
#elif defined(OS_POSIX) || defined(OS_FUCHSIA)
  snprintf(ref, kRefSize, fmt, src, src, src, src, src, src, src);
#endif

  EXPECT_STREQ(ref, out.c_str());
  delete[] ref;
}

TEST(StringPrintfTest, StringAppendV) {
  std::string out;
  StringAppendVTestHelper(&out, "%d foo %s", 1, "bar");
  EXPECT_EQ("1 foo bar", out);

#if defined(OS_WIN)
  std::wstring outw;
  StringAppendVTestHelper(&outw, L"%d foo %ls", 1, L"bar");
  EXPECT_EQ(L"1 foo bar", outw);

  std::u16string out16;
  StringAppendVTestHelper(&out16, u"%d foo %ls", 1, u"bar");
  EXPECT_EQ(u"1 foo bar", out16);
#endif
}

// Test the boundary condition for the size of the string_util's
// internal buffer.
TEST(StringPrintfTest, GrowBoundary) {
  const int kStringUtilBufLen = 1024;
  // Our buffer should be one larger than the size of StringAppendVT's stack
  // buffer.
  // And need extra one for NULL-terminator.
  const int kBufLen = kStringUtilBufLen + 1 + 1;
  char src[kBufLen];
  for (int i = 0; i < kBufLen - 1; ++i)
    src[i] = 'a';
  src[kBufLen - 1] = 0;

  std::string out;
  SStringPrintf(&out, "%s", src);

  EXPECT_STREQ(src, out.c_str());
}

#if defined(OS_WIN)
// vswprintf in Visual Studio 2013 fails when given U+FFFF. This tests that the
// failure case is gracefuly handled. In Visual Studio 2015 the bad character
// is passed through.
TEST(StringPrintfTest, Invalid) {
  wchar_t invalid[2];
  invalid[0] = 0xffff;
  invalid[1] = 0;

  std::wstring out;
  SStringPrintf(&out, L"%ls", invalid);
#if _MSC_VER >= 1900
  EXPECT_STREQ(invalid, out.c_str());
#else
  EXPECT_STREQ(L"", out.c_str());
#endif
}
#endif

// Test that StringPrintf and StringAppendV do not change errno.
TEST(StringPrintfTest, StringPrintfErrno) {
  errno = 1;
  EXPECT_EQ("", StringPrintf("%s", ""));
  EXPECT_EQ(1, errno);
  std::string out;
  StringAppendVTestHelper(&out, "%d foo %s", 1, "bar");
  EXPECT_EQ(1, errno);
}

}  // namespace base
