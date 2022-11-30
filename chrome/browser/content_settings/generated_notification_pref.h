// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CONTENT_SETTINGS_GENERATED_NOTIFICATION_PREF_H_
#define CHROME_BROWSER_CONTENT_SETTINGS_GENERATED_NOTIFICATION_PREF_H_

#include "base/memory/raw_ptr.h"
#include "chrome/browser/extensions/api/settings_private/generated_pref.h"

#include "base/scoped_observation.h"
#include "chrome/browser/profiles/profile.h"
#include "components/content_settings/core/browser/content_settings_observer.h"
#include "components/content_settings/core/browser/host_content_settings_map.h"
#include "components/prefs/pref_change_registrar.h"

namespace content_settings {

extern const char kGeneratedNotificationPref[];

// Must be kept in sync with the enum of the same name in
// chrome/browser/resources/settings/site_settings/constants.js
enum class NotificationSetting {
  ASK = 0,
  QUIETER_MESSAGING = 1,
  BLOCK = 2,
};

// A generated preference which represents the effective Notification setting
// state based on the Notification content setting and quieter UI user
// preference.
class GeneratedNotificationPref
    : public extensions::settings_private::GeneratedPref,
      public content_settings::Observer {
 public:
  explicit GeneratedNotificationPref(Profile* profile);

  ~GeneratedNotificationPref() override;

  // Generated Preference Interface.
  extensions::settings_private::SetPrefResult SetPref(
      const base::Value* value) override;
  std::unique_ptr<extensions::api::settings_private::PrefObject> GetPrefObject()
      const override;

  void OnContentSettingChanged(
      const ContentSettingsPattern& primary_pattern,
      const ContentSettingsPattern& secondary_pattern,
      ContentSettingsTypeSet content_type_set) override;

  void OnNotificationPreferencesChanged();

 private:
  static void ApplyNotificationManagementState(
      Profile* profile,
      extensions::api::settings_private::PrefObject* pref_object);

  const raw_ptr<Profile> profile_;
  raw_ptr<HostContentSettingsMap> host_content_settings_map_;
  base::ScopedObservation<HostContentSettingsMap, content_settings::Observer>
      content_setting_observation_{this};
  PrefChangeRegistrar user_prefs_registrar_;
};

}  // namespace content_settings

#endif  // CHROME_BROWSER_CONTENT_SETTINGS_GENERATED_NOTIFICATION_PREF_H_
