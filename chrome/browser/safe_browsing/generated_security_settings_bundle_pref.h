// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SAFE_BROWSING_GENERATED_SECURITY_SETTINGS_BUNDLE_PREF_H_
#define CHROME_BROWSER_SAFE_BROWSING_GENERATED_SECURITY_SETTINGS_BUNDLE_PREF_H_

#include "base/memory/raw_ptr.h"
#include "chrome/browser/extensions/api/settings_private/generated_pref.h"
#include "chrome/browser/profiles/profile.h"
#include "components/prefs/pref_change_registrar.h"

namespace safe_browsing {

extern const char kGeneratedSecuritySettingsBundlePref[];

// Must be kept in sync with the SecuritySettingsBundle enum located in
// chrome/browser/resources/settings/privacy_page/security_page_v2.js.
// LINT.IfChange(SecuritySettingsBundleSetting)
enum class SecuritySettingsBundleSetting {
  // Standard bundle with default settings.
  STANDARD = 0,
  // Enhanced bundle with most secure settings selected.
  ENHANCED = 1,
};
// LINT.ThenChange(/chrome/browser/resources/settings/privacy_page/security_page_v2.ts:SecuritySettingsBundleSetting)

// A generated preference which represents the effective Security Settings
// Bundled state based on the underlying Security Settings Bundle preference.
// This preference allows the direct use of WebUI controls without exposing any
// logic responsible for coalescing the underlying Security Setting Bundled pref
// to front-end code.
class GeneratedSecuritySettingsBundlePref
    : public extensions::settings_private::GeneratedPref {
 public:
  explicit GeneratedSecuritySettingsBundlePref(Profile* profile);

  // Generated Preference Interface.
  extensions::settings_private::SetPrefResult SetPref(
      const base::Value* value) override;
  extensions::api::settings_private::PrefObject GetPrefObject() const override;

  // Fired when one of the bundle levels are changed.
  void OnSecuritySettingsBundlePreferencesChanged();

 private:
  // Weak reference to the profile this preference is generated for.
  const raw_ptr<Profile> profile_;

  PrefChangeRegistrar user_prefs_registrar_;
};

}  // namespace safe_browsing

#endif  // CHROME_BROWSER_SAFE_BROWSING_GENERATED_SECURITY_SETTINGS_BUNDLE_PREF_H_
