// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef BASE_I18N_INTERNAL_BCP47_PARSER_H_
#define BASE_I18N_INTERNAL_BCP47_PARSER_H_

#include <algorithm>
#include <string_view>
#include <vector>

#include "base/compiler_specific.h"
#include "base/containers/span.h"
#include "base/i18n/internal/bcp47_known_subtags.h"
#include "base/strings/string_split.h"
#include "base/strings/string_util.h"

namespace base::i18n_internal {

constexpr bool VerifyAsciiAlphanumeric(std::string_view str) {
  return std::ranges::all_of(
      str, [](char c) { return base::IsAsciiAlphaNumeric(c); });
}

constexpr bool VerifyAsciiNumeric(std::string_view str) {
  return std::ranges::all_of(str, [](char c) { return base::IsAsciiDigit(c); });
}

constexpr bool VerifyAsciiAlpha(std::string_view str) {
  return std::ranges::all_of(str, [](char c) { return base::IsAsciiAlpha(c); });
}

// Primary language subtag: 2-3 alpha characters.
// RFC 5646 Section 2.2.1.
//  language      = 2*3ALPHA
//                  ["-" extlang]
//                  / 4ALPHA
//                  / 5*8ALPHA
// Note: Extended language subtags (extlang) are not supported.
// Note: 4ALPHA language subtags are not supported.
// Note: 5*8ALPHA language subtags are not supported.
constexpr bool IsLanguageSubtag(std::string_view subtag) {
  return VerifyAsciiAlpha(subtag) && subtag.size() >= 2 && subtag.size() <= 3;
}

// Script subtag: 4 alpha characters.
// RFC 5646 Section 2.2.3.
//  script        = 4ALPHA
constexpr bool IsScriptSubtag(std::string_view subtag) {
  return VerifyAsciiAlpha(subtag) && subtag.size() == 4;
}

// Region subtag: 2 alpha characters or 3 digits.
// RFC 5646 Section 2.2.4.
//  region        = 2ALPHA
//                / 3DIGIT
constexpr bool IsRegionSubtag(std::string_view subtag) {
  return (subtag.size() == 2 && VerifyAsciiAlpha(subtag)) ||
         (subtag.size() == 3 && VerifyAsciiNumeric(subtag));
}

// Variant subtag: 5-8 alphanumeric characters, or 4 characters starting with a
// digit. RFC 5646 Section 2.2.5.
//  variant       = 5*8alphanum
//                 / (DIGIT 3alphanum)
constexpr bool IsVariantSubtag(std::string_view subtag) {
  if (subtag.size() >= 5 && subtag.size() <= 8) {
    return VerifyAsciiAlphanumeric(subtag);
  }
  if (subtag.size() == 4) {
    return base::IsAsciiDigit(subtag[0]) && VerifyAsciiAlphanumeric(subtag);
  }
  return false;
}

// The parsed BCP47 tag. It is a view on the actual input string.
struct ParsedBcp47Tag {
  // See the comments in `IsLanguageSubtag`.
  std::string_view language;
  // See the comments in `IsScriptSubtag`.
  std::string_view script;
  // See the comments in `IsRegionSubtag`.
  std::string_view region;
  // See the comments in `IsVariantSubtag`.
  std::vector<std::string_view> variants;
};

// Returns true if all subtags in `parsed_tag` are known in Chromium.
constexpr bool AreSubtagsKnown(const ParsedBcp47Tag& parsed_tag) {
  if (!IsKnownLanguageSubtag(parsed_tag.language)) {
    return false;
  }
  if (!parsed_tag.script.empty() && !IsKnownScriptSubtag(parsed_tag.script)) {
    return false;
  }
  if (!parsed_tag.region.empty() && !IsKnownRegionSubtag(parsed_tag.region)) {
    return false;
  }
  return std::ranges::all_of(parsed_tag.variants, IsKnownVariantSubtag);
}

// Parses a language tag according to the ABNF in RFC 5646 Section 2.1.
// This does not support extended language subtags or extensions.
// Currently supports:
// language-tag = language["-"script]["-"region]*("-"variant)
constexpr std::optional<ParsedBcp47Tag> ParseBcp47Tag(
    base::span<const std::string_view> subtags) {
  if (subtags.empty()) {
    return std::nullopt;
  }
  ParsedBcp47Tag parsed_tag;
  if (!IsLanguageSubtag(subtags[0])) {
    return std::nullopt;
  }
  parsed_tag.language = subtags.take_first_elem();
  if (!subtags.empty() && IsScriptSubtag(subtags.front())) {
    parsed_tag.script = subtags.take_first_elem();
  }
  if (!subtags.empty() && IsRegionSubtag(subtags.front())) {
    parsed_tag.region = subtags.take_first_elem();
  }
  while (!subtags.empty() && IsVariantSubtag(subtags.front())) {
    parsed_tag.variants.push_back(subtags.take_first_elem());
  }
  // If there are remaining subtags, it means that the input is malformed.
  if (!subtags.empty()) {
    return std::nullopt;
  }
  return parsed_tag;
}

// Parses a language tag according to the ABNF in RFC 5646 Section 2.1.
// Currently supports:
// language-tag = language["-"script]["-"region]*("-"variant)
// See the comments in the functions above for the supported format for each
// subtag.
constexpr std::optional<ParsedBcp47Tag> ParseBcp47Tag(
    std::string_view tag LIFETIME_BOUND) {
  if (tag.empty()) {
    return std::nullopt;
  }
  std::vector<std::string_view> subtags = base::SplitStringPiece(
      tag, "-", base::TRIM_WHITESPACE, base::SPLIT_WANT_ALL);
  return ParseBcp47Tag(base::span<const std::string_view>(subtags));
}

}  // namespace base::i18n_internal

#endif  // BASE_I18N_INTERNAL_BCP47_PARSER_H_
