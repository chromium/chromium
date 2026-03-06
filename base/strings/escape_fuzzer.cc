// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/strings/escape.h"

#include <string>
#include <string_view>

#include "base/containers/span.h"
#include "base/strings/string_view_util.h"
#include "testing/libfuzzer/libfuzzer_base_wrappers.h"

// Prevent the optimizer from optimizing away a function call by "using" the
// result.
//
// TODO(crbug.com/40243629): Replace this with a more general solution.
void UseResult(const std::string& input) {
  volatile char c;
  if (input.length() > 0) {
    c = input[0];
  }
  (void)c;
}

// Entry point for LibFuzzer.
DEFINE_LLVM_FUZZER_TEST_ONE_INPUT_SPAN(base::span<const uint8_t> data) {
  const auto data_string = base::as_string_view(data);

  UseResult(base::EscapeQueryParamValue(data_string, /*use_plus=*/false));
  UseResult(base::EscapeQueryParamValue(data_string, /*use_plus=*/true));

  return 0;
}
