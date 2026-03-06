// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/strings/string_tokenizer.h"

#include <stddef.h>
#include <stdint.h>

#include <string>
#include <string_view>
#include <tuple>

#include "base/containers/span.h"
#include "testing/libfuzzer/libfuzzer_base_wrappers.h"

void GetAllTokens(base::StringTokenizer& t) {
  while (t.GetNext()) {
    std::ignore = t.token();
  }
}

// Entry point for LibFuzzer.
DEFINE_LLVM_FUZZER_TEST_ONE_INPUT_SPAN(base::span<const uint8_t> data) {
  if (data.size() < sizeof(size_t) + 1) {
    return 0;
  }

  // Calculate pattern size based on remaining bytes, otherwise fuzzing is
  // inefficient with bailouts in most cases.
  size_t pattern_size;
  base::byte_span_from_ref(pattern_size)
      .copy_from_nonoverlapping(data.take_first<sizeof(size_t)>());
  pattern_size %= data.size();

  auto [pattern_span, input_span] = data.split_at(pattern_size);

  const std::string pattern(std::from_range, pattern_span);
  const std::string input(std::from_range, input_span);

  // Allow quote_chars and options to be set. Otherwise full coverage
  // won't be possible since IsQuote, FullGetNext and other functions
  // won't be called.
  for (bool return_delims : {false, true}) {
    for (bool return_empty_strings : {false, true}) {
      int options = 0;
      if (return_delims) {
        options |= base::StringTokenizer::RETURN_DELIMS;
      }
      if (return_empty_strings) {
        options |= base::StringTokenizer::RETURN_EMPTY_TOKENS;
      }

      base::StringTokenizer t(input, pattern);
      t.set_options(options);
      GetAllTokens(t);

      base::StringTokenizer t_quote(input, pattern);
      t_quote.set_quote_chars("\"");
      t_quote.set_options(options);
      GetAllTokens(t_quote);
    }
  }

  return 0;
}
