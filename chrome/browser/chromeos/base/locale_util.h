// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// This file contains utility functions for locale change.

#ifndef CHROME_BROWSER_CHROMEOS_BASE_LOCALE_UTIL_H_
#define CHROME_BROWSER_CHROMEOS_BASE_LOCALE_UTIL_H_

#include <memory>
#include <string>

#include "base/callback_forward.h"

class Profile;
class PrefService;

namespace chromeos {
namespace locale_util {

struct LanguageSwitchResult {
  LanguageSwitchResult(const std::string& requested_locale,
                       const std::string& loaded_locale,
                       bool success);

  std::string requested_locale;
  std::string loaded_locale;
  bool success;
};

// This callback is called on UI thread, when ReloadLocaleResources() is
// completed on BlockingPool.
// |result| contains:
//   locale - (copy of) locale argument to SwitchLanguage(). Expected locale.
//   loaded_locale - actual locale name loaded.
//   success - if locale load succeeded.
// (const std::string* locale, const std::string* loaded_locale, bool success)
typedef base::Callback<void(const LanguageSwitchResult& result)>
    SwitchLanguageCallback;

// This function updates input methods only if requested. In general, you want
// |enable_locale_keyboard_layouts = true|. |profile| is needed because IME
// extensions are per-user.
// Note: in case of |enable_locale_keyboard_layouts = false|, the input method
// currently in use may not be supported by the new locale. Using the new locale
// with an unsupported input method may lead to undefined behavior. Use
// |enable_locale_keyboard_layouts = false| with caution.
// Note 2: |login_layouts_only = true| enables only login-capable layouts.
void SwitchLanguage(const std::string& locale,
                    const bool enable_locale_keyboard_layouts,
                    const bool login_layouts_only,
                    const SwitchLanguageCallback& callback,
                    Profile* profile);

// This function checks if the given language is allowed according to the list
// of allowed languages (stored in |prefs|, managed by 'AllowedLanguages'
// policy). If the list is empty, every language is allowed.
bool IsAllowedLanguage(const std::string& language, const PrefService* prefs);

// This function checks if the given language is allowed
// by 'AllowedLanguages' and also can be used as a UI locale.
// (see |IsAllowedLanguage|).
bool IsAllowedUILanguage(const std::string& language, const PrefService* prefs);

// This functions checks if the given language is a native UI language or can be
// converted to one. (e.g., 'en-US', 'fr', 'de', 'de-CH', 'fr-CH' etc. are all
// valid, but 'az' is not.
bool IsNativeUILanguage(const std::string& locale);

// This function removes languages that are disallowed by the
// 'AllowedLanguages' policy from the list of preferred languages.
// If no current preferred languages are allowed, this functions sets
// a language provided by GetAllowedFallbackLanguage() as the only preferred
// language.
void RemoveDisallowedLanguagesFromPreferred(PrefService* prefs);

// This function returns a fallback language that is allowed by the
// 'AllowedLanguages' policy and can be used as a UI language
// (see |IsNativeUILanguage|) as well. We check the user's preferred language
// for any of these conditions. If none of them match, we will take the first
// valid UI language in the list of allowed languages. If none of them are UI
// languages, we default to "en-US" (see kAllowedUILanguageFallback).
// languages (stored in |prefs|, managed by 'AllowedLanguages' policy). If none
// of the user's preferred languages is allowed, the function returns the first
// valid entry in the allowed languages list. If the list contains no valid
// entries, the default fallback will be 'en-US'
// (kAllowedLanguagesFallbackLocale);
std::string GetAllowedFallbackUILanguage(const PrefService* prefs);

// This function adds the |locale| to the list of preferred languages (pref
// |kLanguagePreferredLanguages|). Returns true if the locale was newly added
// to the list, false otherwise.
bool AddLocaleToPreferredLanguages(const std::string& locale,
                                   PrefService* prefs);

}  // namespace locale_util
}  // namespace chromeos

#endif  // CHROME_BROWSER_CHROMEOS_BASE_LOCALE_UTIL_H_
