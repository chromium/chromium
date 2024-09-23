// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/strings/escape.h"

#include <string_view>

// Prevent the optimizer from optimizing away a function call by "using" the
// result.
//
// TODO(crbug.com/40243629): Replace this with a more general solution.
void UseResult(const std::string& input) {
  volatile char c;
  if (input.length() > 0)
    c = input[0];
  (void)c;
}

// Entry point for LibFuzzer.
extern "C" int LLVMFuzzerTestOneInput(const uint8_t* data, size_t size) {
  std::string_view data_string(reinterpret_cast<const char*>(data), size);

  UseResult(base::EscapeQueryParamValue(data_string, /*use_plus=*/false));
  UseResult(base::EscapeQueryParamValue(data_string, /*use_plus=*/true));

  return 0;
}
