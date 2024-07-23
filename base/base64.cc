// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/base64.h"

#include <stddef.h>

#include <string_view>

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

  const size_t after_size =
      base::CheckAdd(encode_data_len, output->size()).ValueOrDie();
  output->resize(after_size);

  span<const char> read = base::as_chars(input);
  span<char> write = base::span(*output).last(encode_data_len);

  const size_t written_size = modp_b64_encode_data(
      write.data(),  // This must point to `encode_data_len` many chars.
      read.data(), read.size());
  // If this failed it would indicate we wrote OOB or left bytes uninitialized.
  // It's possible for this to be elided by the compiler, since writing OOB is
  // UB.
  CHECK_EQ(written_size, write.size());
}

std::string Base64Encode(std::string_view input) {
  return Base64Encode(base::as_byte_span(input));
}

bool Base64Decode(std::string_view input,
                  std::string* output,
                  Base64DecodePolicy policy) {
  std::string decode_buf;
  decode_buf.resize(modp_b64_decode_len(input.size()));

  // Does not NUL-terminate result since result is binary data!
  size_t written_size = modp_b64_decode(decode_buf.data(), input.data(),
                                        input.size(), GetModpPolicy(policy));

  // Forgiving mode requires whitespace to be stripped prior to decoding.
  // We don't do that in the above code to ensure that the "happy path" of
  // input without whitespace is as fast as possible. Since whitespace in input
  // will always cause `modp_b64_decode` to fail, just handle whitespace
  // stripping on failure. This is not much slower than just scanning for
  // whitespace first, even for input with whitespace.
  if (written_size == MODP_B64_ERROR &&
      policy == Base64DecodePolicy::kForgiving) {
    // We could use `output` here to avoid an allocation when decoding is done
    // in-place, but it violates the API contract that `output` is only modified
    // on success.
    std::string input_without_whitespace;
    RemoveChars(input, kInfraAsciiWhitespace, &input_without_whitespace);
    // This means that the required size to decode is at most what was needed
    // above, which means `decode_buf` will fit the decoded bytes at its current
    // size and we don't need to call `modp_b64_decode_len()` again.
    CHECK_LE(input_without_whitespace.size(), input.size());
    written_size =
        modp_b64_decode(decode_buf.data(), input_without_whitespace.data(),
                        input_without_whitespace.size(), GetModpPolicy(policy));
  }

  if (written_size == MODP_B64_ERROR) {
    return false;
  }

  // If this failed it would indicate we wrote OOB. It's possible for this to be
  // elided by the compiler, since writing OOB is UB.
  CHECK_LE(written_size, decode_buf.size());

  // Shrinks the buffer and makes it NUL-terminated.
  decode_buf.resize(written_size);
  *output = std::move(decode_buf);
  return true;
}

std::optional<std::vector<uint8_t>> Base64Decode(std::string_view input) {
  std::vector<uint8_t> write_buf(modp_b64_decode_len(input.size()));
  span<char> write = base::as_writable_chars(base::span(write_buf));

  size_t written_size =
      modp_b64_decode(write.data(), input.data(), input.size());
  if (written_size == MODP_B64_ERROR) {
    return std::nullopt;
  }

  // If this failed it would indicate we wrote OOB. It's possible for this to be
  // elided by the compiler, since writing OOB is UB.
  CHECK_LE(written_size, write.size());

  write_buf.resize(written_size);
  return write_buf;
}

}  // namespace base
