// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/browser_switcher/bho/mini_bho_util.h"

#include <string.h>

#include "base/files/file.h"
#include "base/files/file_util.h"
#include "base/files/scoped_temp_dir.h"
#include "base/strings/string16.h"
#include "base/strings/utf_string_conversions.h"
#include "base/test/test_suite.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {

// Default buffer size used for most tests.
const size_t BUFSZ = 10;

template <typename T>
void FillWithBogus(T* buffer, size_t size = BUFSZ) {
  memset(buffer, 0xAA, size * sizeof(*buffer));
}

template <typename T>
std::basic_string<T> GetBytes(T* buffer, size_t size = BUFSZ) {
  return std::basic_string<T>(buffer, size);
}

}  // namespace

// TODO(crbug.com/950039): Fails on Win7.
TEST(MiniBhoUtil, DISABLED_Logging) {
  base::ScopedTempDir temp_dir;
  ASSERT_TRUE(temp_dir.CreateUniqueTempDir());
  base::FilePath log_path = temp_dir.GetPath().AppendASCII("log.txt");

  util::SetLogFilePathForTesting(log_path.value().c_str());
  util::InitLog();

  util::puts(INFO, "hello world");
  util::printf(ERR, "n = %d\n", 34);

  util::CloseLog();

  const char expected[] =
      "[info] : hello world\n"
      "[*ERROR!*] : n = 34\n";

  EXPECT_TRUE(base::PathExists(log_path));
  base::File file(log_path, base::File::FLAG_OPEN | base::File::FLAG_READ);
  std::unique_ptr<char[]> buffer(new char[file.GetLength() + 1]);
  buffer.get()[file.GetLength()] = '\0';
  EXPECT_GT(file.Read(0, buffer.get(), file.GetLength()), -1);
  EXPECT_EQ(std::string(expected), std::string(buffer.get()));
}

TEST(MiniBhoUtil, VectorMove) {
  util::vector<double> vec1(10);
  util::vector<double> vec2;
  ASSERT_NE(nullptr, vec1.data());
  ASSERT_EQ(10u, vec1.capacity());
  ASSERT_EQ(nullptr, vec2.data());
  ASSERT_EQ(0u, vec2.capacity());

  vec2 = std::move(vec1);
  EXPECT_EQ(nullptr, vec1.data());
  EXPECT_EQ(0u, vec1.capacity());
  EXPECT_NE(nullptr, vec2.data());
  EXPECT_EQ(10u, vec2.capacity());

  util::vector<double> vec3(std::move(vec2));
  EXPECT_EQ(nullptr, vec2.data());
  EXPECT_EQ(0u, vec2.capacity());
  EXPECT_NE(nullptr, vec3.data());
  EXPECT_EQ(10u, vec3.capacity());
}

TEST(MiniBhoUtil, memmove) {
  char data[BUFSZ] = {
      '0', '1', '2', '3', '4', '5', '6', '7', '8', '9',
  };
  char buffer[BUFSZ];

  // Copy backwards.
  ::memcpy(buffer, data, BUFSZ);
  util::memmove(buffer + 5, buffer, 5);
  char expected[BUFSZ] = {
      '0', '1', '2', '3', '4', '0', '1', '2', '3', '4',
  };
  EXPECT_EQ(GetBytes(expected), GetBytes(buffer));

  // Copy forwards.
  ::memcpy(buffer, data, BUFSZ);
  util::memmove(buffer, buffer + 5, 5);
  char expected2[BUFSZ] = {
      '5', '6', '7', '8', '9', '5', '6', '7', '8', '9',
  };
  EXPECT_EQ(GetBytes(expected2), GetBytes(buffer));
}

TEST(MiniBhoUtil, strtok) {
  char str[] = "many,punctuation;separated:values";
  const char delim[] = ",;:";

  EXPECT_EQ("many", std::string(util::strtok(str, delim)));
  EXPECT_EQ("punctuation", std::string(util::strtok(nullptr, delim)));
  EXPECT_EQ("separated", std::string(util::strtok(nullptr, delim)));
  EXPECT_EQ("values", std::string(util::strtok(nullptr, delim)));
  EXPECT_EQ(nullptr, util::strtok(nullptr, delim));

  char str2[] = "no punctuation";
  EXPECT_EQ("no punctuation", std::string(util::strtok(str2, delim)));
  EXPECT_EQ(nullptr, util::strtok(nullptr, delim));

  char str3[] = "";
  EXPECT_EQ("", std::string(util::strtok(str3, delim)));
  EXPECT_EQ(nullptr, util::strtok(nullptr, delim));
}

TEST(MiniBhoUtil, wcs_replace_s) {
  // No match = no replacement.
  wchar_t buffer[BUFSZ];
  FillWithBogus(buffer);
  ::wcscpy(buffer, L"hello");
  EXPECT_FALSE(util::wcs_replace_s(buffer, BUFSZ, L"x", L"y"));
  EXPECT_EQ(std::wstring(L"hello"), std::wstring(buffer));
  wchar_t expected[] = {
      'h', 'e', 'l', 'l', 'o', '\0', 0xAAAA, 0xAAAA, 0xAAAA, 0xAAAA,
  };
  EXPECT_EQ(GetBytes(expected), GetBytes(buffer));

  // Replace in string, same length.
  FillWithBogus(buffer);
  ::wcscpy(buffer, L"hello");
  EXPECT_TRUE(util::wcs_replace_s(buffer, BUFSZ, L"ll", L"yy"));
  EXPECT_EQ(L"heyyo", std::wstring(buffer));
  wchar_t expected2[] = {
      'h', 'e', 'y', 'y', 'o', '\0', 0xAAAA, 0xAAAA, 0xAAAA, 0xAAAA,
  };
  EXPECT_EQ(GetBytes(expected2), GetBytes(buffer));

  // Replace in string, different length.
  FillWithBogus(buffer);
  ::wcscpy(buffer, L"hello");
  EXPECT_TRUE(util::wcs_replace_s(buffer, BUFSZ, L"e", L"ayy"));
  EXPECT_EQ(L"hayyllo", std::wstring(buffer));
  wchar_t expected3[] = {
      'h', 'a', 'y', 'y', 'l', 'l', 'o', '\0', 0xAAAA, 0xAAAA,
  };
  EXPECT_EQ(GetBytes(expected3), GetBytes(buffer));

  FillWithBogus(buffer);
  ::wcscpy(buffer, L"hello");
  EXPECT_TRUE(util::wcs_replace_s(buffer, BUFSZ, L"ell", L"ai"));
  EXPECT_EQ(L"haio", std::wstring(buffer));
  wchar_t expected4[] = {
      'h', 'a', 'i', 'o', '\0', '\0', 0xAAAA, 0xAAAA, 0xAAAA, 0xAAAA,
  };
  EXPECT_EQ(GetBytes(expected4), GetBytes(buffer));

  // Would cause a buffer overflow.
  FillWithBogus(buffer);
  ::wcscpy(buffer, L"hello");
  EXPECT_TRUE(util::wcs_replace_s(buffer, 7, L"ell", L"i there friend"));
  EXPECT_EQ(L"hi the", std::wstring(buffer));
  wchar_t expected5[] = {
      'h', 'i', ' ', 't', 'h', 'e', '\0', 0xAAAA, 0xAAAA, 0xAAAA,
  };
  EXPECT_EQ(GetBytes(expected5), GetBytes(buffer));
}

TEST(MiniBhoUtil, utf_conversions) {
  base::string16 expected_utf16 = STRING16_LITERAL("홍길동");
  std::string expected_utf8 = base::UTF16ToUTF8(expected_utf16);

  util::string utf8 = util::utf16_to_utf8(expected_utf16.c_str());
  util::wstring utf16 = util::utf8_to_utf16(expected_utf8.c_str());

  EXPECT_EQ(expected_utf8, std::string(utf8.data()));
  EXPECT_EQ(expected_utf16, std::wstring(utf16.data()));
}

int main(int argc, char** argv) {
  base::RunUnitTestsUsingBaseTestSuite(argc, argv);
}
