// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef BASE_I18N_LANGUAGE_TAG_H_
#define BASE_I18N_LANGUAGE_TAG_H_

#include <algorithm>
#include <array>
#include <compare>
#include <cstdint>
#include <limits>
#include <optional>
#include <ostream>
#include <string_view>

#include "base/check.h"
#include "base/i18n/base_i18n_export.h"
#include "base/i18n/bcp47_extensions.h"
#include "base/i18n/internal/immutable_string.h"

namespace base {

class BASE_I18N_EXPORT LanguageTagConverter;
class BASE_I18N_EXPORT RegionSubtag;

// A type-safe wrapper for BCP47 language tags (locales).
//
// Supported Format Specification:
// - Core Standard: BCP47 language tags.
// - Structure: Supports subtags separated by hyphens ('-'):
//   - Language: Mandatory, >= 2 chars (e.g., "en", "zh") or 3 chars.
//   - Script: Optional, 4 chars (e.g., "Hant").
//   - Region: Optional, 2-3 chars (e.g., "US", "001").
//   - Variants: Optional (e.g., "oxendict").
//   - Extensions: Optional (e.g., "u-ca-gregory").
//   - Private use: Optional (e.g., "x-privatestuff")
class BASE_I18N_EXPORT LanguageTag {
 public:
  using ImmutableStringType = i18n::internal::ImmutableString;

  ~LanguageTag();
  LanguageTag(const LanguageTag&);
  LanguageTag& operator=(const LanguageTag&);

  friend bool operator==(const LanguageTag& lhs, const LanguageTag& rhs) {
    return lhs.ToString() == rhs.ToString();
  }
  friend bool operator<(const LanguageTag& lhs, const LanguageTag& rhs) {
    return lhs.ToString() < rhs.ToString();
  }
  friend std::ostream& operator<<(std::ostream& os, const LanguageTag& lt) {
    return os << lt.ToString();
  }
  friend std::ostream& operator<<(std::ostream& os,
                                  const std::optional<LanguageTag>& opt) {
    return opt ? os << *opt : os << "nullopt";
  }

  // Returns the BCP47 language tag (e.g., "en-US", "zh-CN").
  std::string_view ToString() const;

  // Returns the language tag in legacy ICU format, replacing hyphens with
  // underscores (e.g., "en_US", "zh_CN").
  // Note: This does not work correctly when the tag has extensions.
  // TODO(crbug.com/517510055): Convert unicode extensions to the legacy format.
  std::string ToLegacyICUFormat() const;

  // Returns the region subtag in the language tag if present.
  // Examples:
  // - "en-US" -> "US"
  // - "zh-Hant-TW" -> "TW"
  // - "en" -> std::nullopt
  // - "sr-Latn" -> std::nullopt
  std::optional<RegionSubtag> region_subtag() const;

  // Retrieves the singleton and subtag(s) for an extension to a BCP47 language
  // tag.
  //
  // Use the helper functions in `i18n_extensions` to specify which extension
  // or private use subtags to retrieve:
  // - `GetExtension(i18n_extensions::unicode())` for "u-" extensions.
  // - `GetExtension(i18n_extensions::priv())` for "x-" private use subtags.
  // - `GetExtension(i18n_extensions::ext('a'))` for any other single-char
  // extension.
  //
  // Example:
  //   auto locale =
  //   LanguageTagConverter::GetInstance().FromString("en-US-u-ca-gregory");
  //   auto ext = locale->GetExtension(i18n_extensions::unicode());
  //   if (ext) {
  //     std::string_view val = ext->subtags_string(); // "ca-gregory"
  //   }
  template <i18n_extensions::ExtensionTrait T>
  std::optional<typename T::type> GetExtension(T traits) const {
    return GetExtensionInternal<typename T::type>(T::key);
  }

 private:
  friend class LanguageTagConverter;

  template <typename R>
  std::optional<R> GetExtensionInternal(char key) const;
  // This constructor is intended for internal use by `LanguageTagConverter`.
  // Do not call this directly.
  explicit LanguageTag(ImmutableStringType tag);

  // The BCP47 language tag, e.g. "pt-BR".
  // Supports language, script, region, variants and extensions.
  ImmutableStringType tag_;
};

namespace internal {

// General representation of a BCP47 subtag
// (https://www.rfc-editor.org/info/rfc5646/#section-2.1)
template <size_t MinLen, size_t MaxLen>
class BASE_I18N_EXPORT Bcp47Subtag {
 public:
  static_assert(MinLen <= MaxLen);
  static_assert(MaxLen <= std::numeric_limits<uint8_t>::max());

  using SubtagType = Bcp47Subtag<MinLen, MaxLen>;

  explicit Bcp47Subtag(std::string_view subtag) : size_(subtag.size()) {
    std::copy(subtag.begin(), subtag.end(), value_.begin());
    CHECK_GE(subtag.size(), MinLen);
    CHECK_LE(subtag.size(), MaxLen);
  }

  friend bool operator==(const SubtagType& lhs, const SubtagType& rhs) {
    return lhs.subtag_string() == rhs.subtag_string();
  }
  friend bool operator<(const SubtagType& lhs, const SubtagType& rhs) {
    return lhs.subtag_string() < rhs.subtag_string();
  }
  friend std::ostream& operator<<(std::ostream& os, const SubtagType& tag) {
    return os << tag.subtag_string();
  }
  friend std::ostream& operator<<(std::ostream& os,
                                  const std::optional<SubtagType>& opt) {
    return opt ? os << *opt : os << "nullopt";
  }

  std::string_view subtag_string() const {
    return std::string_view(value_.data(), static_cast<size_t>(size_));
  }

 private:
  std::array<char, MaxLen> value_;
  uint8_t size_;
};

}  // namespace internal

// Represents the region subtag extracted from a LanguageTag.
// The spec definition can be found here:
// https://www.rfc-editor.org/info/rfc5646/#section-2.1
// They are defined as:
// region        = 2ALPHA
//              / 3DIGIT
class RegionSubtag : public internal::Bcp47Subtag<2, 3> {
 public:
  using base_type = internal::Bcp47Subtag<2, 3>;
  using base_type::base_type;
};

}  // namespace base

namespace std {

template <>
struct hash<base::LanguageTag> {
  std::size_t operator()(const base::LanguageTag& tag) const {
    return std::hash<std::string_view>()(tag.ToString());
  }
};

template <>
struct hash<base::RegionSubtag> {
  std::size_t operator()(const base::RegionSubtag& region_subtag) const {
    return std::hash<std::string_view>()(region_subtag.subtag_string());
  }
};

}  // namespace std

#endif  // BASE_I18N_LANGUAGE_TAG_H_
