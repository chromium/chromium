// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/strings/utf_string_conversions.h"

#include <string_view>
#include <tuple>

#include "base/containers/span.h"
#include "base/strings/string_util.h"
#include "base/strings/string_view_util.h"
#include "testing/libfuzzer/libfuzzer_base_wrappers.h"

std::string output_std_string;
std::wstring output_std_wstring;
std::u16string output_string16;

// Entry point for LibFuzzer.
DEFINE_LLVM_FUZZER_TEST_ONE_INPUT_SPAN(base::span<const uint8_t> data) {
  std::string_view string_piece_input = base::as_string_view(data);

  std::ignore = base::UTF8ToWide(string_piece_input);
  std::ignore = base::UTF8ToWide(string_piece_input, &output_std_wstring);
  std::ignore = base::UTF8ToUTF16(string_piece_input);
  std::ignore = base::UTF8ToUTF16(string_piece_input, &output_string16);

  // Test for char16_t.
  if (data.size() % sizeof(char16_t) == 0) {
    std::u16string_view string_piece_input16 = base::as_string_view(
        base::subtle::reinterpret_span<const char16_t>(data));
    std::ignore = base::UTF16ToWide(output_string16);
    std::ignore = base::UTF16ToWide(string_piece_input16, &output_std_wstring);
    std::ignore = base::UTF16ToUTF8(string_piece_input16);
    std::ignore = base::UTF16ToUTF8(string_piece_input16, &output_std_string);
  }

  // Test for wchar_t.
  if (data.size() % sizeof(wchar_t) == 0) {
    std::wstring_view string_piece_input_wide = base::as_string_view(
        base::subtle::reinterpret_span<const wchar_t>(data));
    std::ignore = base::WideToUTF8(output_std_wstring);
    std::ignore = base::WideToUTF8(string_piece_input_wide, &output_std_string);
    std::ignore = base::WideToUTF16(output_std_wstring);
    std::ignore = base::WideToUTF16(string_piece_input_wide, &output_string16);
  }

  // Test for ASCII. This condition is needed to avoid hitting instant CHECK
  // failures.
  if (base::IsStringASCII(string_piece_input)) {
    output_string16 = base::ASCIIToUTF16(string_piece_input);
    std::u16string_view string_piece_input16(output_string16);
    std::ignore = base::UTF16ToASCII(string_piece_input16);
  }

  return 0;
}
