// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/40284755): Remove this and spanify to fix the errors.
#pragma allow_unsafe_buffers
#endif

#include "base/strings/stringprintf.h"

#include <errno.h>
#include <stddef.h>

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
}

TEST(StringPrintfTest, StringAppendfEmptyString) {
  std::string value("Hello");
  StringAppendF(&value, "%s", "");
  EXPECT_EQ("Hello", value);
}

TEST(StringPrintfTest, StringAppendfString) {
  std::string value("Hello");
  StringAppendF(&value, " %s", "World");
  EXPECT_EQ("Hello World", value);
}

TEST(StringPrintfTest, StringAppendfInt) {
  std::string value("Hello");
  StringAppendF(&value, " %d", 123);
  EXPECT_EQ("Hello 123", value);
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
    EXPECT_EQ(src, StringPrintf("%s", src));
  }
}

// Test very large sprintfs that will cause the buffer to grow.
TEST(StringPrintfTest, Grow) {
  char src[1026];
  for (auto& i : src)
    i = 'A';
  src[1025] = 0;

  const char fmt[] = "%sB%sB%sB%sB%sB%sB%s";

  const int kRefSize = 320000;
  char* ref = new char[kRefSize];
#if BUILDFLAG(IS_WIN)
  sprintf_s(ref, kRefSize, fmt, src, src, src, src, src, src, src);
#elif BUILDFLAG(IS_POSIX) || BUILDFLAG(IS_FUCHSIA)
  snprintf(ref, kRefSize, fmt, src, src, src, src, src, src, src);
#endif

  EXPECT_EQ(ref, StringPrintf(fmt, src, src, src, src, src, src, src));
  delete[] ref;
}

TEST(StringPrintfTest, StringAppendV) {
  std::string out;
  StringAppendVTestHelper(&out, "%d foo %s", 1, "bar");
  EXPECT_EQ("1 foo bar", out);
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

  EXPECT_EQ(src, StringPrintf("%s", src));
}

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
