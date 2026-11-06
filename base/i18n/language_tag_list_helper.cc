// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/i18n/language_tag_list_helper.h"

#include <optional>
#include <utility>

#include "base/containers/to_vector.h"
#include "base/i18n/tag_converters.h"
#include "base/strings/string_split.h"
#include "base/strings/string_util.h"

namespace base::i18n {

namespace {

constexpr std::string_view kLanguageTagListSeparator = ",";

// Parses each string of `tag_strings` into a `LanguageTag`, and returns the
// valid ones for which `filter` returns true, in order.
template <typename StringRange>
std::vector<LanguageTag> ParseLanguageTags(
    const StringRange& tag_strings,
    base::FunctionRef<bool(const LanguageTag&)> filter) {
  std::vector<LanguageTag> tags;
  for (std::string_view tag_string : tag_strings) {
    std::optional<LanguageTag> tag = GetLanguageTagFromString(tag_string);
    if (tag && filter(*tag)) {
      tags.push_back(std::move(*tag));
    }
  }
  return tags;
}

}  // namespace

std::vector<LanguageTag> GetLanguageTagListFromString(
    std::string_view comma_separated_tags) {
  return GetLanguageTagListFromString(comma_separated_tags,
                                      [](const LanguageTag&) { return true; });
}

std::vector<LanguageTag> GetLanguageTagListFromString(
    std::string_view comma_separated_tags,
    base::FunctionRef<bool(const LanguageTag&)> filter) {
  return ParseLanguageTags(
      base::SplitStringPiece(comma_separated_tags, kLanguageTagListSeparator,
                             base::TRIM_WHITESPACE, base::SPLIT_WANT_NONEMPTY),
      filter);
}

std::vector<LanguageTag> GetLanguageTagListFromStringList(
    base::span<const std::string> tag_strings) {
  return GetLanguageTagListFromStringList(
      tag_strings, [](const LanguageTag&) { return true; });
}

std::vector<LanguageTag> GetLanguageTagListFromStringList(
    base::span<const std::string> tag_strings,
    base::FunctionRef<bool(const LanguageTag&)> filter) {
  return ParseLanguageTags(tag_strings, filter);
}

std::string LanguageTagListToString(base::span<const LanguageTag> tags) {
  return base::JoinString(base::ToVector(tags, &LanguageTag::tag_string),
                          kLanguageTagListSeparator);
}

}  // namespace base::i18n
