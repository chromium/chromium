// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <string>
#include <string_view>

#include "base/base64.h"
#include "base/containers/span.h"
#include "base/strings/string_view_util.h"
#include "testing/libfuzzer/libfuzzer_base_wrappers.h"

DEFINE_LLVM_FUZZER_TEST_ONE_INPUT_SPAN(base::span<const uint8_t> data) {
  std::string decode_output;
  base::Base64Decode(base::as_string_view(data), &decode_output);
  return 0;
}
