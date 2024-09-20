// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/base64url.h"

#include <stddef.h>

#include <string_view>

#include "base/base64.h"
#include "base/numerics/safe_math.h"
#include "base/strings/string_util.h"
#include "third_party/modp_b64/modp_b64.h"

namespace base {

namespace {

const char kPaddingChar = '=';

// Base64url maps {+, /} to {-, _} in order for the encoded content to be safe
// to use in a URL. These characters will be translated by this implementation.
const char kBase64Chars[] = "+/";
const char kBase64UrlSafeChars[] = "-_";

class StringViewOrString {
 public:
  explicit StringViewOrString(std::string_view piece) : piece_(piece) {}
  explicit StringViewOrString(std::string str) : str_(std::move(str)) {}

  std::string_view get() const {
    if (str_) {
      return *str_;
    }
    return piece_;
  }

 private:
  const std::optional<std::string> str_;
  const std::string_view piece_;
};

// Converts the base64url `input` into a plain base64 string.
std::optional<StringViewOrString> Base64UrlToBase64(
    std::string_view input,
    Base64UrlDecodePolicy policy) {
  // Characters outside of the base64url alphabet are disallowed, which includes
  // the {+, /} characters found in the conventional base64 alphabet.
  if (input.find_first_of(kBase64Chars) != std::string::npos)
    return std::nullopt;

  const size_t required_padding_characters = input.size() % 4;
  const bool needs_replacement =
      input.find_first_of(kBase64UrlSafeChars) != std::string::npos;

  switch (policy) {
    case Base64UrlDecodePolicy::REQUIRE_PADDING:
      // Fail if the required padding is not included in |input|.
      if (required_padding_characters > 0)
        return std::nullopt;
      break;
    case Base64UrlDecodePolicy::IGNORE_PADDING:
      // Missing padding will be silently appended.
      break;
    case Base64UrlDecodePolicy::DISALLOW_PADDING:
      // Fail if padding characters are included in |input|.
      if (input.find_first_of(kPaddingChar) != std::string::npos)
        return std::nullopt;
      break;
  }

  if (required_padding_characters == 0 && !needs_replacement) {
    return StringViewOrString(input);
  }

  // If the string either needs replacement of URL-safe characters to normal
  // base64 ones, or additional padding, a copy of |input| needs to be made in
  // order to make these adjustments without side effects.
  CheckedNumeric<size_t> out_size = input.size();
  if (required_padding_characters > 0) {
    out_size += 4 - required_padding_characters;
  }

  std::string base64_input;
  base64_input.reserve(out_size.ValueOrDie());
  base64_input.append(input);

  // Substitute the base64url URL-safe characters to their base64 equivalents.
  ReplaceChars(base64_input, "-", "+", &base64_input);
  ReplaceChars(base64_input, "_", "/", &base64_input);

  // Append the necessary padding characters.
  base64_input.resize(out_size.ValueOrDie(), '=');

  return StringViewOrString(std::move(base64_input));
}

}  // namespace

void Base64UrlEncode(span<const uint8_t> input,
                     Base64UrlEncodePolicy policy,
                     std::string* output) {
  *output = Base64Encode(input);

  ReplaceChars(*output, "+", "-", output);
  ReplaceChars(*output, "/", "_", output);

  switch (policy) {
    case Base64UrlEncodePolicy::INCLUDE_PADDING:
      // The padding included in |*output| will not be amended.
      break;
    case Base64UrlEncodePolicy::OMIT_PADDING:
      // The padding included in |*output| will be removed.
      const size_t last_non_padding_pos =
          output->find_last_not_of(kPaddingChar);
      if (last_non_padding_pos != std::string::npos) {
        output->resize(last_non_padding_pos + 1);
      }

      break;
  }
}

void Base64UrlEncode(std::string_view input,
                     Base64UrlEncodePolicy policy,
                     std::string* output) {
  Base64UrlEncode(base::as_byte_span(input), policy, output);
}

bool Base64UrlDecode(std::string_view input,
                     Base64UrlDecodePolicy policy,
                     std::string* output) {
  std::optional<StringViewOrString> base64_input =
      Base64UrlToBase64(input, policy);
  if (!base64_input) {
    return false;
  }
  return Base64Decode(base64_input->get(), output);
}

std::optional<std::vector<uint8_t>> Base64UrlDecode(
    std::string_view input,
    Base64UrlDecodePolicy policy) {
  std::optional<StringViewOrString> base64_input =
      Base64UrlToBase64(input, policy);
  if (!base64_input) {
    return std::nullopt;
  }
  return Base64Decode(base64_input->get());
}

}  // namespace base
