// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <string>
#include <string_view>

#include "base/base64.h"
#include "base/check_op.h"
#include "base/containers/span.h"
#include "base/features.h"
#include "base/strings/string_view_util.h"
#include "base/test/scoped_feature_list.h"
#include "testing/libfuzzer/libfuzzer_base_wrappers.h"

namespace {

void EncodeDecode(base::span<const uint8_t> data) {
  std::string_view data_string = base::as_string_view(data);

  const std::string encode_output = base::Base64Encode(data);
  std::string decode_output;
  CHECK(base::Base64Decode(encode_output, &decode_output));
  CHECK_EQ(data_string, decode_output);

  // Also run the std::string_view variant and check that it gives the same
  // results.
  CHECK_EQ(encode_output, base::Base64Encode(data_string));
}

}  // namespace

// Encode some random data, and then decode it.
DEFINE_LLVM_FUZZER_TEST_ONE_INPUT_SPAN(const base::span<const uint8_t> data) {
  EncodeDecode(data);

  {
    base::test::ScopedFeatureList enable_simdutf(
        base::features::kSimdutfBase64Encode);
    EncodeDecode(data);
  }
  return 0;
}
