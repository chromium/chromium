// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/40284755): Remove this and spanify to fix the errors.
#pragma allow_unsafe_buffers
#endif

#include "base/strings/string_number_conversions.h"

#include <errno.h>
#include <limits.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>

#include <cmath>
#include <limits>
#include <string_view>

#include "base/bit_cast.h"
#include "base/format_macros.h"
#include "base/strings/stringprintf.h"
#include "base/strings/utf_string_conversions.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace base {

namespace {

template <typename INT>
struct NumberToStringTest {
  INT num;
  const char* sexpected;
  const char* uexpected;
};

}  // namespace

TEST(StringNumberConversionsTest, NumberToString) {
  static const NumberToStringTest<int> int_tests[] = {
      {0, "0", "0"},
      {-1, "-1", "4294967295"},
      {std::numeric_limits<int>::max(), "2147483647", "2147483647"},
      {std::numeric_limits<int>::min(), "-2147483648", "2147483648"},
  };
  static const NumberToStringTest<int64_t> int64_tests[] = {
      {0, "0", "0"},
      {-1, "-1", "18446744073709551615"},
      {
          std::numeric_limits<int64_t>::max(),
          "9223372036854775807",
          "9223372036854775807",
      },
      {std::numeric_limits<int64_t>::min(), "-9223372036854775808",
       "9223372036854775808"},
  };

  for (const auto& test : int_tests) {
    EXPECT_EQ(NumberToString(test.num), test.sexpected);
    EXPECT_EQ(NumberToString16(test.num), UTF8ToUTF16(test.sexpected));
    EXPECT_EQ(NumberToString(static_cast<unsigned>(test.num)), test.uexpected);
    EXPECT_EQ(NumberToString16(static_cast<unsigned>(test.num)),
              UTF8ToUTF16(test.uexpected));
  }
  for (const auto& test : int64_tests) {
    EXPECT_EQ(NumberToString(test.num), test.sexpected);
    EXPECT_EQ(NumberToString16(test.num), UTF8ToUTF16(test.sexpected));
    EXPECT_EQ(NumberToString(static_cast<uint64_t>(test.num)), test.uexpected);
    EXPECT_EQ(NumberToString16(static_cast<uint64_t>(test.num)),
              UTF8ToUTF16(test.uexpected));
  }
}

TEST(StringNumberConversionsTest, Uint64ToString) {
  static const struct {
    uint64_t input;
    std::string output;
  } cases[] = {
      {0, "0"},
      {42, "42"},
      {INT_MAX, "2147483647"},
      {std::numeric_limits<uint64_t>::max(), "18446744073709551615"},
  };

  for (const auto& i : cases)
    EXPECT_EQ(i.output, NumberToString(i.input));
}

TEST(StringNumberConversionsTest, SizeTToString) {
  size_t size_t_max = std::numeric_limits<size_t>::max();
  std::string size_t_max_string = StringPrintf("%" PRIuS, size_t_max);

  static const struct {
    size_t input;
    std::string output;
  } cases[] = {
    {0, "0"},
    {9, "9"},
    {42, "42"},
    {INT_MAX, "2147483647"},
    {2147483648U, "2147483648"},
#if SIZE_MAX > 4294967295U
    {99999999999U, "99999999999"},
#endif
    {size_t_max, size_t_max_string},
  };

  for (const auto& i : cases)
    EXPECT_EQ(i.output, NumberToString(i.input));
}

TEST(StringNumberConversionsTest, StringToInt) {
  static const struct {
    std::string input;
    int output;
    bool success;
  } cases[] = {
      {"0", 0, true},
      {"42", 42, true},
      {"42\x99", 42, false},
      {"\x99"
       "42\x99",
       0, false},
      {"-2147483648", INT_MIN, true},
      {"2147483647", INT_MAX, true},
      {"", 0, false},
      {" 42", 42, false},
      {"42 ", 42, false},
      {"\t\n\v\f\r 42", 42, false},
      {"blah42", 0, false},
      {"42blah", 42, false},
      {"blah42blah", 0, false},
      {"-273.15", -273, false},
      {"+98.6", 98, false},
      {"--123", 0, false},
      {"++123", 0, false},
      {"-+123", 0, false},
      {"+-123", 0, false},
      {"-", 0, false},
      {"-2147483649", INT_MIN, false},
      {"-99999999999", INT_MIN, false},
      {"2147483648", INT_MAX, false},
      {"99999999999", INT_MAX, false},
  };

  for (const auto& i : cases) {
    int output = i.output ^ 1;  // Ensure StringToInt wrote something.
    EXPECT_EQ(i.success, StringToInt(i.input, &output));
    EXPECT_EQ(i.output, output);

    std::u16string utf16_input = UTF8ToUTF16(i.input);
    output = i.output ^ 1;  // Ensure StringToInt wrote something.
    EXPECT_EQ(i.success, StringToInt(utf16_input, &output));
    EXPECT_EQ(i.output, output);
  }

  // One additional test to verify that conversion of numbers in strings with
  // embedded NUL characters.  The NUL and extra data after it should be
  // interpreted as junk after the number.
  const char input[] =
      "6\0"
      "6";
  std::string input_string(input, std::size(input) - 1);
  int output;
  EXPECT_FALSE(StringToInt(input_string, &output));
  EXPECT_EQ(6, output);

  std::u16string utf16_input = UTF8ToUTF16(input_string);
  output = 0;
  EXPECT_FALSE(StringToInt(utf16_input, &output));
  EXPECT_EQ(6, output);

  output = 0;
  const char16_t negative_wide_input[] = {0xFF4D, '4', '2', 0};
  EXPECT_FALSE(StringToInt(std::u16string(negative_wide_input), &output));
  EXPECT_EQ(0, output);
}

TEST(StringNumberConversionsTest, StringToUint) {
  static const struct {
    std::string input;
    unsigned output;
    bool success;
  } cases[] = {
      {"0", 0, true},
      {"42", 42, true},
      {"42\x99", 42, false},
      {"\x99"
       "42\x99",
       0, false},
      {"-2147483648", 0, false},
      {"2147483647", INT_MAX, true},
      {"", 0, false},
      {" 42", 42, false},
      {"42 ", 42, false},
      {"\t\n\v\f\r 42", 42, false},
      {"blah42", 0, false},
      {"42blah", 42, false},
      {"blah42blah", 0, false},
      {"-273.15", 0, false},
      {"+98.6", 98, false},
      {"--123", 0, false},
      {"++123", 0, false},
      {"-+123", 0, false},
      {"+-123", 0, false},
      {"-", 0, false},
      {"-2147483649", 0, false},
      {"-99999999999", 0, false},
      {"4294967295", UINT_MAX, true},
      {"4294967296", UINT_MAX, false},
      {"99999999999", UINT_MAX, false},
  };

  for (const auto& i : cases) {
    unsigned output = i.output ^ 1;  // Ensure StringToUint wrote something.
    EXPECT_EQ(i.success, StringToUint(i.input, &output));
    EXPECT_EQ(i.output, output);

    std::u16string utf16_input = UTF8ToUTF16(i.input);
    output = i.output ^ 1;  // Ensure StringToUint wrote something.
    EXPECT_EQ(i.success, StringToUint(utf16_input, &output));
    EXPECT_EQ(i.output, output);
  }

  // One additional test to verify that conversion of numbers in strings with
  // embedded NUL characters.  The NUL and extra data after it should be
  // interpreted as junk after the number.
  const char input[] =
      "6\0"
      "6";
  std::string input_string(input, std::size(input) - 1);
  unsigned output;
  EXPECT_FALSE(StringToUint(input_string, &output));
  EXPECT_EQ(6U, output);

  std::u16string utf16_input = UTF8ToUTF16(input_string);
  output = 0;
  EXPECT_FALSE(StringToUint(utf16_input, &output));
  EXPECT_EQ(6U, output);

  output = 0;
  const char16_t negative_wide_input[] = {0xFF4D, '4', '2', 0};
  EXPECT_FALSE(StringToUint(std::u16string(negative_wide_input), &output));
  EXPECT_EQ(0U, output);
}

TEST(StringNumberConversionsTest, StringToInt64) {
  static const struct {
    std::string input;
    int64_t output;
    bool success;
  } cases[] = {
      {"0", 0, true},
      {"42", 42, true},
      {"-2147483648", INT_MIN, true},
      {"2147483647", INT_MAX, true},
      {"-2147483649", INT64_C(-2147483649), true},
      {"-99999999999", INT64_C(-99999999999), true},
      {"2147483648", INT64_C(2147483648), true},
      {"99999999999", INT64_C(99999999999), true},
      {"9223372036854775807", std::numeric_limits<int64_t>::max(), true},
      {"-9223372036854775808", std::numeric_limits<int64_t>::min(), true},
      {"09", 9, true},
      {"-09", -9, true},
      {"", 0, false},
      {" 42", 42, false},
      {"42 ", 42, false},
      {"0x42", 0, false},
      {"\t\n\v\f\r 42", 42, false},
      {"blah42", 0, false},
      {"42blah", 42, false},
      {"blah42blah", 0, false},
      {"-273.15", -273, false},
      {"+98.6", 98, false},
      {"--123", 0, false},
      {"++123", 0, false},
      {"-+123", 0, false},
      {"+-123", 0, false},
      {"-", 0, false},
      {"-9223372036854775809", std::numeric_limits<int64_t>::min(), false},
      {"-99999999999999999999", std::numeric_limits<int64_t>::min(), false},
      {"9223372036854775808", std::numeric_limits<int64_t>::max(), false},
      {"99999999999999999999", std::numeric_limits<int64_t>::max(), false},
  };

  for (const auto& i : cases) {
    int64_t output = 0;
    EXPECT_EQ(i.success, StringToInt64(i.input, &output));
    EXPECT_EQ(i.output, output);

    std::u16string utf16_input = UTF8ToUTF16(i.input);
    output = 0;
    EXPECT_EQ(i.success, StringToInt64(utf16_input, &output));
    EXPECT_EQ(i.output, output);
  }

  // One additional test to verify that conversion of numbers in strings with
  // embedded NUL characters.  The NUL and extra data after it should be
  // interpreted as junk after the number.
  const char input[] =
      "6\0"
      "6";
  std::string input_string(input, std::size(input) - 1);
  int64_t output;
  EXPECT_FALSE(StringToInt64(input_string, &output));
  EXPECT_EQ(6, output);

  std::u16string utf16_input = UTF8ToUTF16(input_string);
  output = 0;
  EXPECT_FALSE(StringToInt64(utf16_input, &output));
  EXPECT_EQ(6, output);
}

TEST(StringNumberConversionsTest, StringToUint64) {
  static const struct {
    std::string input;
    uint64_t output;
    bool success;
  } cases[] = {
      {"0", 0, true},
      {"42", 42, true},
      {"-2147483648", 0, false},
      {"2147483647", INT_MAX, true},
      {"-2147483649", 0, false},
      {"-99999999999", 0, false},
      {"2147483648", UINT64_C(2147483648), true},
      {"99999999999", UINT64_C(99999999999), true},
      {"9223372036854775807", std::numeric_limits<int64_t>::max(), true},
      {"-9223372036854775808", 0, false},
      {"09", 9, true},
      {"-09", 0, false},
      {"", 0, false},
      {" 42", 42, false},
      {"42 ", 42, false},
      {"0x42", 0, false},
      {"\t\n\v\f\r 42", 42, false},
      {"blah42", 0, false},
      {"42blah", 42, false},
      {"blah42blah", 0, false},
      {"-273.15", 0, false},
      {"+98.6", 98, false},
      {"--123", 0, false},
      {"++123", 0, false},
      {"-+123", 0, false},
      {"+-123", 0, false},
      {"-", 0, false},
      {"-9223372036854775809", 0, false},
      {"-99999999999999999999", 0, false},
      {"9223372036854775808", UINT64_C(9223372036854775808), true},
      {"99999999999999999999", std::numeric_limits<uint64_t>::max(), false},
      {"18446744073709551615", std::numeric_limits<uint64_t>::max(), true},
      {"18446744073709551616", std::numeric_limits<uint64_t>::max(), false},
  };

  for (const auto& i : cases) {
    uint64_t output = 0;
    EXPECT_EQ(i.success, StringToUint64(i.input, &output));
    EXPECT_EQ(i.output, output);

    std::u16string utf16_input = UTF8ToUTF16(i.input);
    output = 0;
    EXPECT_EQ(i.success, StringToUint64(utf16_input, &output));
    EXPECT_EQ(i.output, output);
  }

  // One additional test to verify that conversion of numbers in strings with
  // embedded NUL characters.  The NUL and extra data after it should be
  // interpreted as junk after the number.
  const char input[] =
      "6\0"
      "6";
  std::string input_string(input, std::size(input) - 1);
  uint64_t output;
  EXPECT_FALSE(StringToUint64(input_string, &output));
  EXPECT_EQ(6U, output);

  std::u16string utf16_input = UTF8ToUTF16(input_string);
  output = 0;
  EXPECT_FALSE(StringToUint64(utf16_input, &output));
  EXPECT_EQ(6U, output);
}

TEST(StringNumberConversionsTest, StringToSizeT) {
  size_t size_t_max = std::numeric_limits<size_t>::max();
  std::string size_t_max_string = StringPrintf("%" PRIuS, size_t_max);

  static const struct {
    std::string input;
    size_t output;
    bool success;
  } cases[] = {
    {"0", 0, true},
    {"42", 42, true},
    {"-2147483648", 0, false},
    {"2147483647", INT_MAX, true},
    {"-2147483649", 0, false},
    {"-99999999999", 0, false},
    {"2147483648", 2147483648U, true},
#if SIZE_MAX > 4294967295U
    {"99999999999", 99999999999U, true},
#endif
    {"-9223372036854775808", 0, false},
    {"09", 9, true},
    {"-09", 0, false},
    {"", 0, false},
    {" 42", 42, false},
    {"42 ", 42, false},
    {"0x42", 0, false},
    {"\t\n\v\f\r 42", 42, false},
    {"blah42", 0, false},
    {"42blah", 42, false},
    {"blah42blah", 0, false},
    {"-273.15", 0, false},
    {"+98.6", 98, false},
    {"--123", 0, false},
    {"++123", 0, false},
    {"-+123", 0, false},
    {"+-123", 0, false},
    {"-", 0, false},
    {"-9223372036854775809", 0, false},
    {"-99999999999999999999", 0, false},
    {"999999999999999999999999", size_t_max, false},
    {size_t_max_string, size_t_max, true},
  };

  for (const auto& i : cases) {
    size_t output = 0;
    EXPECT_EQ(i.success, StringToSizeT(i.input, &output));
    EXPECT_EQ(i.output, output);

    std::u16string utf16_input = UTF8ToUTF16(i.input);
    output = 0;
    EXPECT_EQ(i.success, StringToSizeT(utf16_input, &output));
    EXPECT_EQ(i.output, output);
  }

  // One additional test to verify that conversion of numbers in strings with
  // embedded NUL characters.  The NUL and extra data after it should be
  // interpreted as junk after the number.
  const char input[] =
      "6\0"
      "6";
  std::string input_string(input, std::size(input) - 1);
  size_t output;
  EXPECT_FALSE(StringToSizeT(input_string, &output));
  EXPECT_EQ(6U, output);

  std::u16string utf16_input = UTF8ToUTF16(input_string);
  output = 0;
  EXPECT_FALSE(StringToSizeT(utf16_input, &output));
  EXPECT_EQ(6U, output);
}

TEST(StringNumberConversionsTest, HexStringToInt) {
  static const struct {
    std::string input;
    int64_t output;
    bool success;
  } cases[] = {
      {"0", 0, true},
      {"42", 66, true},
      {"-42", -66, true},
      {"+42", 66, true},
      {"7fffffff", INT_MAX, true},
      {"-80000000", INT_MIN, true},
      {"80000000", INT_MAX, false},   // Overflow test.
      {"-80000001", INT_MIN, false},  // Underflow test.
      {"0x42", 66, true},
      {"-0x42", -66, true},
      {"+0x42", 66, true},
      {"0x7fffffff", INT_MAX, true},
      {"-0x80000000", INT_MIN, true},
      {"-80000000", INT_MIN, true},
      {"80000000", INT_MAX, false},   // Overflow test.
      {"-80000001", INT_MIN, false},  // Underflow test.
      {"0x0f", 15, true},
      {"0f", 15, true},
      {" 45", 0x45, false},
      {"\t\n\v\f\r 0x45", 0x45, false},
      {" 45", 0x45, false},
      {"45 ", 0x45, false},
      {"45:", 0x45, false},
      {"efgh", 0xef, false},
      {"0xefgh", 0xef, false},
      {"hgfe", 0, false},
      {"-", 0, false},
      {"", 0, false},
      {"0x", 0, false},
  };

  for (const auto& i : cases) {
    int output = 0;
    EXPECT_EQ(i.success, HexStringToInt(i.input, &output));
    EXPECT_EQ(i.output, output);
  }
  // One additional test to verify that conversion of numbers in strings with
  // embedded NUL characters.  The NUL and extra data after it should be
  // interpreted as junk after the number.
  const char input[] =
      "0xc0ffee\0"
      "9";
  std::string input_string(input, std::size(input) - 1);
  int output;
  EXPECT_FALSE(HexStringToInt(input_string, &output));
  EXPECT_EQ(0xc0ffee, output);
}

TEST(StringNumberConversionsTest, HexStringToUInt) {
  static const struct {
    std::string input;
    uint32_t output;
    bool success;
  } cases[] = {
      {"0", 0, true},
      {"42", 0x42, true},
      {"-42", 0, false},
      {"+42", 0x42, true},
      {"7fffffff", INT_MAX, true},
      {"-80000000", 0, false},
      {"ffffffff", 0xffffffff, true},
      {"DeadBeef", 0xdeadbeef, true},
      {"0x42", 0x42, true},
      {"-0x42", 0, false},
      {"+0x42", 0x42, true},
      {"0x7fffffff", INT_MAX, true},
      {"-0x80000000", 0, false},
      {"0xffffffff", std::numeric_limits<uint32_t>::max(), true},
      {"0XDeadBeef", 0xdeadbeef, true},
      {"0x7fffffffffffffff", std::numeric_limits<uint32_t>::max(),
       false},  // Overflow test.
      {"-0x8000000000000000", 0, false},
      {"0x8000000000000000", std::numeric_limits<uint32_t>::max(),
       false},  // Overflow test.
      {"-0x8000000000000001", 0, false},
      {"0xFFFFFFFFFFFFFFFF", std::numeric_limits<uint32_t>::max(),
       false},  // Overflow test.
      {"FFFFFFFFFFFFFFFF", std::numeric_limits<uint32_t>::max(),
       false},  // Overflow test.
      {"0x0000000000000000", 0, true},
      {"0000000000000000", 0, true},
      {"1FFFFFFFFFFFFFFFF", std::numeric_limits<uint32_t>::max(),
       false},  // Overflow test.
      {"0x0f", 0x0f, true},
      {"0f", 0x0f, true},
      {" 45", 0x45, false},
      {"\t\n\v\f\r 0x45", 0x45, false},
      {" 45", 0x45, false},
      {"45 ", 0x45, false},
      {"45:", 0x45, false},
      {"efgh", 0xef, false},
      {"0xefgh", 0xef, false},
      {"hgfe", 0, false},
      {"-", 0, false},
      {"", 0, false},
      {"0x", 0, false},
  };

  for (const auto& i : cases) {
    uint32_t output = 0;
    EXPECT_EQ(i.success, HexStringToUInt(i.input, &output));
    EXPECT_EQ(i.output, output);
  }
  // One additional test to verify that conversion of numbers in strings with
  // embedded NUL characters.  The NUL and extra data after it should be
  // interpreted as junk after the number.
  const char input[] =
      "0xc0ffee\0"
      "9";
  std::string input_string(input, std::size(input) - 1);
  uint32_t output;
  EXPECT_FALSE(HexStringToUInt(input_string, &output));
  EXPECT_EQ(0xc0ffeeU, output);
}

TEST(StringNumberConversionsTest, HexStringToInt64) {
  static const struct {
    std::string input;
    int64_t output;
    bool success;
  } cases[] = {
      {"0", 0, true},
      {"42", 66, true},
      {"-42", -66, true},
      {"+42", 66, true},
      {"40acd88557b", INT64_C(4444444448123), true},
      {"7fffffff", INT_MAX, true},
      {"-80000000", INT_MIN, true},
      {"ffffffff", 0xffffffff, true},
      {"DeadBeef", 0xdeadbeef, true},
      {"0x42", 66, true},
      {"-0x42", -66, true},
      {"+0x42", 66, true},
      {"0x40acd88557b", INT64_C(4444444448123), true},
      {"0x7fffffff", INT_MAX, true},
      {"-0x80000000", INT_MIN, true},
      {"0xffffffff", 0xffffffff, true},
      {"0XDeadBeef", 0xdeadbeef, true},
      {"0x7fffffffffffffff", std::numeric_limits<int64_t>::max(), true},
      {"-0x8000000000000000", std::numeric_limits<int64_t>::min(), true},
      {"0x8000000000000000", std::numeric_limits<int64_t>::max(),
       false},  // Overflow test.
      {"-0x8000000000000001", std::numeric_limits<int64_t>::min(),
       false},  // Underflow test.
      {"0x0f", 15, true},
      {"0f", 15, true},
      {" 45", 0x45, false},
      {"\t\n\v\f\r 0x45", 0x45, false},
      {" 45", 0x45, false},
      {"45 ", 0x45, false},
      {"45:", 0x45, false},
      {"efgh", 0xef, false},
      {"0xefgh", 0xef, false},
      {"hgfe", 0, false},
      {"-", 0, false},
      {"", 0, false},
      {"0x", 0, false},
  };

  for (const auto& i : cases) {
    int64_t output = 0;
    EXPECT_EQ(i.success, HexStringToInt64(i.input, &output));
    EXPECT_EQ(i.output, output);
  }
  // One additional test to verify that conversion of numbers in strings with
  // embedded NUL characters.  The NUL and extra data after it should be
  // interpreted as junk after the number.
  const char input[] =
      "0xc0ffee\0"
      "9";
  std::string input_string(input, std::size(input) - 1);
  int64_t output;
  EXPECT_FALSE(HexStringToInt64(input_string, &output));
  EXPECT_EQ(0xc0ffee, output);
}

TEST(StringNumberConversionsTest, HexStringToUInt64) {
  static const struct {
    std::string input;
    uint64_t output;
    bool success;
  } cases[] = {
      {"0", 0, true},
      {"42", 66, true},
      {"-42", 0, false},
      {"+42", 66, true},
      {"40acd88557b", INT64_C(4444444448123), true},
      {"7fffffff", INT_MAX, true},
      {"-80000000", 0, false},
      {"ffffffff", 0xffffffff, true},
      {"DeadBeef", 0xdeadbeef, true},
      {"0x42", 66, true},
      {"-0x42", 0, false},
      {"+0x42", 66, true},
      {"0x40acd88557b", INT64_C(4444444448123), true},
      {"0x7fffffff", INT_MAX, true},
      {"-0x80000000", 0, false},
      {"0xffffffff", 0xffffffff, true},
      {"0XDeadBeef", 0xdeadbeef, true},
      {"0x7fffffffffffffff", std::numeric_limits<int64_t>::max(), true},
      {"-0x8000000000000000", 0, false},
      {"0x8000000000000000", UINT64_C(0x8000000000000000), true},
      {"-0x8000000000000001", 0, false},
      {"0xFFFFFFFFFFFFFFFF", std::numeric_limits<uint64_t>::max(), true},
      {"FFFFFFFFFFFFFFFF", std::numeric_limits<uint64_t>::max(), true},
      {"0x0000000000000000", 0, true},
      {"0000000000000000", 0, true},
      {"1FFFFFFFFFFFFFFFF", std::numeric_limits<uint64_t>::max(),
       false},  // Overflow test.
      {"0x0f", 15, true},
      {"0f", 15, true},
      {" 45", 0x45, false},
      {"\t\n\v\f\r 0x45", 0x45, false},
      {" 45", 0x45, false},
      {"45 ", 0x45, false},
      {"45:", 0x45, false},
      {"efgh", 0xef, false},
      {"0xefgh", 0xef, false},
      {"hgfe", 0, false},
      {"-", 0, false},
      {"", 0, false},
      {"0x", 0, false},
  };

  for (const auto& i : cases) {
    uint64_t output = 0;
    EXPECT_EQ(i.success, HexStringToUInt64(i.input, &output));
    EXPECT_EQ(i.output, output);
  }
  // One additional test to verify that conversion of numbers in strings with
  // embedded NUL characters.  The NUL and extra data after it should be
  // interpreted as junk after the number.
  const char input[] =
      "0xc0ffee\0"
      "9";
  std::string input_string(input, std::size(input) - 1);
  uint64_t output;
  EXPECT_FALSE(HexStringToUInt64(input_string, &output));
  EXPECT_EQ(0xc0ffeeU, output);
}

// Tests for HexStringToBytes, HexStringToString, HexStringToSpan.
TEST(StringNumberConversionsTest, HexStringToBytesStringSpan) {
  static const struct {
    const std::string input;
    const char* output;
    size_t output_len;
    bool success;
  } cases[] = {
      {"0", "", 0, false},  // odd number of characters fails
      {"00", "\0", 1, true},
      {"42", "\x42", 1, true},
      {"-42", "", 0, false},  // any non-hex value fails
      {"+42", "", 0, false},
      {"7fffffff", "\x7f\xff\xff\xff", 4, true},
      {"80000000", "\x80\0\0\0", 4, true},
      {"deadbeef", "\xde\xad\xbe\xef", 4, true},
      {"DeadBeef", "\xde\xad\xbe\xef", 4, true},
      {"0x42", "", 0, false},  // leading 0x fails (x is not hex)
      {"0f", "\xf", 1, true},
      {"45  ", "\x45", 1, false},
      {"efgh", "\xef", 1, false},
      {"", "", 0, false},
      {"0123456789ABCDEF", "\x01\x23\x45\x67\x89\xAB\xCD\xEF", 8, true},
      {"0123456789ABCDEF012345", "\x01\x23\x45\x67\x89\xAB\xCD\xEF\x01\x23\x45",
       11, true},
  };

  for (size_t test_i = 0; test_i < std::size(cases); ++test_i) {
    const auto& test = cases[test_i];

    std::string expected_output(test.output, test.output_len);

    // Test HexStringToBytes().
    {
      std::vector<uint8_t> output;
      EXPECT_EQ(test.success, HexStringToBytes(test.input, &output))
          << test_i << ": " << test.input;
      EXPECT_EQ(expected_output, std::string(output.begin(), output.end()));
    }

    // Test HexStringToString().
    {
      std::string output;
      EXPECT_EQ(test.success, HexStringToString(test.input, &output))
          << test_i << ": " << test.input;
      EXPECT_EQ(expected_output, output) << test_i << ": " << test.input;
    }

    // Test HexStringToSpan() with a properly sized output.
    {
      std::vector<uint8_t> output;
      output.resize(test.input.size() / 2);

      EXPECT_EQ(test.success, HexStringToSpan(test.input, output))
          << test_i << ": " << test.input;

      // On failure the output will only have been partially written (with
      // everything after the failure being 0).
      for (size_t i = 0; i < test.output_len; ++i) {
        EXPECT_EQ(test.output[i], static_cast<char>(output[i]))
            << test_i << ": " << test.input;
      }
      for (size_t i = test.output_len; i < output.size(); ++i) {
        EXPECT_EQ('\0', static_cast<char>(output[i]))
            << test_i << ": " << test.input;
      }
    }

    // Test HexStringToSpan() with an output that is 1 byte too small.
    {
      std::vector<uint8_t> output;
      if (test.input.size() > 1)
        output.resize(test.input.size() / 2 - 1);

      EXPECT_FALSE(HexStringToSpan(test.input, output))
          << test_i << ": " << test.input;
    }

    // Test HexStringToSpan() with an output that is 1 byte too large.
    {
      std::vector<uint8_t> output;
      output.resize(test.input.size() / 2 + 1);

      EXPECT_FALSE(HexStringToSpan(test.input, output))
          << test_i << ": " << test.input;
    }
  }
}

TEST(StringNumberConversionsTest, StringToDouble) {
  static const struct {
    std::string input;
    double output;
    bool success;
  } cases[] = {
      // Test different forms of zero.
      {"0", 0.0, true},
      {"+0", 0.0, true},
      {"-0", 0.0, true},
      {"0.0", 0.0, true},
      {"000000000000000000000000000000.0", 0.0, true},
      {"0.000000000000000000000000000", 0.0, true},

      // Test the answer.
      {"42", 42.0, true},
      {"-42", -42.0, true},

      // Test variances of an ordinary number.
      {"123.45", 123.45, true},
      {"-123.45", -123.45, true},
      {"+123.45", 123.45, true},

      // Test different forms of representation.
      {"2.99792458e8", 299792458.0, true},
      {"149597870.691E+3", 149597870691.0, true},
      {"6.", 6.0, true},

      // Test around the largest/smallest value that a double can represent.
      {"9e307", 9e307, true},
      {"1.7976e308", 1.7976e308, true},
      {"1.7977e308", HUGE_VAL, false},
      {"1.797693134862315807e+308", HUGE_VAL, true},
      {"1.797693134862315808e+308", HUGE_VAL, false},
      {"9e308", HUGE_VAL, false},
      {"9e309", HUGE_VAL, false},
      {"9e999", HUGE_VAL, false},
      {"9e1999", HUGE_VAL, false},
      {"9e19999", HUGE_VAL, false},
      {"9e99999999999999999999", HUGE_VAL, false},
      {"-9e307", -9e307, true},
      {"-1.7976e308", -1.7976e308, true},
      {"-1.7977e308", -HUGE_VAL, false},
      {"-1.797693134862315807e+308", -HUGE_VAL, true},
      {"-1.797693134862315808e+308", -HUGE_VAL, false},
      {"-9e308", -HUGE_VAL, false},
      {"-9e309", -HUGE_VAL, false},
      {"-9e999", -HUGE_VAL, false},
      {"-9e1999", -HUGE_VAL, false},
      {"-9e19999", -HUGE_VAL, false},
      {"-9e99999999999999999999", -HUGE_VAL, false},

      // Test more exponents.
      {"1e-2", 0.01, true},
      {"42 ", 42.0, false},
      {" 1e-2", 0.01, false},
      {"1e-2 ", 0.01, false},
      {"-1E-7", -0.0000001, true},
      {"01e02", 100, true},
      {"2.3e15", 2.3e15, true},
      {"100e-309", 100e-309, true},

      // Test some invalid cases.
      {"\t\n\v\f\r -123.45e2", -12345.0, false},
      {"+123 e4", 123.0, false},
      {"123e ", 123.0, false},
      {"123e", 123.0, false},
      {"10.5px", 10.5, false},
      {"11.5e2em", 1150, false},
      {" 2.99", 2.99, false},
      {"1e3.4", 1000.0, false},
      {"nothing", 0.0, false},
      {"-", 0.0, false},
      {"+", 0.0, false},
      {"", 0.0, false},

      // crbug.org/588726
      {"-0.0010000000000000000000000000000000000000001e-256",
       -1.0000000000000001e-259, true},
  };

  for (size_t i = 0; i < std::size(cases); ++i) {
    SCOPED_TRACE(
        StringPrintf("case %" PRIuS " \"%s\"", i, cases[i].input.c_str()));
    double output;
    errno = 1;
    EXPECT_EQ(cases[i].success, StringToDouble(cases[i].input, &output));
    if (cases[i].success)
      EXPECT_EQ(1, errno) << i;  // confirm that errno is unchanged.
    EXPECT_DOUBLE_EQ(cases[i].output, output);
  }

  // One additional test to verify that conversion of numbers in strings with
  // embedded NUL characters.  The NUL and extra data after it should be
  // interpreted as junk after the number.
  const char input[] =
      "3.14\0"
      "159";
  std::string input_string(input, std::size(input) - 1);
  double output;
  EXPECT_FALSE(StringToDouble(input_string, &output));
  EXPECT_DOUBLE_EQ(3.14, output);
}

TEST(StringNumberConversionsTest, DoubleToString) {
  static const struct {
    double input;
    const char* expected;
  } cases[] = {
      {0.0, "0"},
      {0.5, "0.5"},
      {1.25, "1.25"},
      {1.33518e+012, "1.33518e+12"},
      {1.33489e+012, "1.33489e+12"},
      {1.33505e+012, "1.33505e+12"},
      {1.33545e+009, "1335450000"},
      {1.33503e+009, "1335030000"},
  };

  for (const auto& i : cases) {
    EXPECT_EQ(i.expected, NumberToString(i.input));
    EXPECT_EQ(i.expected, UTF16ToUTF8(NumberToString16(i.input)));
  }

  // The following two values were seen in crashes in the wild.
  const char input_bytes[8] = {0, 0, 0, 0, '\xee', '\x6d', '\x73', '\x42'};
  double input = 0;
  memcpy(&input, input_bytes, std::size(input_bytes));
  EXPECT_EQ("1.335179083776e+12", NumberToString(input));
  const char input_bytes2[8] = {0,      0,      0,      '\xa0',
                                '\xda', '\x6c', '\x73', '\x42'};
  input = 0;
  memcpy(&input, input_bytes2, std::size(input_bytes2));
  EXPECT_EQ("1.33489033216e+12", NumberToString(input));
}

TEST(StringNumberConversionsTest, AppendHexEncodedByte) {
  std::string hex;
  AppendHexEncodedByte(0, hex);
  AppendHexEncodedByte(0, hex, false);
  AppendHexEncodedByte(1, hex);
  AppendHexEncodedByte(1, hex, false);
  AppendHexEncodedByte(0xf, hex);
  AppendHexEncodedByte(0xf, hex, false);
  AppendHexEncodedByte(0x8a, hex);
  AppendHexEncodedByte(0x8a, hex, false);
  AppendHexEncodedByte(0xe0, hex);
  AppendHexEncodedByte(0xe0, hex, false);
  AppendHexEncodedByte(0xff, hex);
  AppendHexEncodedByte(0xff, hex, false);
  EXPECT_EQ(hex, "000001010F0f8A8aE0e0FFff");
}

TEST(StringNumberConversionsTest, HexEncode) {
  EXPECT_EQ(HexEncode(nullptr, 0), "");
  EXPECT_EQ(HexEncode(base::span<uint8_t>()), "");
  EXPECT_EQ(HexEncode(std::string()), "");

  const uint8_t kBytes[] = {0x01, 0xff, 0x02, 0xfe, 0x03, 0x80, 0x81};
  EXPECT_EQ(HexEncode(kBytes, sizeof(kBytes)), "01FF02FE038081");
  EXPECT_EQ(HexEncode(kBytes), "01FF02FE038081");  // Implicit span conversion.

  const std::string kString = "\x01\xff";
  EXPECT_EQ(HexEncode(kString.c_str(), kString.size()), "01FF");
  EXPECT_EQ(HexEncode(kString),
            "01FF");  // Implicit std::string_view conversion.
}

// Test cases of known-bad strtod conversions that motivated the use of dmg_fp.
// See https://bugs.chromium.org/p/chromium/issues/detail?id=593512.
TEST(StringNumberConversionsTest, StrtodFailures) {
  static const struct {
    const char* input;
    uint64_t expected;
  } cases[] = {
      // http://www.exploringbinary.com/incorrectly-rounded-conversions-in-visual-c-plus-plus/
      {"9214843084008499", 0x43405e6cec57761aULL},
      {"0.500000000000000166533453693773481063544750213623046875",
       0x3fe0000000000002ULL},
      {"30078505129381147446200", 0x44997a3c7271b021ULL},
      {"1777820000000000000001", 0x4458180d5bad2e3eULL},
      {"0.500000000000000166547006220929549868969843373633921146392822265625",
       0x3fe0000000000002ULL},
      {"0.50000000000000016656055874808561867439493653364479541778564453125",
       0x3fe0000000000002ULL},
      {"0.3932922657273", 0x3fd92bb352c4623aULL},

      // http://www.exploringbinary.com/incorrectly-rounded-conversions-in-gcc-and-glibc/
      {"0.500000000000000166533453693773481063544750213623046875",
       0x3fe0000000000002ULL},
      {"3.518437208883201171875e13", 0x42c0000000000002ULL},
      {"62.5364939768271845828", 0x404f44abd5aa7ca4ULL},
      {"8.10109172351e-10", 0x3e0bd5cbaef0fd0cULL},
      {"1.50000000000000011102230246251565404236316680908203125",
       0x3ff8000000000000ULL},
      {"9007199254740991.4999999999999999999999999999999995",
       0x433fffffffffffffULL},

      // http://www.exploringbinary.com/incorrect-decimal-to-floating-point-conversion-in-sqlite/
      {"1e-23", 0x3b282db34012b251ULL},
      {"8.533e+68", 0x4e3fa69165a8eea2ULL},
      {"4.1006e-184", 0x19dbe0d1c7ea60c9ULL},
      {"9.998e+307", 0x7fe1cc0a350ca87bULL},
      {"9.9538452227e-280", 0x0602117ae45cde43ULL},
      {"6.47660115e-260", 0x0a1fdd9e333badadULL},
      {"7.4e+47", 0x49e033d7eca0adefULL},
      {"5.92e+48", 0x4a1033d7eca0adefULL},
      {"7.35e+66", 0x4dd172b70eababa9ULL},
      {"8.32116e+55", 0x4b8b2628393e02cdULL},
  };

  for (const auto& test : cases) {
    SCOPED_TRACE(StringPrintf("input: \"%s\"", test.input));
    double output;
    EXPECT_TRUE(StringToDouble(test.input, &output));
    EXPECT_EQ(bit_cast<uint64_t>(output), test.expected);
  }
}

}  // namespace base
