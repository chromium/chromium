// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/content_settings/generated_notification_pref.h"

#include "chrome/browser/content_settings/host_content_settings_map_factory.h"
#include "chrome/common/extensions/api/settings_private.h"
#include "components/content_settings/core/browser/content_settings_utils.h"
#include "components/content_settings/core/common/content_settings.h"
#include "components/content_settings/core/common/pref_names.h"

namespace settings_api = extensions::api::settings_private;
typedef extensions::settings_private::GeneratedPref GeneratedPref;

namespace content_settings {

const char kGeneratedNotificationPref[] = "generated.notification";

namespace {

bool IsDefaultNotificationContentSettingUserControlled(
    HostContentSettingsMap* map) {
  std::string content_setting_provider;
  map->GetDefaultContentSetting(ContentSettingsType::NOTIFICATIONS,
                                &content_setting_provider);
  auto content_setting_source =
      HostContentSettingsMap::GetSettingSourceFromProviderName(
          content_setting_provider);
  return content_setting_source == SettingSource::SETTING_SOURCE_USER;
}

}  // namespace

GeneratedNotificationPref::GeneratedNotificationPref(Profile* profile)
    : profile_(profile) {
  user_prefs_registrar_.Init(profile->GetPrefs());
  user_prefs_registrar_.Add(
      prefs::kEnableQuietNotificationPermissionUi,
      base::BindRepeating(
          &GeneratedNotificationPref::OnNotificationPreferencesChanged,
          base::Unretained(this)));

  host_content_settings_map_ =
      HostContentSettingsMapFactory::GetForProfile(profile_);
  content_setting_observation_.Observe(host_content_settings_map_.get());
}

GeneratedNotificationPref::~GeneratedNotificationPref() = default;

void GeneratedNotificationPref::OnContentSettingChanged(
    const ContentSettingsPattern& primary_pattern,
    const ContentSettingsPattern& secondary_pattern,
    ContentSettingsTypeSet content_type_set) {
  if (content_type_set.Contains(ContentSettingsType::NOTIFICATIONS)) {
    NotifyObservers(kGeneratedNotificationPref);
  }
}

void GeneratedNotificationPref::OnNotificationPreferencesChanged() {
  NotifyObservers(kGeneratedNotificationPref);
}

extensions::settings_private::SetPrefResult GeneratedNotificationPref::SetPref(
    const base::Value* value) {
  if (!value->is_int())
    return extensions::settings_private::SetPrefResult::PREF_TYPE_MISMATCH;

  auto selection = static_cast<NotificationSetting>(value->GetInt());

  if (selection != NotificationSetting::ASK &&
      selection != NotificationSetting::QUIETER_MESSAGING &&
      selection != NotificationSetting::BLOCK) {
    return extensions::settings_private::SetPrefResult::PREF_TYPE_MISMATCH;
  }

  auto current_content_setting =
      host_content_settings_map_->GetDefaultContentSetting(
          ContentSettingsType::NOTIFICATIONS);
  auto new_content_setting = selection == NotificationSetting::BLOCK
                                 ? ContentSetting::CONTENT_SETTING_BLOCK
                                 : ContentSetting::CONTENT_SETTING_ASK;

  // Do not modify content setting if its setting source is not user.
  // If there's no actual change, the check doesn't apply.
  if (current_content_setting != new_content_setting &&
      !IsDefaultNotificationContentSettingUserControlled(
          host_content_settings_map_)) {
    return extensions::settings_private::SetPrefResult::PREF_NOT_MODIFIABLE;
  }
  if (selection != NotificationSetting::BLOCK) {
    const PrefService::Preference* quieter_pref =
        profile_->GetPrefs()->FindPreference(
            prefs::kEnableQuietNotificationPermissionUi);
    bool quieter_value = quieter_pref->GetValue()->GetBool();
    bool new_quieter_value = selection != NotificationSetting::ASK;

    // Do not modify the preference value if the user is unable to change its
    // value. If there's no actual change, this check doesn't apply.
    if (quieter_value != new_quieter_value &&
        !quieter_pref->IsUserModifiable()) {
      return extensions::settings_private::SetPrefResult::PREF_NOT_MODIFIABLE;
    }
    profile_->GetPrefs()->SetBoolean(
        prefs::kEnableQuietNotificationPermissionUi, new_quieter_value);
  }

  host_content_settings_map_->SetDefaultContentSetting(
      ContentSettingsType::NOTIFICATIONS, new_content_setting);
  return extensions::settings_private::SetPrefResult::SUCCESS;
}

settings_api::PrefObject GeneratedNotificationPref::GetPrefObject() const {
  settings_api::PrefObject pref_object;
  pref_object.key = kGeneratedNotificationPref;
  pref_object.type = settings_api::PREF_TYPE_NUMBER;

  const auto quieter_pref_enabled =
      profile_->GetPrefs()
          ->FindPreference(prefs::kEnableQuietNotificationPermissionUi)
          ->GetValue()
          ->GetBool();
  const auto notification_content_setting =
      host_content_settings_map_->GetDefaultContentSetting(
          ContentSettingsType::NOTIFICATIONS);
  const auto notification_content_setting_enabled =
      notification_content_setting != ContentSetting::CONTENT_SETTING_BLOCK;

  if (notification_content_setting_enabled && quieter_pref_enabled) {
    pref_object.value =
        base::Value(static_cast<int>(NotificationSetting::QUIETER_MESSAGING));
  } else if (notification_content_setting_enabled) {
    pref_object.value = base::Value(static_cast<int>(NotificationSetting::ASK));
  } else {
    DCHECK_EQ(ContentSetting::CONTENT_SETTING_BLOCK,
              notification_content_setting);
    pref_object.value =
        base::Value(static_cast<int>(NotificationSetting::BLOCK));
  }

  ApplyNotificationManagementState(*profile_, pref_object);

  return pref_object;
}

/* static */
void GeneratedNotificationPref::ApplyNotificationManagementState(
    Profile& profile,
    settings_api::PrefObject& pref_object) {
  HostContentSettingsMap* map =
      HostContentSettingsMapFactory::GetForProfile(&profile);
  std::string content_setting_provider;
  auto content_setting = map->GetDefaultContentSetting(
      ContentSettingsType::NOTIFICATIONS, &content_setting_provider);
  auto content_setting_source =
      HostContentSettingsMap::GetSettingSourceFromProviderName(
          content_setting_provider);
  bool content_setting_enforced =
      content_setting_source !=
      content_settings::SettingSource::SETTING_SOURCE_USER;

  const PrefService::Preference* quieter_ui_pref =
      profile.GetPrefs()->FindPreference(
          prefs::kEnableQuietNotificationPermissionUi);
  auto quieter_ui_on = quieter_ui_pref->GetValue()->GetBool();
  auto quieter_ui_enforced = !quieter_ui_pref->IsUserModifiable();
  auto quieter_ui_recommended =
      quieter_ui_pref->GetRecommendedValue() != nullptr;
  auto quieter_ui_recommended_on =
      (quieter_ui_recommended &&
       quieter_ui_pref->GetRecommendedValue()->GetBool());

  if (!content_setting_enforced && !quieter_ui_enforced &&
      !quieter_ui_recommended) {
    // No notification setting is enforced or recommended.
    return;
  }

  if (content_setting_enforced) {
    pref_object.enforcement = settings_api::Enforcement::ENFORCEMENT_ENFORCED;

    if (content_setting == CONTENT_SETTING_BLOCK) {
      // Preference is fully managed by the content setting.
      GeneratedPref::ApplyControlledByFromContentSettingSource(
          &pref_object, content_setting_source);
      return;
    } else if (quieter_ui_enforced) {
      // Preference is fully managed by the content setting and quieter ui pref.
      GeneratedPref::ApplyControlledByFromPref(&pref_object, quieter_ui_pref);
      return;
    }
    GeneratedPref::ApplyControlledByFromContentSettingSource(
        &pref_object, content_setting_source);

    DCHECK(content_setting_enforced && !quieter_ui_enforced);
    // Since content setting is enforced but the quieter ui pref is not,
    // user can choose from 2 options.
    GeneratedPref::AddUserSelectableValue(
        &pref_object, static_cast<int>(NotificationSetting::ASK));
    GeneratedPref::AddUserSelectableValue(
        &pref_object, static_cast<int>(NotificationSetting::QUIETER_MESSAGING));

    if (quieter_ui_recommended) {
      pref_object.recommended_value = base::Value(static_cast<int>(
          quieter_ui_recommended_on ? NotificationSetting::QUIETER_MESSAGING
                                    : NotificationSetting::ASK));
    }
    return;
  }

  if (quieter_ui_enforced) {
    // Quieter ui pref is enforced, but the content setting is not, so the user
    // can choose from 2 options
    pref_object.enforcement = settings_api::Enforcement::ENFORCEMENT_ENFORCED;
    GeneratedPref::ApplyControlledByFromPref(&pref_object, quieter_ui_pref);
    GeneratedPref::AddUserSelectableValue(
        &pref_object,
        static_cast<int>(quieter_ui_on ? NotificationSetting::QUIETER_MESSAGING
                                       : NotificationSetting::ASK));
    GeneratedPref::AddUserSelectableValue(
        &pref_object, static_cast<int>(NotificationSetting::BLOCK));
  }
  // If neither of notification content setting nor quieter ui preference is
  // enforced, but quieter ui preference is recommended, then recommended value
  // is ignored since it's ambiguous given that notification content setting
  // can be blocked.
}

}  // namespace content_settings
