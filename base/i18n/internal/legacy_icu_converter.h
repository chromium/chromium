// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef BASE_I18N_INTERNAL_LEGACY_ICU_CONVERTER_H_
#define BASE_I18N_INTERNAL_LEGACY_ICU_CONVERTER_H_

#include <optional>
#include <string>
#include <string_view>

namespace base::i18n::internal {

// TODO(crbug.com/517510055): implement the inversion once Extensions is
// available in `LanguageCode`.
// Converts a legacy ICU locale code (e.g., "en_US@currency=USD") to a BCP47
// language tag (e.g., "en-US-u-cu-usd").
//
// Legacy ICU locale IDs often use '_' as a subtag separator and '@' to denote
// keywords. This function converts those into BCP47-compliant tags using '-' as
// a separator and the "-u-" extension for Unicode locale extensions.
//
// See https://www.unicode.org/reports/tr35/#Unicode_locale_identifier for the
// BCP47 Unicode locale extension specification.
//
// Returns std::nullopt if the input is already BCP47-compatible or doesn't
// contain legacy ICU-specific separators ('_' or '@').
//
// Note: this function does not run checks on whether the keys and values in
// unicode extensions are valid, as this function is supposed to be used to
// prepare the input for the actual parsing that happens later when constructing
// a LanguageCode.
std::optional<std::string> ConvertLegacyCodeToBcp47IfNecessary(
    std::string_view code);

}  // namespace base::i18n::internal

#endif  // BASE_I18N_INTERNAL_LEGACY_ICU_CONVERTER_H_
