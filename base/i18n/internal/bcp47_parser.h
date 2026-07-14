// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// This is a compile-time parser for the BCP47 standard [RFC 5646]:
// https://www.rfc-editor.org/info/rfc5646/
//
// This is for internal to //base/i18n usage only by, more specifically to
// support construction of `LanguageTag`s at compile-time. The parser function
// provided here splits a BCP47 tag in its subtags representing <language>,
// <script>, <region>, <variants>, <extensions> and <private-use>.
//
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

// Singleton subtag: Single alphanumerics; "x" reserved for private use.
//  singleton     = DIGIT               ; 0 - 9
//                / %x41-57             ; A - W
//                / %x59-5A             ; Y - Z
//                / %x61-77             ; a - w
//                / %x79-7A             ; y - z
constexpr bool IsExtensionSingleton(std::string_view subtag) {
  return subtag.size() == 1 && subtag != "x" && subtag != "X" &&
         VerifyAsciiAlphanumeric(subtag);
}

// extension     = singleton 1*("-" (2*8alphanum))
constexpr bool IsExtensionSubtag(std::string_view subtag) {
  return subtag.size() >= 2 && subtag.size() <= 8 &&
         VerifyAsciiAlphanumeric(subtag);
}

// privateuse    = "x" 1*("-" (1*8alphanum))
constexpr bool IsPrivateUseSubtag(std::string_view subtag) {
  return subtag.size() >= 1 && subtag.size() <= 8 &&
         VerifyAsciiAlphanumeric(subtag);
}

// Assumes that `subtags` is at the point where extensions can be consumed,
// i.e. the subtags that come before extensions have already been consumed.
// It checks that the first subtag is a singleton (a subtag of length 1) and
// then consumes all the subtags after that until it reaches a subtag that is
// not a extension subtag (see the function `IsExtensionSubtag` for more info).
// It does that repeatedly as there can be multiple extensions in a BCP47 tag.
// There is also two cases where the parsing would fail (std::nullopt is
// returned):
// - Repeated singleton: there are more than one extension with the same
// singleton, this is not allowed by the BCP47 standard.
// - Empty extension: if the singleton is not followed by any valid extension
// subtag. This is also not allowed by the standard.
constexpr std::optional<
    std::vector<std::pair<char, std::vector<std::string_view>>>>
ParseBcp47Extensions(base::span<const std::string_view>& subtags) {
  std::vector<std::pair<char, std::vector<std::string_view>>> result;
  std::vector<char> seen_singletons;
  while (!subtags.empty() && IsExtensionSingleton(subtags.front())) {
    char singleton = base::ToLowerASCII(subtags.take_first_elem().front());
    // There cannot be two extensions with the same singleton in a language tag.
    if (std::ranges::find(seen_singletons, singleton) !=
        seen_singletons.end()) {
      return std::nullopt;
    }

    // Takes only the first char in `singleton` with .front().
    seen_singletons.push_back(singleton);
    std::vector<std::string_view> extension_subtags;
    while (!subtags.empty() && IsExtensionSubtag(subtags.front())) {
      extension_subtags.push_back(subtags.take_first_elem());
    }
    // Every BCP47 extension has to have at least one subtag, i.e. it is formed
    // by a singleton subtag (a subtag of length 1) followed one or more subtags
    // of length between 2 and 8.
    if (extension_subtags.empty()) {
      return std::nullopt;
    }
    result.emplace_back(singleton, std::move(extension_subtags));
  }

  return result;
}

// Assumes that the first subtag is the "x" singleton subtag. It parses all the
// following subtags by checking whether they are valid private-use subtags (see
// the `IsPrivateUseSubtag` function for more details). If the "x" singleton is
// not followed by any valid subtag, parsing fails (std::nullopt is returned) as
// this is not allowed by the BCP47 standard.
constexpr std::optional<std::vector<std::string_view>> ParseBcp47PrivateUse(
    base::span<const std::string_view>& subtags) {
  std::vector<std::string_view> private_use;
  if (subtags.empty()) {
    return private_use;
  }
  if (subtags.front() != "x" && subtags.front() != "X") {
    return private_use;
  }
  // Private-use subtags parsing.
  subtags.take_first_elem();
  // Having only the singleton "x" not followed by any subtags is not allowed.
  if (subtags.empty()) {
    return std::nullopt;
  }
  while (!subtags.empty() && IsPrivateUseSubtag(subtags.front())) {
    private_use.push_back(subtags.take_first_elem());
  }
  return private_use;
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
  // See the comments in `IsExtensionSingleton` and `IsExtensionSubtag`.
  std::vector<std::pair<char, std::vector<std::string_view>>> extensions;
  // See the comments in `IsPrivateUseSubtag`.
  std::vector<std::string_view> private_use;
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

  std::optional<std::vector<std::pair<char, std::vector<std::string_view>>>>
      extensions = ParseBcp47Extensions(subtags);
  if (!extensions.has_value()) {
    return std::nullopt;
  }
  parsed_tag.extensions = *std::move(extensions);
  std::optional<std::vector<std::string_view>> private_use =
      ParseBcp47PrivateUse(subtags);
  if (!private_use.has_value()) {
    return std::nullopt;
  }
  parsed_tag.private_use = *std::move(private_use);

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
      tag, "-", base::KEEP_WHITESPACE, base::SPLIT_WANT_ALL);
  return ParseBcp47Tag(base::span<const std::string_view>(subtags));
}

}  // namespace base::i18n_internal

#endif  // BASE_I18N_INTERNAL_BCP47_PARSER_H_
