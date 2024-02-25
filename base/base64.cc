// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/base64.h"

#include <stddef.h>

#include "base/check.h"
#include "base/numerics/checked_math.h"
#include "base/strings/string_util.h"
#include "third_party/modp_b64/modp_b64.h"

namespace base {

namespace {

ModpDecodePolicy GetModpPolicy(Base64DecodePolicy policy) {
  switch (policy) {
    case Base64DecodePolicy::kStrict:
      return ModpDecodePolicy::kStrict;
    case Base64DecodePolicy::kForgiving:
      return ModpDecodePolicy::kForgiving;
  }
}

}  // namespace

std::string Base64Encode(span<const uint8_t> input) {
  std::string output;
  Base64EncodeAppend(input, &output);
  return output;
}

void Base64EncodeAppend(span<const uint8_t> input, std::string* output) {
  // Ensure `modp_b64_encode_data_len` will not overflow.
  CHECK_LE(input.size(), MODP_B64_MAX_INPUT_LEN);
  size_t encode_data_len = modp_b64_encode_data_len(input.size());

  size_t prefix_len = output->size();
  output->resize(base::CheckAdd(encode_data_len, prefix_len).ValueOrDie());

  const size_t output_size = modp_b64_encode_data(
      output->data() + prefix_len, reinterpret_cast<const char*>(input.data()),
      input.size());
  CHECK_EQ(output->size(), prefix_len + output_size);
}

std::string Base64Encode(StringPiece input) {
  return Base64Encode(base::as_byte_span(input));
}

bool Base64Decode(StringPiece input,
                  std::string* output,
                  Base64DecodePolicy policy) {
  std::string temp;
  temp.resize(modp_b64_decode_len(input.size()));

  // does not null terminate result since result is binary data!
  size_t input_size = input.size();
  size_t output_size = modp_b64_decode(&(temp[0]), input.data(), input_size,
                                       GetModpPolicy(policy));

  // Forgiving mode requires whitespace to be stripped prior to decoding.
  // We don't do that in the above code to ensure that the "happy path" of
  // input without whitespace is as fast as possible. Since whitespace in input
  // will always cause `modp_b64_decode` to fail, just handle whitespace
  // stripping on failure. This is not much slower than just scanning for
  // whitespace first, even for input with whitespace.
  if (output_size == MODP_B64_ERROR &&
      policy == Base64DecodePolicy::kForgiving) {
    // We could use `output` here to avoid an allocation when decoding is done
    // in-place, but it violates the API contract that `output` is only modified
    // on success.
    std::string input_without_whitespace;
    RemoveChars(input, kInfraAsciiWhitespace, &input_without_whitespace);
    output_size =
        modp_b64_decode(&(temp[0]), input_without_whitespace.data(),
                        input_without_whitespace.size(), GetModpPolicy(policy));
  }

  if (output_size == MODP_B64_ERROR)
    return false;

  temp.resize(output_size);
  output->swap(temp);
  return true;
}

std::optional<std::vector<uint8_t>> Base64Decode(StringPiece input) {
  std::vector<uint8_t> ret(modp_b64_decode_len(input.size()));

  size_t input_size = input.size();
  size_t output_size = modp_b64_decode(reinterpret_cast<char*>(ret.data()),
                                       input.data(), input_size);
  if (output_size == MODP_B64_ERROR)
    return std::nullopt;

  ret.resize(output_size);
  return ret;
}

}  // namespace base
