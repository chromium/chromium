// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CONTENT_SETTINGS_GENERATED_NOTIFICATION_PREF_H_
#define CHROME_BROWSER_CONTENT_SETTINGS_GENERATED_NOTIFICATION_PREF_H_

#include "chrome/browser/extensions/api/settings_private/generated_pref.h"

#include "base/scoped_observer.h"
#include "chrome/browser/profiles/profile.h"
#include "components/content_settings/core/browser/content_settings_observer.h"
#include "components/content_settings/core/browser/host_content_settings_map.h"
#include "components/prefs/pref_change_registrar.h"

namespace content_settings {

extern const char kGeneratedNotificationPref[];

enum class NotificationSetting { ASK, QUIETER_MESSAGING, BLOCK };

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

  void OnContentSettingChanged(const ContentSettingsPattern& primary_pattern,
                               const ContentSettingsPattern& secondary_pattern,
                               ContentSettingsType content_type,
                               const std::string& resource_identifier) override;

  void OnNotificationPreferencesChanged();

 private:
  static void ApplyNotificationManagementState(
      Profile* profile,
      extensions::api::settings_private::PrefObject* pref_object);

  Profile* const profile_;
  HostContentSettingsMap* host_content_settings_map_;
  ScopedObserver<HostContentSettingsMap, content_settings::Observer>
      content_setting_observer_{this};
  PrefChangeRegistrar user_prefs_registrar_;
};

}  // namespace content_settings

#endif  // CHROME_BROWSER_CONTENT_SETTINGS_GENERATED_NOTIFICATION_PREF_H_
