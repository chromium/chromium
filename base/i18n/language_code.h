// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef BASE_I18N_LANGUAGE_CODE_H_
#define BASE_I18N_LANGUAGE_CODE_H_

#include <algorithm>
#include <array>
#include <compare>
#include <cstdint>
#include <functional>
#include <optional>
#include <ostream>
#include <string_view>
#include <type_traits>

#include "base/check.h"
#include "base/i18n/base_i18n_export.h"

namespace base {

class LanguageCodeBuilder;

// A type-safe wrapper for BCP47 language codes (locales).
//
// Supported Format Specification:
// - Core Standard: BCP47 language tags.
// - Structure: Supports up to 3 subtags separated by hyphens ('-'):
//   - Language: Mandatory, >= 2 chars (e.g., "en", "zh") or 3 chars.
//   - Script: Optional, 4 chars (e.g., "Hant").
//   - Region: Optional, 2-3 chars (e.g., "US", "001").
// - Supported Combinations: "lang", "lang-region", "lang-script",
// "lang-script-region".
// - Constraints: Maximum total length of 12 characters, maximum of 3 parts.
// - Variants: Language variants (e.g. oxendict) are not supported yet.
class BASE_I18N_EXPORT LanguageCode {
 public:
  ~LanguageCode() = default;
  LanguageCode(const LanguageCode&) = default;
  LanguageCode& operator=(const LanguageCode&) = default;

  friend constexpr bool operator==(const LanguageCode& lhs,
                                   const LanguageCode& rhs) {
    return lhs.code_ == rhs.code_;
  }
  friend constexpr bool operator<(const LanguageCode& lhs,
                                  const LanguageCode& rhs) {
    return lhs.code_ < rhs.code_;
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
  std::string ToLegacyICUFormat() const;

 private:
  friend class LanguageCodeBuilder;

  // This constructor is intended for internal use by `LanguageCodeBuilder`.
  // Do not call this directly.
  // A constexpr constructor is provided so we can construct fixed sets of
  // LanguageCode at compile-time.
  constexpr explicit LanguageCode(std::string_view code)
      : length_(code.size()) {
    CHECK(code.size() >= 2 && code.size() <= 12);
    std::copy(code.begin(), code.end(), code_.begin());
  }

  // The BCP47 language code, e.g. "pt-BR".
  // language_subtag_length <= 3
  // script_length <= 4
  // region_length <= 3
  // The separators add 2 to the length, in total we need 3 + 4 + 3 + 2 = 12
  // We do not null-terminate because std::string_view does not require null
  // termination and it can be created from a pointer and length.
  std::array<char, 12> code_{};
  uint8_t length_;
};

}  // namespace base

namespace std {

template <>
struct hash<base::LanguageCode> {
  std::size_t operator()(const base::LanguageCode& lc) const {
    return std::hash<std::string_view>()(lc.ToString());
  }
};

}  // namespace std

#endif  // BASE_I18N_LANGUAGE_CODE_H_
