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

// A generated preference which represents the effective Security Settings
// Bundled state based on the underlying Security Settings Bundle preference.
// This preference allows the direct use of WebUI controls without exposing any
// logic responsible for coalescing the underlying Security Setting Bundled pref
// to front-end code.
// To add a new setting controlled by the bundled pref, add a static
// GetDefault() method which returns the setting's default value and pass the
// default to WebUI in settings::AddSecurityData(). Update the WebUI
// reset-to-default logic.
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
