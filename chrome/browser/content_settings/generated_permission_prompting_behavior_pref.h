// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CONTENT_SETTINGS_GENERATED_PERMISSION_PROMPTING_BEHAVIOR_PREF_H_
#define CHROME_BROWSER_CONTENT_SETTINGS_GENERATED_PERMISSION_PROMPTING_BEHAVIOR_PREF_H_

#include "base/memory/raw_ptr.h"
#include "base/scoped_observation.h"

#include "chrome/browser/extensions/api/settings_private/generated_pref.h"
#include "chrome/browser/profiles/profile.h"
#include "components/content_settings/core/browser/content_settings_observer.h"
#include "components/content_settings/core/browser/host_content_settings_map.h"
#include "components/content_settings/core/common/content_settings_types.h"
#include "components/prefs/pref_change_registrar.h"

namespace content_settings {

// Must be kept in sync with the enum of the same name in
// chrome/browser/resources/settings/site_settings/constants.js
enum class SettingsState {
  kCanPromptWithAlwaysLoudUI = 0,
  kCanPromptWithAlwaysQuietUI = 1,
  kCanPromptWithCPSS = 2,
  kBlocked = 3,
};

extern const char kGeneratedNotificationPref[];
extern const char kGeneratedGeolocationPref[];

// A generated preference which represents the effective Notification or
// Geolocation setting state based on the content setting state, quiet ui pref
// and cpss pref. The object is tied to the lifetime of the
// extensions::settings_private::GeneratedPrefs keyed service.
class GeneratedPermissionPromptingBehaviorPref
    : public extensions::settings_private::GeneratedPref,
      public content_settings::Observer {
 public:
  explicit GeneratedPermissionPromptingBehaviorPref(
      Profile* profile,
      ContentSettingsType content_settings_type);

  ~GeneratedPermissionPromptingBehaviorPref() override;

  extensions::settings_private::SetPrefResult SetPref(
      const base::Value* value) override;
  extensions::api::settings_private::PrefObject GetPrefObject() const override;

  void OnContentSettingChanged(
      const ContentSettingsPattern& primary_pattern,
      const ContentSettingsPattern& secondary_pattern,
      ContentSettingsTypeSet content_type_set) override;

  void OnPreferencesChanged();

 private:
  const raw_ptr<Profile> profile_;
  raw_ptr<HostContentSettingsMap> host_content_settings_map_;
  base::ScopedObservation<HostContentSettingsMap, content_settings::Observer>
      content_setting_observation_{this};
  PrefChangeRegistrar user_prefs_registrar_;
  ContentSettingsType content_settings_type_;
  std::string quiet_ui_pref_name_;
  std::string generated_pref_name_;
  std::string cpss_pref_name_;
};

}  // namespace content_settings

#endif  // CHROME_BROWSER_CONTENT_SETTINGS_GENERATED_PERMISSION_PROMPTING_BEHAVIOR_PREF_H_
