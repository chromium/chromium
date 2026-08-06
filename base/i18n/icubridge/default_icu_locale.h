// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef BASE_I18N_ICUBRIDGE_DEFAULT_ICU_LOCALE_H_
#define BASE_I18N_ICUBRIDGE_DEFAULT_ICU_LOCALE_H_

#include "base/i18n/base_i18n_export.h"
#include "base/i18n/language_tag.h"

namespace base::i18n {

class ScopedDefaultIcuLocale;
class BASE_I18N_EXPORT DefaultIcuLocaleSetterKey;

// Returns the current system/library-wide default ICU locale as a
// `LanguageTag`. Safe to call concurrently from any thread.
BASE_I18N_EXPORT LanguageTag GetDefaultIcuLocale();

// Updates the global default ICU locale.
//
// Access is restricted to authorized callers (like `ScopedDefaultIcuLocale`)
// via the `DefaultIcuLocaleSetterKey` pass-key.
//
// This operation is safe to call from any thread.
BASE_I18N_EXPORT void SetDefaultIcuLocale(DefaultIcuLocaleSetterKey key,
                                          const LanguageTag& language_tag);

// A capability token enforcing the C++ pass-key pattern for setting the
// mutable default ICU locale.
//
// Calling `SetDefaultIcuLocale` requires an instance of this key. Since the
// constructor of `DefaultIcuLocaleSetterKey` is private, only explicitly
// friended classes can instantiate it. This prevents arbitrary production
// code from modifying the global default ICU locale, while allowing
// authorized test utilities (like ScopedDefaultIcuLocale) to temporarily
// override it.
class BASE_I18N_EXPORT DefaultIcuLocaleSetterKey {
 public:
  ~DefaultIcuLocaleSetterKey() = default;

 private:
  friend class ScopedDefaultIcuLocale;

  DefaultIcuLocaleSetterKey() = default;
};

}  // namespace base::i18n

#endif  // BASE_I18N_ICUBRIDGE_DEFAULT_ICU_LOCALE_H_
