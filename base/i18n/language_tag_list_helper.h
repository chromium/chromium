// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef BASE_I18N_LANGUAGE_TAG_LIST_HELPER_H_
#define BASE_I18N_LANGUAGE_TAG_LIST_HELPER_H_

#include <string>
#include <string_view>
#include <vector>

#include "base/component_export.h"
#include "base/containers/span.h"
#include "base/functional/function_ref.h"
#include "base/i18n/language_tag.h"

// Helpers to convert between vectors of `LanguageTag`s and lists of language
// tags stored as strings, e.g. comma-separated strings such as
// "en-US,fr,zh-Hant-TW" (as stored in the accept languages pref).

namespace base::i18n {

// Parses `comma_separated_tags`, a list of language tags separated by commas,
// into a vector of `LanguageTag`s, preserving their order and duplicates.
// Whitespace surrounding each element is trimmed, and each element is parsed
// and canonicalized with `GetLanguageTagFromString()`. Empty and malformed
// elements are skipped.
//
// Example:
//   GetLanguageTagListFromString(" en_US,iw,,invalid---tag,fr ")
//   -> {"en-US", "he", "fr"}
COMPONENT_EXPORT(LANGUAGE_TAG)
std::vector<LanguageTag> GetLanguageTagListFromString(
    std::string_view comma_separated_tags);

// Same as above, but only keeps the language tags for which `filter` returns
// true. `filter` is invoked synchronously, once for every successfully parsed
// language tag, in list order.
//
// Example:
//   // Only keeps the language tags without a region subtag.
//   GetLanguageTagListFromString(
//       "en-US,fr,de-DE,ja", [](const LanguageTag& tag) {
//         return tag.region_subtag().empty();
//       });
//   -> {"fr", "ja"}
COMPONENT_EXPORT(LANGUAGE_TAG)
std::vector<LanguageTag> GetLanguageTagListFromString(
    std::string_view comma_separated_tags,
    base::FunctionRef<bool(const LanguageTag&)> filter);

// Parses `tag_strings`, a list of strings that each hold a single language
// tag, into a vector of `LanguageTag`s, preserving their order and duplicates.
// Each element is parsed and canonicalized as is with
// `GetLanguageTagFromString()`, and malformed elements are skipped.
//
// Example:
//   GetLanguageTagListFromStringList({"en_US", "iw", "invalid---tag", "fr"})
//   -> {"en-US", "he", "fr"}
COMPONENT_EXPORT(LANGUAGE_TAG)
std::vector<LanguageTag> GetLanguageTagListFromStringList(
    base::span<const std::string> tag_strings);

// Same as above, but only keeps the language tags for which `filter` returns
// true. `filter` is invoked synchronously, once for every successfully parsed
// language tag, in list order.
COMPONENT_EXPORT(LANGUAGE_TAG)
std::vector<LanguageTag> GetLanguageTagListFromStringList(
    base::span<const std::string> tag_strings,
    base::FunctionRef<bool(const LanguageTag&)> filter);

// Joins `tags` into a single string, separated by commas and without any
// whitespace. Returns an empty string if `tags` is empty. The result can be
// parsed back with `GetLanguageTagListFromString()`.
//
// Example:
//   LanguageTagListToString(
//       {GetKnownLanguageTag("en-US"), GetKnownLanguageTag("fr")})
//   -> "en-US,fr"
COMPONENT_EXPORT(LANGUAGE_TAG)
std::string LanguageTagListToString(base::span<const LanguageTag> tags);

}  // namespace base::i18n

#endif  // BASE_I18N_LANGUAGE_TAG_LIST_HELPER_H_
