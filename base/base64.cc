// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/base64.h"

#include <stddef.h>

#include "third_party/modp_b64/modp_b64.h"

namespace base {

std::string Base64Encode(span<const uint8_t> input) {
  std::string output;
  output.resize(modp_b64_encode_len(input.size()));  // makes room for null byte

  // modp_b64_encode_len() returns at least 1, so output[0] is safe to use.
  const size_t output_size = modp_b64_encode(
      &(output[0]), reinterpret_cast<const char*>(input.data()), input.size());

  output.resize(output_size);
  return output;
}

void Base64Encode(StringPiece input, std::string* output) {
  *output = Base64Encode(base::as_bytes(base::make_span(input)));
}

bool Base64Decode(StringPiece input, std::string* output) {
  std::string temp;
  temp.resize(modp_b64_decode_len(input.size()));

  // does not null terminate result since result is binary data!
  size_t input_size = input.size();
  size_t output_size = modp_b64_decode(&(temp[0]), input.data(), input_size);
  if (output_size == MODP_B64_ERROR)
    return false;

  temp.resize(output_size);
  output->swap(temp);
  return true;
}

absl::optional<std::vector<uint8_t>> Base64Decode(StringPiece input) {
  std::vector<uint8_t> ret(modp_b64_decode_len(input.size()));

  size_t input_size = input.size();
  size_t output_size = modp_b64_decode(reinterpret_cast<char*>(ret.data()),
                                       input.data(), input_size);
  if (output_size == MODP_B64_ERROR)
    return absl::nullopt;

  ret.resize(output_size);
  return ret;
}

}  // namespace base
