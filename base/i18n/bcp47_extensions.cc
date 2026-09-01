// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/i18n/bcp47_extensions.h"

#include <algorithm>
#include <iterator>
#include <optional>
#include <ranges>
#include <string>
#include <string_view>
#include <utility>

#include "base/check.h"
#include "base/check_op.h"
#include "base/containers/flat_map.h"
#include "base/containers/span.h"
#include "base/i18n/language_tag.h"
#include "base/strings/strcat.h"
#include "base/strings/string_split.h"
#include "base/strings/string_util.h"
#include "base/types/pass_key.h"

namespace base::i18n {
namespace {

bool IsAllAlphaNumeric(std::string_view s) {
  return std::ranges::all_of(
      s, [](char c) { return base::IsAsciiAlphaNumeric(c); });
}

size_t FindNextKeyIndex(base::span<const std::string_view> subtags) {
  auto it = std::ranges::find_if(subtags, [](const std::string_view& subtag) {
    return subtag.size() == 2u;
  });
  return std::distance(subtags.begin(), it);
}

// Returns true if all items in subtags match the rule for either an attribute
// or a type: three to eight alphanumeric ascii chars.
bool VerifyTypeOrAttributeSubtags(base::span<const std::string_view> subtags) {
  return std::ranges::all_of(subtags, [](std::string_view subtag) {
    return subtag.size() >= 3 && subtag.size() <= 8 &&
           IsAllAlphaNumeric(subtag);
  });
}

std::string CanonicalizeTypeOrAttributeSubtags(
    base::span<const std::string_view> subtags) {
  std::vector<std::string> canonicalized;
  std::ranges::transform(
      subtags, std::back_inserter(canonicalized),
      [](std::string_view s) { return base::ToLowerASCII(s); });
  return base::JoinString(canonicalized, "-");
}

// Returns true if subtag matches the rule for a "key": exactly two alphanumeric
// ascii chars.
bool VerifyKeySubtag(std::string_view subtag) {
  return subtag.size() == 2 && IsAllAlphaNumeric(subtag);
}

// Adds `attributes_span` to `result` using `UnicodeExtension::AddAttribute`. It
// returns whether the attributes are valid. It stops as soon as the first
// non-valid attribute is seen. Even if it returns false, `result` might have
// been modified.
bool AddAttributes(base::span<const std::string_view> attributes_span,
                   UnicodeExtension& result) {
  for (std::string_view attribute : attributes_span) {
    if (!result.AddAttribute(attribute)) {
      return false;
    }
  }
  return true;
}

// Parses and adds `keywords_span` into result using
// `UnicodeExtension::SetKeyword`. It returns whether the input was valid
// stopping in the first non-valid entry. Even if it returns false, `result`
// might have been modified.
bool AddKeywords(base::span<const std::string_view> keywords_span,
                 UnicodeExtension& result) {
  // Keywords are a two-character key followed by zero or more types subtags.
  while (!keywords_span.empty()) {
    std::string_view key = keywords_span.take_first_elem();
    size_t next_key_index = FindNextKeyIndex(keywords_span);
    base::span<const std::string_view> types_subspan =
        keywords_span.take_first(next_key_index);

    if (!VerifyTypeOrAttributeSubtags(types_subspan)) {
      return false;
    }

    // According to RFC 6067, if a keyword appears more than once, only its
    // first definition is considered; subsequent occurrences are ignored.
    std::string canonical_key = base::ToLowerASCII(key);
    if (result.has_keyword(canonical_key)) {
      continue;
    }
    if (!result.SetKeyword(canonical_key,
                           base::JoinString(types_subspan, "-"))) {
      return false;
    }
  }
  return true;
}

}  // namespace

// static
std::optional<Extension> Extension::FromString(std::string_view extension) {
  if (extension.size() < 3u || !base::IsAsciiAlphaNumeric(extension[0]) ||
      extension[1] != '-') {
    return std::nullopt;
  }

  std::vector<std::string_view> subtags = base::SplitStringPiece(
      extension.substr(2), "-", base::KEEP_WHITESPACE, base::SPLIT_WANT_ALL);
  return FromSubtags(extension[0], subtags);
}

// static
std::optional<Extension> Extension::FromSubtags(
    char singleton,
    base::span<const std::string_view> subtags) {
  if (!base::IsAsciiAlphaNumeric(singleton)) {
    return std::nullopt;
  }
  if (subtags.empty()) {
    return std::nullopt;
  }
  Extension result(singleton);
  for (std::string_view subtag : subtags) {
    if (!result.AddSubtag(subtag)) {
      return std::nullopt;
    }
  }
  return result;
}

Extension::Extension(char singleton) : singleton_(singleton) {}
Extension::~Extension() = default;
Extension::Extension(const Extension&) = default;
Extension& Extension::operator=(const Extension&) = default;
Extension::Extension(Extension&&) = default;
Extension& Extension::operator=(Extension&&) = default;

bool Extension::AddSubtag(std::string_view subtag) {
  if (!VerifyTypeOrAttributeSubtags({subtag})) {
    return false;
  }
  subtags_.insert(CanonicalizeTypeOrAttributeSubtags({subtag}));
  return true;
}

std::string Extension::SubtagsString() const {
  return base::JoinString(base::span(subtags_), "-");
}

bool PrivateUseSubtags::AddSubtag(std::string_view subtag) {
  if (subtag.empty() || !IsAllAlphaNumeric(subtag)) {
    return false;
  }
  subtags_.emplace_back(subtag);
  return true;
}

std::string PrivateUseSubtags::SubtagsString() const {
  return base::JoinString(subtags_, "-");
}

PrivateUseSubtags::PrivateUseSubtags() = default;
PrivateUseSubtags::~PrivateUseSubtags() = default;
PrivateUseSubtags::PrivateUseSubtags(const PrivateUseSubtags&) = default;
PrivateUseSubtags& PrivateUseSubtags::operator=(const PrivateUseSubtags&) =
    default;
PrivateUseSubtags::PrivateUseSubtags(PrivateUseSubtags&&) = default;
PrivateUseSubtags& PrivateUseSubtags::operator=(PrivateUseSubtags&&) = default;

// static
std::optional<PrivateUseSubtags> PrivateUseSubtags::FromString(
    std::string_view private_use_subtags) {
  if (private_use_subtags.empty()) {
    return std::nullopt;
  }

  // If the caller passed the singleton, just ignore it.
  if (private_use_subtags.size() > 1 && private_use_subtags[0] == 'x' &&
      private_use_subtags[1] == '-') {
    private_use_subtags = private_use_subtags.substr(2);
  }
  std::vector<std::string_view> subtags = base::SplitStringPiece(
      private_use_subtags, "-", base::KEEP_WHITESPACE, base::SPLIT_WANT_ALL);
  return FromSubtags(subtags);
}

// static
std::optional<PrivateUseSubtags> PrivateUseSubtags::FromSubtags(
    base::span<const std::string_view> subtags) {
  if (subtags.empty()) {
    return std::nullopt;
  }
  PrivateUseSubtags private_use_subtags_result;
  for (std::string_view subtag : subtags) {
    if (!private_use_subtags_result.AddSubtag(subtag)) {
      return std::nullopt;
    }
  }
  return private_use_subtags_result;
}

UnicodeExtension::~UnicodeExtension() = default;
UnicodeExtension::UnicodeExtension(const UnicodeExtension&) = default;
UnicodeExtension& UnicodeExtension::operator=(const UnicodeExtension&) =
    default;
UnicodeExtension::UnicodeExtension(UnicodeExtension&&) = default;
UnicodeExtension& UnicodeExtension::operator=(UnicodeExtension&&) = default;

UnicodeExtension::UnicodeExtension() = default;

// static
std::optional<UnicodeExtension> UnicodeExtension::FromString(
    std::string_view extension) {
  if (extension.size() < 4u || extension[0] != 'u' || extension[1] != '-') {
    return std::nullopt;
  }

  std::vector<std::string_view> subtags = base::SplitStringPiece(
      extension.substr(2), "-", base::KEEP_WHITESPACE, base::SPLIT_WANT_ALL);
  return FromSubtags(subtags);
}

// static
std::optional<UnicodeExtension> UnicodeExtension::FromSubtags(
    base::span<const std::string_view> subtags) {
  size_t keyword_start = FindNextKeyIndex(subtags);

  UnicodeExtension result;
  // Attributes come before the keywords.
  if (!AddAttributes(subtags.take_first(keyword_start), result)) {
    return std::nullopt;
  }
  // The rest of the subtags are keywords.
  if (!AddKeywords(subtags, result)) {
    return std::nullopt;
  }

  // Empty extensions are not considered valid.
  if (result.attributes_.empty() && result.keywords_.empty()) {
    return std::nullopt;
  }

  return result;
}

bool UnicodeExtension::AddAttribute(std::string_view attribute) {
  if (!VerifyTypeOrAttributeSubtags({attribute})) {
    return false;
  }
  attributes_.emplace(CanonicalizeTypeOrAttributeSubtags({attribute}));
  return true;
}

bool UnicodeExtension::SetKeyword(std::string_view key,
                                  std::string_view type_subtags) {
  if (!VerifyKeySubtag(key)) {
    return false;
  }
  std::vector<std::string_view> types;
  if (!type_subtags.empty()) {
    types = base::SplitStringPiece(type_subtags, "-", base::KEEP_WHITESPACE,
                                   base::SPLIT_WANT_ALL);
  }
  if (!VerifyTypeOrAttributeSubtags(types)) {
    return false;
  }
  keywords_.insert_or_assign(base::ToLowerASCII(key),
                             CanonicalizeTypeOrAttributeSubtags(types));
  return true;
}

std::string UnicodeExtension::SubtagsString() const {
  std::string subtags = base::JoinString(attributes_, "-");
  // Iterates sorted by key.
  for (auto& [key, value] : keywords_) {
    if (!subtags.empty()) {
      subtags.append("-");
    }
    subtags.append(key);
    if (!value.empty()) {
      base::StrAppend(&subtags, {"-", value});
    }
  }
  return subtags;
}

std::optional<std::string_view> UnicodeExtension::GetKeywordValue(
    std::string_view key) const {
  if (key.size() != 2) {
    return std::nullopt;
  }
  auto it = keywords_.find(base::ToLowerASCII(key));
  if (it == keywords_.end()) {
    return std::nullopt;
  }
  return it->second;
}

std::vector<std::string> UnicodeExtension::GetKeywordKeys() const {
  std::vector<std::string> keys;
  keys.reserve(keywords_.size());
  std::ranges::transform(keywords_, std::back_inserter(keys),
                         [](const auto& pair) { return pair.first; });
  return keys;
}

}  // namespace base::i18n
