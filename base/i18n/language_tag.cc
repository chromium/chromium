// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/i18n/language_tag.h"

#include <algorithm>
#include <utility>

#include "base/check_op.h"
#include "base/i18n/bcp47_extensions.h"
#include "base/i18n/internal/legacy_icu_converter.h"
#include "base/numerics/safe_conversions.h"
#include "base/strings/strcat.h"
#include "base/strings/string_util.h"

namespace base::i18n {
namespace {

// Finds the position of start of the next singleton identified as
// "-"+<singleton>+"-". Where <singleton> is any alpha ASCII character.
size_t FindNextSingleton(std::string_view tag) {
  // Skip the first two characters as they are always either an extension
  // singleton (e.g. "u-") or the beginning of the language tag which is at
  // least two characters long.
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
      size_t next_extension_pos = FindNextSingleton(tag);
      // The `code` must never start with an extension.
      if (next_extension_pos == 0u) {
        return {};
      }
      return (next_extension_pos != std::string_view::npos)
                 ? tag.substr(0, next_extension_pos - 1u)
                 : tag;
    }

    // Move to the next singleton.
    extension_pos = FindNextSingleton(tag);
  }

  return {};
}

}  // namespace

std::string LanguageTag::ToLegacyICUFormat() const {
  size_t first_extension_pos = FindNextSingleton(tag_.AsString());
  CHECK_GT(first_extension_pos, 0u);
  std::string legacy_code;
  base::ReplaceChars(tag_string().substr(0, first_extension_pos - 1u), "-", "_",
                     &legacy_code);
  // If there are no extensions, there is nothing left to do.
  if (first_extension_pos == std::string_view::npos) {
    return legacy_code;
  }
  std::optional<UnicodeExtension> unicode_extension =
      GetExtension(bcp47_extensions::unicode());
  // There is only support to converting unicode extensions to the legacy
  // format. The rest is ignored.
  if (!unicode_extension) {
    return legacy_code;
  }

  base::StrAppend(&legacy_code,
                  {"@", i18n_internal::ConvertBcp47UnicodeKeywordsToLegacyCode(
                            unicode_extension->keywords())});
  return legacy_code;
}

LanguageTag::LanguageTag(ImmutableStringType tag) : tag_(std::move(tag)) {
  CHECK(tag_string().size() >= 2);
}

LanguageSubtag LanguageTag::GetLanguageSubtag() const {
  std::string_view tag = tag_.AsString();
  size_t lang_subtag_end = std::min(tag.find('-'), tag.size());
  return LanguageSubtag(tag.substr(0, lang_subtag_end));
}

std::optional<RegionSubtag> LanguageTag::GetRegionSubtag() const {
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

std::string_view LanguageTag::GetExtensionStringInternal(char key) const {
  return GetExtensionString(tag_.AsString(), key);
}

}  // namespace base::i18n
