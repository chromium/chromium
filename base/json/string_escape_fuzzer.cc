// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/json/string_escape.h"

#include <memory>
#include <string_view>

#include "base/compiler_specific.h"
#include "base/containers/heap_array.h"
#include "base/containers/span.h"

// Entry point for LibFuzzer.
extern "C" int LLVMFuzzerTestOneInput(const uint8_t* data, size_t size) {
  if (size < 2)
    return 0;

  // SAFETY: required from fuzzer.
  auto all_input = UNSAFE_BUFFERS(base::span<const uint8_t>(data, size));

  const bool put_in_quotes = all_input[size - 1];

  // Create a copy of input buffer, as otherwise we don't catch
  // overflow that touches the last byte (which is used in put_in_quotes).
  auto input = base::HeapArray<char>::CopiedFrom(
      base::as_chars(all_input.first(size - 1)));

  std::string_view input_string = base::as_string_view(input.as_span());
  std::string escaped_string;
  base::EscapeJSONString(input_string, put_in_quotes, &escaped_string);

  // Test for wide-strings if available size is even.
  if (input.size() & 1) {
    return 0;
  }

  size_t actual_size_char16 = input.size() / 2;
  std::u16string_view input_string16(reinterpret_cast<char16_t*>(input.data()),
                                     actual_size_char16);
  escaped_string.clear();
  base::EscapeJSONString(input_string16, put_in_quotes, &escaped_string);

  return 0;
}
