// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/i18n/language_tag.h"

#include <utility>

#include "base/numerics/safe_conversions.h"
#include "base/strings/string_util.h"

namespace base {
namespace {

// Finds the position of start of the next singleton identified as
// "-"+<singleton>+"-". Where <singleton> is any alpha ASCII character.
size_t FindNextSingleton(std::string_view tag) {
  // Skip the first two characters as they are always present within a language
  // definition or a subtag inside a non-private extension.
  for (size_t i = 2; i + 2 < tag.size(); i++) {
    if (tag[i] == '-' && tag[i + 2] == '-' && base::IsAsciiAlpha(tag[i + 1])) {
      // Skip the first '-', e.g. if "-x-value" was found, "x-value" is
      // returned.
      return i + 1;
    }
  }

  return std::string_view::npos;
}

// Returns the subtags for the extension identified by the singleton `ext_id`.
// It returns the whole extension string (e.g., "a-myext").
std::string_view GetExtensionString(std::string_view tag, char ext_id) {
  size_t extension_pos = FindNextSingleton(tag);
  while (extension_pos != std::string_view::npos) {
    // As `extension_pos` is not `npos`, code is not empty.
    tag = tag.substr(extension_pos);
    // The singleton 'x' was found, the remainder of the code is a sequence of
    // private use subtags.
    if (tag[0] == 'x') {
      return (ext_id == 'x') ? tag : std::string_view();
    }
    if (tag[0] == ext_id) {
      // Look for the next singleton, that is where the found extension is going
      // to end.
      size_t next_extension_pos = FindNextSingleton(tag.substr(2));
      // The `code` must never start with an extension.
      if (next_extension_pos == 0u) {
        return {};
      }
      return (next_extension_pos != std::string_view::npos)
                 ? tag.substr(0, next_extension_pos - 1u)
                 : tag;
    }

    // Move to the next singleton, not that the first two characters are skipped
    // as they are part of the current singleton.
    extension_pos = FindNextSingleton(tag.substr(2));
  }

  return {};
}

}  // namespace

LanguageTag::~LanguageTag() = default;
LanguageTag::LanguageTag(const LanguageTag&) = default;
LanguageTag& LanguageTag::operator=(const LanguageTag&) = default;

std::string LanguageTag::ToLegacyICUFormat() const {
  std::string legacy_tag(ToString());
  base::ReplaceSubstringsAfterOffset(&legacy_tag, 0, "-", "_");
  return legacy_tag;
}

std::string_view LanguageTag::ToString() const {
  return tag_.AsString();
}

LanguageTag::LanguageTag(ImmutableStringType tag) : tag_(std::move(tag)) {
  CHECK(tag_.AsString().size() >= 2);
}

std::optional<RegionSubtag> LanguageTag::region_subtag() const {
  std::string_view tag = tag_.AsString();
  // Region tags are at least 2 chars, and language is at least 2.
  // "en-US" is 5 chars.
  if (tag.size() < 3) {
    return std::nullopt;
  }

  size_t first_hyphen = tag.find('-');
  if (first_hyphen == std::string_view::npos) {
    return std::nullopt;
  }

  size_t second_hyphen = tag.find('-', first_hyphen + 1);

  size_t second_subtag_len;
  if (second_hyphen == std::string_view::npos) {
    second_subtag_len = tag.size() - first_hyphen - 1;
  } else {
    second_subtag_len = second_hyphen - first_hyphen - 1;
  }

  // BCP47 subtag rules:
  // - Language: 2-3 characters (at the start).
  // - Script: Exactly 4 characters (e.g., "Latn", "Hant").
  // - Region: 2 characters (ISO 3166-1 alpha-2) or 3 digits (UN M.49).
  // - Extension: 1 character prefix (e.g., "u-", "x-").
  // - Variant: 5-8 characters (or 4 if it starts with a digit).

  // Check if the second subtag is a region tag (length 2 or 3).
  // This effectively skips single-character extensions and 4-character scripts.
  if (second_subtag_len >= 2 && second_subtag_len <= 3) {
    return RegionSubtag(tag.substr(first_hyphen + 1, second_subtag_len));
  }

  // If the second subtag was not a region, it might be a script (length 4).
  // If so, the region could be the third subtag.
  if (second_subtag_len != 4 || second_hyphen == std::string_view::npos) {
    return std::nullopt;
  }

  // Extract the third subtag and check if it's a region (length 2 or 3).
  size_t third_hyphen = tag.find('-', second_hyphen + 1);
  size_t third_subtag_len;
  if (third_hyphen == std::string_view::npos) {
    third_subtag_len = tag.size() - second_hyphen - 1;
  } else {
    third_subtag_len = third_hyphen - second_hyphen - 1;
  }

  if (third_subtag_len >= 2 && third_subtag_len <= 3) {
    return RegionSubtag(tag.substr(second_hyphen + 1, third_subtag_len));
  }

  return std::nullopt;
}

template <typename R>
std::optional<R> LanguageTag::GetExtensionInternal(char key) const {
  std::string_view extension_string = GetExtensionString(tag_.AsString(), key);
  if (extension_string.empty()) {
    return std::nullopt;
  }
  return R(base::PassKey<LanguageTag>(), extension_string);
}

template BASE_I18N_EXPORT std::optional<i18n_extensions::UnicodeExtension>
LanguageTag::GetExtensionInternal(char) const;

template BASE_I18N_EXPORT std::optional<i18n_extensions::PrivateUseSubtags>
LanguageTag::GetExtensionInternal(char) const;

template BASE_I18N_EXPORT std::optional<i18n_extensions::Extension>
LanguageTag::GetExtensionInternal(char) const;

}  // namespace base
