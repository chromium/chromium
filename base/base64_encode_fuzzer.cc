// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <string>
#include <string_view>

#include "base/base64.h"
#include "base/check_op.h"

// Encode some random data, and then decode it.
extern "C" int LLVMFuzzerTestOneInput(const uint8_t* data_ptr, size_t size) {
  // SAFETY: libfuzzer provides a valid pointer and size pair.
  auto data = UNSAFE_BUFFERS(base::span(data_ptr, size));
  std::string_view data_string = base::as_string_view(data);

  const std::string encode_output = base::Base64Encode(data);
  std::string decode_output;
  CHECK(base::Base64Decode(encode_output, &decode_output));
  CHECK_EQ(data_string, decode_output);

  // Also run the std::string_view variant and check that it gives the same
  // results.
  CHECK_EQ(encode_output, base::Base64Encode(data_string));

  return 0;
}
