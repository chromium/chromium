// Copyright 2011 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef BASE_BASE64_H_
#define BASE_BASE64_H_

#include <stdint.h>

#include <string>
#include <vector>

#include "base/base_export.h"
#include "base/containers/span.h"
#include "base/strings/string_piece.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

namespace base {

// Encodes the input binary data in base64.
BASE_EXPORT std::string Base64Encode(span<const uint8_t> input);

// Encodes the input binary data in base64 and appends it to the output.
BASE_EXPORT void Base64EncodeAppend(span<const uint8_t> input,
                                    std::string* output);

// Encodes the input string in base64.
BASE_EXPORT void Base64Encode(StringPiece input, std::string* output);

// Decodes the base64 input string.  Returns true if successful and false
// otherwise. The output string is only modified if successful. The decoding can
// be done in-place.
enum class Base64DecodePolicy {
  // Input should match the output format of Base64Encode. i.e.
  // - Input length should be divisible by 4
  // - Maximum of 2 padding characters
  // - No non-base64 characters.
  kStrict,

  // Matches https://infra.spec.whatwg.org/#forgiving-base64-decode.
  // - Removes all ascii whitespace
  // - Maximum of 2 padding characters
  // - Allows input length not divisible by 4 if no padding chars are added.
  kForgiving,
};
BASE_EXPORT bool Base64Decode(
    StringPiece input,
    std::string* output,
    Base64DecodePolicy policy = Base64DecodePolicy::kStrict);

// Decodes the base64 input string. Returns `absl::nullopt` if unsuccessful.
BASE_EXPORT absl::optional<std::vector<uint8_t>> Base64Decode(
    StringPiece input);

}  // namespace base

#endif  // BASE_BASE64_H_
