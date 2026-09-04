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
#include "base/containers/flat_map.h"
#include "base/containers/span.h"
#include "base/i18n/internal/bcp47_known_subtags.h"
#include "base/i18n/internal/bcp47_subtags_reader.h"
#include "base/strings/string_util.h"

namespace base::i18n_internal {

// Assumes that `subtags` is at the point where extensions can be consumed,
// i.e. the subtags that come before extensions have already been consumed.
// It checks that the first subtag is a singleton (a subtag of length 1) and
// then consumes all the subtags after that until it reaches a subtag that is
// not a extension subtag (see the function `IsExtensionSubtag` for more info).
// It does that repeatedly as there can be multiple extensions in a BCP47 tag.
// There are also two cases where the parsing would fail (std::nullopt is
// returned):
// - Repeated singleton: there are more than one extension with the same
// singleton, this is not allowed by the BCP47 standard.
// - Empty extension: if the singleton is not followed by any valid extension
// subtag. This is also not allowed by the standard.
constexpr std::optional<base::flat_map<char, std::vector<std::string_view>>>
ParseBcp47Extensions(SubtagsReader& subtags) {
  base::flat_map<char, std::vector<std::string_view>> result;
  std::string_view singleton;
  while (!(singleton = subtags.Read(SubtagsReader::Type::kExtensionSingleton))
              .empty()) {
    char normalized_singleton = base::ToLowerASCII(singleton.front());
    // There cannot be two extensions with the same singleton in a language tag.
    if (result.contains(normalized_singleton)) {
      return std::nullopt;
    }

    if (result.contains(normalized_singleton)) {
      return std::nullopt;
    }

    std::vector<std::string_view> extension_subtags;
    std::string_view subtag;
    while (!(subtag = subtags.Read(SubtagsReader::Type::kExtensionSubtag))
                .empty()) {
      extension_subtags.push_back(subtag);
    }
    result[normalized_singleton] = extension_subtags;
  }

  return result;
}

// Assumes that the first subtag is the "x" singleton subtag. It parses all the
// following subtags by checking whether they are valid private-use subtags (see
// the `IsPrivateUseSubtag` function for more details). If the "x" singleton is
// not followed by any valid subtag, parsing fails (std::nullopt is returned) as
// this is not allowed by the BCP47 standard.
constexpr std::optional<std::vector<std::string_view>> ParseBcp47PrivateUse(
    SubtagsReader& subtags) {
  if (subtags.Read(SubtagsReader::Type::kPrivateUseSingleton).empty()) {
    return std::vector<std::string_view>();
  }
  // Private-use subtags parsing.
  std::vector<std::string_view> private_use;
  std::string_view subtag;
  while (!(subtag = subtags.Read(SubtagsReader::Type::kPrivateUseSubtag))
              .empty()) {
    private_use.push_back(subtag);
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
  base::flat_map<char, std::vector<std::string_view>> extensions;
  // See the comments in `IsPrivateUseSubtag`.
  std::vector<std::string_view> private_use;
};

// Returns true if all subtags in `parsed_tag` are known in Chromium.
constexpr bool AreSubtagsKnown(const ParsedBcp47Tag& parsed_tag) {
  if (!parsed_tag.extensions.empty()) {
    return false;
  }
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
// This does not support extended language subtags.
// Currently supports:
// language-tag =
//   language["-"script]["-"region]*("-"variant)*["-"extensions]["-"private_use]
constexpr std::optional<ParsedBcp47Tag> ParseBcp47Tag(SubtagsReader subtags) {
  ParsedBcp47Tag parsed_tag;
  parsed_tag.language = subtags.Read(SubtagsReader::Type::kLanguage);
  if (parsed_tag.language.empty()) {
    return std::nullopt;
  }

  parsed_tag.script = subtags.Read(SubtagsReader::Type::kScript);
  parsed_tag.region = subtags.Read(SubtagsReader::Type::kRegion);
  std::string_view variant;
  while (!(variant = subtags.Read(SubtagsReader::Type::kVariant)).empty()) {
    parsed_tag.variants.push_back(variant);
  }

  std::optional<base::flat_map<char, std::vector<std::string_view>>>
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
  if (subtags.HasError() || !subtags.IsDone()) {
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
  return ParseBcp47Tag(SubtagsReader(tag));
}

// Reconstructs the BCP47 tag's individual subtags and hyphen separators from
// the parsed tag representation into a vector of string_views. This is useful
// for reconstructing or modifying parts of a BCP47 tag before re-assembling it.
constexpr std::vector<std::string_view> GetBcp47TagPieces(
    const ParsedBcp47Tag& tag) {
  std::vector<std::string_view> result{tag.language};
  if (!tag.script.empty()) {
    result.push_back("-");
    result.push_back(tag.script);
  }
  if (!tag.region.empty()) {
    result.push_back("-");
    result.push_back(tag.region);
  }
  if (!tag.variants.empty()) {
    for (const std::string_view& variant : tag.variants) {
      result.push_back("-");
      result.push_back(variant);
    }
  }
  if (!tag.extensions.empty()) {
    for (const auto& extension : tag.extensions) {
      result.push_back("-");
      result.emplace_back(&extension.first, 1);
      for (const std::string_view& subtag : extension.second) {
        result.push_back("-");
        result.push_back(subtag);
      }
    }
  }
  if (!tag.private_use.empty()) {
    result.push_back("-");
    result.push_back("x");
    for (const std::string_view& subtag : tag.private_use) {
      result.push_back("-");
      result.push_back(subtag);
    }
  }
  return result;
}

}  // namespace base::i18n_internal

#endif  // BASE_I18N_INTERNAL_BCP47_PARSER_H_
