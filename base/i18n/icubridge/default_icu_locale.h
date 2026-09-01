// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef BASE_I18N_ICUBRIDGE_DEFAULT_ICU_LOCALE_H_
#define BASE_I18N_ICUBRIDGE_DEFAULT_ICU_LOCALE_H_

#include <string>
#include <string_view>

#include "base/i18n/base_i18n_export.h"
#include "base/i18n/language_tag.h"

namespace base::i18n {

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

}  // namespace base::i18n

// Forward declarations for DefaultIcuLocaleSetterKey.
class WebEngineMainDelegate;
class WebEngineBrowserMainParts;

namespace android_webview {
class AwMainDelegate;
void InitIcuAndResourceBundleBrowserSide();
}

namespace blink {
class LocaleController;
}

namespace l10n_util {
std::string GetApplicationLocale(std::string_view, bool);
}

namespace base::i18n {

class ScopedDefaultIcuLocale;
void BASE_I18N_EXPORT SetICUDefaultLocale(std::string_view);

// A capability token enforcing the C++ pass-key pattern for setting the
// mutable default ICU locale.
//
// Calling `SetDefaultIcuLocale` requires an instance of this key. Since the
// constructor of `DefaultIcuLocaleSetterKey` is private, only explicitly
// friended classes can instantiate it. This prevents arbitrary production
// code from modifying the global default ICU locale, while allowing
// authorized test utilities to temporarily override it.
class BASE_I18N_EXPORT DefaultIcuLocaleSetterKey {
 public:
  ~DefaultIcuLocaleSetterKey() = default;

 private:
  friend class ScopedDefaultIcuLocale;
  friend class ::blink::LocaleController;
  friend class ::android_webview::AwMainDelegate;
  friend void ::android_webview::InitIcuAndResourceBundleBrowserSide();
  friend class ::WebEngineMainDelegate;
  friend class ::WebEngineBrowserMainParts;
  friend std::string(::l10n_util::GetApplicationLocale)(std::string_view, bool);
  friend BASE_I18N_EXPORT void SetICUDefaultLocale(std::string_view);

  DefaultIcuLocaleSetterKey() = default;
};

}  // namespace base::i18n

#endif  // BASE_I18N_ICUBRIDGE_DEFAULT_ICU_LOCALE_H_
