// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <fuzzer/FuzzedDataProvider.h>
#include <stdint.h>

#include <string>
#include <tuple>

#include "base/base64url.h"
#include "base/check.h"
#include "base/check_op.h"

namespace base {

extern "C" int LLVMFuzzerTestOneInput(const uint8_t* data, size_t size) {
  FuzzedDataProvider provider(data, size);

  // Test encoding of a random plaintext.
  std::string plaintext = provider.ConsumeRandomLengthString();
  Base64UrlEncodePolicy encode_policy =
      provider.ConsumeBool() ? Base64UrlEncodePolicy::INCLUDE_PADDING
                             : Base64UrlEncodePolicy::OMIT_PADDING;
  std::string encoded;
  Base64UrlEncode(plaintext, encode_policy, &encoded);

  // Check decoding of the above gives the original text.
  std::string decoded;
  CHECK(Base64UrlDecode(encoded,
                        encode_policy == Base64UrlEncodePolicy::INCLUDE_PADDING
                            ? Base64UrlDecodePolicy::REQUIRE_PADDING
                            : Base64UrlDecodePolicy::DISALLOW_PADDING,
                        &decoded));
  CHECK_EQ(decoded, plaintext);
  // Same result should be when ignoring padding.
  decoded.clear();
  CHECK(Base64UrlDecode(encoded, Base64UrlDecodePolicy::IGNORE_PADDING,
                        &decoded));
  CHECK_EQ(decoded, plaintext);

  // Additionally test decoding of a random input.
  std::string decoding_input = provider.ConsumeRandomLengthString();
  Base64UrlDecodePolicy decode_policy;
  switch (provider.ConsumeIntegralInRange<int>(0, 2)) {
    case 0:
      decode_policy = Base64UrlDecodePolicy::REQUIRE_PADDING;
      break;
    case 1:
      decode_policy = Base64UrlDecodePolicy::IGNORE_PADDING;
      break;
    case 2:
      decode_policy = Base64UrlDecodePolicy::DISALLOW_PADDING;
      break;
  }
  std::ignore = Base64UrlDecode(decoding_input, decode_policy, &decoded);

  return 0;
}

}  // namespace base
