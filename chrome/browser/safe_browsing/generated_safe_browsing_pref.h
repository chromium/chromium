// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SAFE_BROWSING_GENERATED_SAFE_BROWSING_PREF_H_
#define CHROME_BROWSER_SAFE_BROWSING_GENERATED_SAFE_BROWSING_PREF_H_

#include "base/memory/raw_ptr.h"
#include "chrome/browser/extensions/api/settings_private/generated_pref.h"
#include "chrome/browser/profiles/profile.h"
#include "components/prefs/pref_change_registrar.h"

namespace safe_browsing {

extern const char kGeneratedSafeBrowsingPref[];

// Must be kept in sync with the SafeBrowsing enum located in
// chrome/browser/resources/settings/privacy_page/security_page.js.
enum class SafeBrowsingSetting {
  ENHANCED,
  STANDARD,
  DISABLED,
};

// A generated preference which represents the effective Safe Browsing setting
// state (including non-user management) based on the underlying Safe Browsing
// enhanced, enabled and reporting preferences. This preference allows the
// direct use of WebUI controls without exposing any logic responsible for
// coalescing the underlying Safe Browsing prefs to front-end code.
class GeneratedSafeBrowsingPref
    : public extensions::settings_private::GeneratedPref {
 public:
  explicit GeneratedSafeBrowsingPref(Profile* profile);

  // Generated Preference Interface.
  extensions::settings_private::SetPrefResult SetPref(
      const base::Value* value) override;
  extensions::api::settings_private::PrefObject GetPrefObject() const override;

  // Fired when underlying Safe Browsing preferences are changed.
  void OnSafeBrowsingPreferencesChanged();

 private:
  // Applies the effective management state of Safe Browsing for |profile| to
  // |pref_object|.
  static void ApplySafeBrowsingManagementState(
      const Profile& profile,
      extensions::api::settings_private::PrefObject& pref_object);

  // Weak reference to the profile this preference is generated for.
  const raw_ptr<Profile> profile_;

  PrefChangeRegistrar user_prefs_registrar_;
};

}  // namespace safe_browsing

#endif  // CHROME_BROWSER_SAFE_BROWSING_GENERATED_SAFE_BROWSING_PREF_H_
