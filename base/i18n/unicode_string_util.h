// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef BASE_I18N_UNICODE_STRING_UTIL_H_
#define BASE_I18N_UNICODE_STRING_UTIL_H_

#include <stdint.h>

#include <string>
#include <string_view>

#include "base/containers/span.h"
#include "base/i18n/base_i18n_export.h"

namespace base::i18n {

// Returns true if the UTF-8 or UTF-16 string contains any Unicode Control
// (category Cc) or Format (category Cf, such as bidirectional controls or
// zero-width joiners) characters. Ill-formed byte sequences or unpaired
// surrogates are ignored.
BASE_I18N_EXPORT bool ContainsUnicodeControlOrFormat(std::string_view str);
BASE_I18N_EXPORT bool ContainsUnicodeControlOrFormat(std::u16string_view str);

// Returns true if the Latin-1 (ISO-8859-1) byte span contains any Unicode
// Control (Cc) or Format (Cf) characters. Takes `span<const uint8_t>` rather
// than `std::string_view` so UTF-8 strings cannot be passed accidentally.
BASE_I18N_EXPORT bool ContainsUnicodeControlOrFormatLatin1(
    base::span<const uint8_t> str);

// Removes all Unicode Control (Cc) and Format (Cf) characters from `str`.
// Returns true if any Control or Format characters were removed; when
// sanitizing, any unpaired UTF-16 surrogates are also replaced with U+FFFD so
// removing an intervening character cannot coalesce split surrogates into a
// new character. If `str` contains no Control or Format characters, returns
// false without modifying `str`.
BASE_I18N_EXPORT bool RemoveUnicodeControlAndFormat(std::u16string& str);

}  // namespace base::i18n

#endif  // BASE_I18N_UNICODE_STRING_UTIL_H_
