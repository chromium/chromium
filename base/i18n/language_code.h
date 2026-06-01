// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef BASE_I18N_LANGUAGE_CODE_H_
#define BASE_I18N_LANGUAGE_CODE_H_

#include <algorithm>
#include <array>
#include <compare>
#include <cstdint>
#include <optional>
#include <ostream>
#include <string_view>

#include "base/check.h"
#include "base/i18n/base_i18n_export.h"
#include "base/i18n/internal/immutable_string.h"

namespace base {

class BASE_I18N_EXPORT LanguageCodeBuilder;
class BASE_I18N_EXPORT RegionCode;
// A type-safe wrapper for BCP47 language codes (locales).
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
class BASE_I18N_EXPORT LanguageCode {
 public:
  using ImmutableStringType = i18n::internal::ImmutableString;

  ~LanguageCode();
  LanguageCode(const LanguageCode&);
  LanguageCode& operator=(const LanguageCode&);

  friend bool operator==(const LanguageCode& lhs, const LanguageCode& rhs) {
    return lhs.ToString() == rhs.ToString();
  }
  friend bool operator<(const LanguageCode& lhs, const LanguageCode& rhs) {
    return lhs.ToString() < rhs.ToString();
  }
  friend std::ostream& operator<<(std::ostream& os, const LanguageCode& lc) {
    return os << lc.ToString();
  }
  friend std::ostream& operator<<(std::ostream& os,
                                  const std::optional<LanguageCode>& opt) {
    return opt ? os << *opt : os << "nullopt";
  }

  // Returns the BCP47 language tag (e.g., "en-US", "zh-CN").
  std::string_view ToString() const;

  // Returns the language code in legacy ICU format, replacing hyphens with
  // underscores (e.g., "en_US", "zh_CN").
  // Note: This does not work correctly when the code has extensions.
  // TODO(crbug.com/517510055): Convert unicode extensions to the legacy format.
  std::string ToLegacyICUFormat() const;

  // Returns the region subtag in the language code if present.
  // Examples:
  // - "en-US" -> "US"
  // - "zh-Hant-TW" -> "TW"
  // - "en" -> std::nullopt
  // - "sr-Latn" -> std::nullopt
  std::optional<RegionCode> GetRegionCode() const;

 private:
  friend class LanguageCodeBuilder;

  // This constructor is intended for internal use by `LanguageCodeBuilder`.
  // Do not call this directly.
  explicit LanguageCode(ImmutableStringType code);

  // The BCP47 language code, e.g. "pt-BR".
  // Supports language, script, region, variants and extensions.
  ImmutableStringType code_;
};

// Represents the code for a region extracted from a LanguageCode.
class RegionCode {
 public:
  std::string_view ToString() const;
  friend bool operator==(const RegionCode& lhs, const RegionCode& rhs) {
    return lhs.ToString() == rhs.ToString();
  }
  friend bool operator<(const RegionCode& lhs, const RegionCode& rhs) {
    return lhs.ToString() < rhs.ToString();
  }
  friend std::ostream& operator<<(std::ostream& os, const RegionCode& rc) {
    return os << rc.ToString();
  }
  friend std::ostream& operator<<(std::ostream& os,
                                  const std::optional<RegionCode>& opt) {
    return opt ? os << *opt : os << "nullopt";
  }

 private:
  friend class LanguageCode;

  // Only LanguageCode can construct it.
  explicit RegionCode(std::string_view code);

  std::array<char, 3> code_;
  uint8_t size_;
};

}  // namespace base

namespace std {

template <>
struct hash<base::LanguageCode> {
  std::size_t operator()(const base::LanguageCode& lc) const {
    return std::hash<std::string_view>()(lc.ToString());
  }
};

template <>
struct hash<base::RegionCode> {
  std::size_t operator()(const base::RegionCode& rc) const {
    return std::hash<std::string_view>()(rc.ToString());
  }
};

}  // namespace std

#endif  // BASE_I18N_LANGUAGE_CODE_H_
