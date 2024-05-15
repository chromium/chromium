// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/content_settings/generated_permission_prompting_behavior_pref.h"

#include "base/check.h"
#include "chrome/browser/content_settings/host_content_settings_map_factory.h"
#include "chrome/common/extensions/api/settings_private.h"
#include "components/content_settings/core/browser/content_settings_utils.h"
#include "components/content_settings/core/common/content_settings.h"
#include "components/content_settings/core/common/content_settings_types.h"
#include "components/content_settings/core/common/pref_names.h"

namespace settings_api = extensions::api::settings_private;
typedef extensions::settings_private::GeneratedPref GeneratedPref;

namespace content_settings {

const char kGeneratedGeolocationPref[] = "generated.geolocation";
const char kGeneratedNotificationPref[] = "generated.notification";

GeneratedPermissionPromptingBehaviorPref::
    GeneratedPermissionPromptingBehaviorPref(
        Profile* profile,
        ContentSettingsType content_settings_type)
    : profile_(profile), content_settings_type_(content_settings_type) {
  switch (content_settings_type) {
    case ContentSettingsType::NOTIFICATIONS:
      quiet_ui_pref_name_ = prefs::kEnableQuietNotificationPermissionUi;
      generated_pref_name_ = kGeneratedNotificationPref;
      cpss_pref_name_ = prefs::kEnableNotificationCPSS;
      break;
    case ContentSettingsType::GEOLOCATION:
      quiet_ui_pref_name_ = prefs::kEnableQuietGeolocationPermissionUi;
      generated_pref_name_ = kGeneratedGeolocationPref;
      cpss_pref_name_ = prefs::kEnableGeolocationCPSS;
      break;
    default:
      NOTREACHED_IN_MIGRATION();
  }
  user_prefs_registrar_.Init(profile->GetPrefs());
  user_prefs_registrar_.Add(
      quiet_ui_pref_name_,
      base::BindRepeating(
          &GeneratedPermissionPromptingBehaviorPref::OnPreferencesChanged,
          base::Unretained(this)));

  host_content_settings_map_ =
      HostContentSettingsMapFactory::GetForProfile(profile_);
  content_setting_observation_.Observe(host_content_settings_map_.get());
}

GeneratedPermissionPromptingBehaviorPref::
    ~GeneratedPermissionPromptingBehaviorPref() = default;

void GeneratedPermissionPromptingBehaviorPref::OnContentSettingChanged(
    const ContentSettingsPattern& primary_pattern,
    const ContentSettingsPattern& secondary_pattern,
    ContentSettingsTypeSet content_type_set) {
  if (content_type_set.Contains(content_settings_type_)) {
    NotifyObservers(generated_pref_name_);
  }
}

void GeneratedPermissionPromptingBehaviorPref::OnPreferencesChanged() {
  NotifyObservers(generated_pref_name_);
}

extensions::settings_private::SetPrefResult
GeneratedPermissionPromptingBehaviorPref::SetPref(const base::Value* value) {
  if (!value->is_int()) {
    return extensions::settings_private::SetPrefResult::PREF_TYPE_MISMATCH;
  }

  auto selection = static_cast<SettingsState>(value->GetInt());
  if (selection != SettingsState::kCanPromptWithAlwaysLoudUI &&
      selection != SettingsState::kCanPromptWithAlwaysQuietUI &&
      selection != SettingsState::kCanPromptWithCPSS &&
      selection != SettingsState::kBlocked) {
    return extensions::settings_private::SetPrefResult::PREF_TYPE_MISMATCH;
  }

  auto* pref_service = profile_->GetPrefs();

  switch (selection) {
    case SettingsState::kBlocked:
    case SettingsState::kCanPromptWithAlwaysLoudUI:
      pref_service->SetBoolean(quiet_ui_pref_name_, /*value=*/false);
      pref_service->SetBoolean(cpss_pref_name_, /*value=*/false);
      break;
    case SettingsState::kCanPromptWithAlwaysQuietUI:
      pref_service->SetBoolean(quiet_ui_pref_name_, /*value=*/true);
      pref_service->SetBoolean(cpss_pref_name_, /*value=*/false);
      break;
    case SettingsState::kCanPromptWithCPSS:
      pref_service->SetBoolean(quiet_ui_pref_name_, /*value=*/false);
      pref_service->SetBoolean(cpss_pref_name_, /*value=*/true);
      break;
  }

  HostContentSettingsMapFactory::GetForProfile(profile_)
      ->SetDefaultContentSetting(content_settings_type_,
                                 selection == SettingsState::kBlocked
                                     ? ContentSetting::CONTENT_SETTING_BLOCK
                                     : ContentSetting::CONTENT_SETTING_ASK);

  return extensions::settings_private::SetPrefResult::SUCCESS;
}

settings_api::PrefObject
GeneratedPermissionPromptingBehaviorPref::GetPrefObject() const {
  const auto* pref_service = profile_->GetPrefs();
  const bool is_quiet_ui_enabled =
      pref_service->GetBoolean(quiet_ui_pref_name_);
  const bool is_cpss_enabled = pref_service->GetBoolean(cpss_pref_name_);

  // Both the prefs shouldn't be enabled at the same time.
  DCHECK(!(is_cpss_enabled && is_quiet_ui_enabled));

  settings_api::PrefObject pref_object;
  pref_object.key = generated_pref_name_;
  pref_object.type = settings_api::PrefType::kNumber;

  content_settings::ProviderType content_setting_provider;
  const auto content_setting =
      host_content_settings_map_->GetDefaultContentSetting(
          content_settings_type_, &content_setting_provider);
  auto content_setting_source =
      content_settings::GetSettingSourceFromProviderType(
          content_setting_provider);
  const bool content_setting_managed =
      content_setting_source != content_settings::SettingSource::kUser;

  if (content_setting == CONTENT_SETTING_ASK && !content_setting_managed) {
    if (is_quiet_ui_enabled) {
      pref_object.value = base::Value(
          static_cast<int>(SettingsState::kCanPromptWithAlwaysQuietUI));
    } else if (is_cpss_enabled) {
      pref_object.value =
          base::Value(static_cast<int>(SettingsState::kCanPromptWithCPSS));
    } else {
      pref_object.value = base::Value(
          static_cast<int>(SettingsState::kCanPromptWithAlwaysLoudUI));
    }
  } else if (content_setting == CONTENT_SETTING_ASK ||
             content_setting == CONTENT_SETTING_ALLOW) {
    pref_object.value = base::Value(
        static_cast<int>(SettingsState::kCanPromptWithAlwaysLoudUI));
  } else {
    pref_object.value = base::Value(static_cast<int>(SettingsState::kBlocked));
  }

  if (content_setting_managed) {
    pref_object.enforcement = settings_api::Enforcement::kEnforced;
    GeneratedPref::ApplyControlledByFromContentSettingSource(
        &pref_object, SettingSource::kPolicy);
  }
  return pref_object;
}

}  // namespace content_settings
