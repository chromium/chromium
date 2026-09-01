// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/i18n/language_tag.h"

#include <algorithm>
#include <ostream>
#include <utility>

#include "base/check_op.h"
#include "base/i18n/bcp47_extensions.h"
#include "base/i18n/internal/bcp47_parser.h"
#include "base/i18n/internal/legacy_icu_converter.h"
#include "base/numerics/safe_conversions.h"
#include "base/strings/strcat.h"
#include "base/strings/string_split.h"
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

LanguageTag LanguageTag::WithExtensionStringInternal(
    char key,
    std::string_view subtags) const {
  std::optional<i18n_internal::ParsedBcp47Tag> parsed =
      i18n_internal::ParseBcp47Tag(tag_.AsString());
  if (!parsed) {
    return *this;
  }

  parsed->extensions[key] = base::SplitStringPiece(
      subtags, "-", base::KEEP_WHITESPACE, base::SPLIT_WANT_ALL);
  return LanguageTag(i18n_internal::GetBcp47TagPieces(*parsed));
}

LanguageTag LanguageTag::WithLanguageSubtagOnly() const {
  CHECK(language_subtag().size() >= 2);
  return LanguageTag(ImmutableStringType({language_subtag()}));
}

LanguageTag::LanguageTag(ImmutableStringType tag) : tag_(std::move(tag)) {
  CHECK(tag_string().size() >= 2);
}

std::vector<std::string_view> LanguageTag::GetExtensionSubtagsInternal(
    char key) const {
  std::optional<i18n_internal::ParsedBcp47Tag> parsed =
      i18n_internal::ParseBcp47Tag(tag_.AsString());
  if (!parsed) {
    return {};
  }
  char normalized_key = base::ToLowerASCII(key);
  if (normalized_key == 'x') {
    return parsed->private_use;
  }
  return parsed->extensions[normalized_key];
}

std::optional<UnicodeExtension> LanguageTag::GetExtension(
    bcp47_extensions::Traits<'u'> traits) const {
  std::vector<std::string_view> extension = GetExtensionSubtagsInternal('u');
  if (extension.empty()) {
    return std::nullopt;
  }

  return traits.Factory(base::PassKey<LanguageTag>(), extension);
}

std::optional<PrivateUseSubtags> LanguageTag::GetExtension(
    bcp47_extensions::Traits<'x'> traits) const {
  std::vector<std::string_view> extension = GetExtensionSubtagsInternal('x');
  if (extension.empty()) {
    return std::nullopt;
  }

  return traits.Factory(base::PassKey<LanguageTag>(), extension);
}

LanguageTag LanguageTag::WithExtension(
    const UnicodeExtension& extension) const {
  return WithExtensionStringInternal(extension.singleton(),
                                     extension.SubtagsString());
}

LanguageTag LanguageTag::WithExtension(
    const PrivateUseSubtags& extension) const {
  return WithExtensionStringInternal(extension.singleton(),
                                     extension.SubtagsString());
}

LanguageTag LanguageTag::WithExtension(const Extension& extension) const {
  return WithExtensionStringInternal(extension.singleton(),
                                     extension.SubtagsString());
}

LanguageTag LanguageTag::WithExtensionRemoved(char key) const {
  std::optional<i18n_internal::ParsedBcp47Tag> parsed =
      i18n_internal::ParseBcp47Tag(tag_.AsString());
  if (!parsed) {
    return *this;
  }
  char normalized_key = base::ToLowerASCII(key);
  if (normalized_key == 'x') {
    parsed->private_use.clear();
  } else {
    parsed->extensions.erase(normalized_key);
  }
  return LanguageTag(i18n_internal::GetBcp47TagPieces(*parsed));
}

std::ostream& operator<<(std::ostream& os, const LanguageTag& lt) {
  return os << lt.tag_string();
}

std::ostream& operator<<(std::ostream& os,
                         const std::optional<LanguageTag>& opt) {
  return opt ? os << *opt : os << "nullopt";
}

}  // namespace base::i18n
