// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SSL_GENERATED_HTTPS_FIRST_MODE_PREF_H_
#define CHROME_BROWSER_SSL_GENERATED_HTTPS_FIRST_MODE_PREF_H_

#include "base/memory/raw_ptr.h"
#include "base/scoped_observation.h"
#include "chrome/browser/extensions/api/settings_private/generated_pref.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/safe_browsing/advanced_protection_status_manager.h"
#include "components/prefs/pref_change_registrar.h"

// The generated pref for HTTPS-First Mode. Only used for managing the setting
// in the Security UI settings page. The actual HTTPS-First Mode is controlled
// by `prefs::kHttpsOnlyModeEnabled` and `prefs::kHttpsFirstBalancedMode`.
extern const char kGeneratedHttpsFirstModePref[];

class GeneratedHttpsFirstModePref
    : public extensions::settings_private::GeneratedPref,
      public safe_browsing::AdvancedProtectionStatusManager::
          StatusChangedObserver {
 public:
  explicit GeneratedHttpsFirstModePref(Profile* profile);
  ~GeneratedHttpsFirstModePref() override;

  // Generated Preference Interface.
  extensions::settings_private::SetPrefResult SetPref(
      const base::Value* value) override;
  extensions::api::settings_private::PrefObject GetPrefObject() const override;

  // Fired when preferences used to generate this preference are changed.
  void OnSourcePreferencesChanged();

  // safe_browsing::AdvancedProtectionStatusManager::StatusChangedObserver:
  void OnAdvancedProtectionStatusChanged(bool enabled) override;

 private:
  // Applies the effective management state of HTTPS-First Mode for `profile` to
  // `pref_object`.
  static void ApplyManagementState(
      const Profile& profile,
      extensions::api::settings_private::PrefObject& pref_object);

  // Non-owning pointer to the profile this preference is generated for.
  const raw_ptr<Profile> profile_;
  PrefChangeRegistrar user_prefs_registrar_;

  base::ScopedObservation<
      safe_browsing::AdvancedProtectionStatusManager,
      safe_browsing::AdvancedProtectionStatusManager::StatusChangedObserver>
      obs_{this};
};

#endif  // CHROME_BROWSER_SSL_GENERATED_HTTPS_FIRST_MODE_PREF_H_
