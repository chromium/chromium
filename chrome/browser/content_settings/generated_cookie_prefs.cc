// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/content_settings/generated_cookie_prefs.h"

#include "base/notreached.h"
#include "chrome/browser/content_settings/host_content_settings_map_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/common/extensions/api/settings_private.h"
#include "components/content_settings/core/browser/cookie_settings.h"
#include "components/content_settings/core/browser/host_content_settings_map.h"
#include "components/content_settings/core/common/content_settings.h"
#include "components/content_settings/core/common/pref_names.h"
#include "components/prefs/pref_service.h"

namespace content_settings {
namespace {

namespace settings_api = ::extensions::api::settings_private;
namespace settings_private = ::extensions::settings_private;
using settings_api::ControlledBy;
using enum settings_api::Enforcement;

bool IsDefaultCookieContentSettingUserControlled(HostContentSettingsMap* map) {
  content_settings::ProviderType content_setting_provider;
  map->GetDefaultContentSetting(ContentSettingsType::COOKIES,
                                &content_setting_provider);
  auto content_setting_source =
      content_settings::GetSettingSourceFromProviderType(
          content_setting_provider);
  return content_setting_source == SettingSource::kUser;
}

settings_api::Enforcement GetEnforcement(
    const PrefService::Preference* cookie_controls_mode_pref) {
  if (cookie_controls_mode_pref->IsManaged() ||
      cookie_controls_mode_pref->IsExtensionControlled()) {
    return kEnforced;
  }
  if (cookie_controls_mode_pref->IsManagedByCustodian()) {
    return kParentSupervised;
  }
  if (cookie_controls_mode_pref->IsRecommended()) {
    return kRecommended;
  }
  return settings_api::Enforcement::kNone;
}
}  // namespace

const char kCookieDefaultContentSetting[] =
    "generated.cookie_default_content_setting";
const char kThirdPartyCookieBlockingSetting[] =
    "generated.third_party_cookie_blocking_setting";

GeneratedCookieDefaultContentSettingPref::
    GeneratedCookieDefaultContentSettingPref(Profile* profile)
    : profile_(profile) {
  host_content_settings_map_ =
      HostContentSettingsMapFactory::GetForProfile(profile_);
  content_settings_observation_.Observe(host_content_settings_map_.get());
}

GeneratedCookieDefaultContentSettingPref::
    ~GeneratedCookieDefaultContentSettingPref() = default;

void GeneratedCookieDefaultContentSettingPref::OnContentSettingChanged(
    const ContentSettingsPattern& primary_pattern,
    const ContentSettingsPattern& secondary_pattern,
    ContentSettingsTypeSet content_type_set) {
  if (content_type_set.Contains(ContentSettingsType::COOKIES)) {
    NotifyObservers(kCookieDefaultContentSetting);
  }
}

extensions::settings_private::SetPrefResult
GeneratedCookieDefaultContentSettingPref::SetPref(const base::Value* value) {
  if (!value->is_string()) {
    return extensions::settings_private::SetPrefResult::PREF_TYPE_MISMATCH;
  }

  ContentSetting setting;
  if (!content_settings::ContentSettingFromString(value->GetString(),
                                                  &setting)) {
    return extensions::settings_private::SetPrefResult::PREF_TYPE_MISMATCH;
  }
  if (setting != CONTENT_SETTING_ALLOW &&
      setting != CONTENT_SETTING_SESSION_ONLY &&
      setting != CONTENT_SETTING_BLOCK) {
    return extensions::settings_private::SetPrefResult::PREF_TYPE_MISMATCH;
  }

  if (!IsDefaultCookieContentSettingUserControlled(
          host_content_settings_map_)) {
    return extensions::settings_private::SetPrefResult::PREF_NOT_MODIFIABLE;
  }

  host_content_settings_map_->SetDefaultContentSetting(
      ContentSettingsType::COOKIES, static_cast<ContentSetting>(setting));

  return extensions::settings_private::SetPrefResult::SUCCESS;
}

settings_api::PrefObject
GeneratedCookieDefaultContentSettingPref::GetPrefObject() const {
  settings_api::PrefObject pref_object;
  pref_object.key = kCookieDefaultContentSetting;
  pref_object.type = settings_api::PrefType::kString;

  content_settings::ProviderType content_setting_provider;
  auto content_setting = host_content_settings_map_->GetDefaultContentSetting(
      ContentSettingsType::COOKIES, &content_setting_provider);

  pref_object.value =
      base::Value(content_settings::ContentSettingToString(content_setting));

  // Cookies content setting can be managed via policy, extension or
  // supervision, but cannot be recommended.
  auto content_setting_source =
      content_settings::GetSettingSourceFromProviderType(
          content_setting_provider);
  if (content_setting_source == SettingSource::kPolicy) {
    pref_object.controlled_by = ControlledBy::kDevicePolicy;
    pref_object.enforcement = kEnforced;
  }
  if (content_setting_source == SettingSource::kExtension) {
    pref_object.controlled_by = ControlledBy::kExtension;
    pref_object.enforcement = kEnforced;
  }
  if (content_setting_source == SettingSource::kSupervised) {
    pref_object.controlled_by = ControlledBy::kChildRestriction;
    pref_object.enforcement = kEnforced;
  }

  return pref_object;
}

GeneratedThirdPartyCookieBlockingSettingPref::
    GeneratedThirdPartyCookieBlockingSettingPref(Profile* profile)
    : profile_(profile) {
  user_prefs_registrar_.Init(profile->GetPrefs());
  user_prefs_registrar_.Add(
      prefs::kCookieControlsMode,
      base::BindRepeating(&GeneratedThirdPartyCookieBlockingSettingPref::
                              OnSourcePreferencesChanged,
                          base::Unretained(this)));
}

GeneratedThirdPartyCookieBlockingSettingPref::
    ~GeneratedThirdPartyCookieBlockingSettingPref() = default;

ThirdPartyCookieBlockingSetting
GeneratedThirdPartyCookieBlockingSettingPref::GetValue() const {
  switch (static_cast<CookieControlsMode>(
      profile_->GetPrefs()->GetInteger(prefs::kCookieControlsMode))) {
    case CookieControlsMode::kBlockThirdParty:
      return ThirdPartyCookieBlockingSetting::BLOCK_THIRD_PARTY;
    default:
      return ThirdPartyCookieBlockingSetting::INCOGNITO_ONLY;
  }
}

extensions::settings_private::SetPrefResult
GeneratedThirdPartyCookieBlockingSettingPref::SetPrefResult(
    CookieControlsMode value) {
  profile_->GetPrefs()->SetInteger(prefs::kCookieControlsMode,
                                   static_cast<int>(value));
  return extensions::settings_private::SetPrefResult::SUCCESS;
}

extensions::settings_private::SetPrefResult
GeneratedThirdPartyCookieBlockingSettingPref::SetPref(
    const base::Value* value) {
  if (!value->is_int()) {
    return extensions::settings_private::SetPrefResult::PREF_TYPE_MISMATCH;
  }
  if (!profile_->GetPrefs()
           ->FindPreference(prefs::kCookieControlsMode)
           ->IsUserModifiable()) {
    return extensions::settings_private::SetPrefResult::PREF_NOT_MODIFIABLE;
  }

  switch (static_cast<ThirdPartyCookieBlockingSetting>(value->GetInt())) {
    case ThirdPartyCookieBlockingSetting::INCOGNITO_ONLY:
      return SetPrefResult(CookieControlsMode::kIncognitoOnly);
    case ThirdPartyCookieBlockingSetting::BLOCK_THIRD_PARTY:
      return SetPrefResult(CookieControlsMode::kBlockThirdParty);
    default:
      return extensions::settings_private::SetPrefResult::PREF_TYPE_MISMATCH;
  }
}

settings_api::PrefObject
GeneratedThirdPartyCookieBlockingSettingPref::GetPrefObject() const {
  settings_api::PrefObject pref_object;
  const PrefService::Preference* cookie_controls_mode_pref =
      profile_->GetPrefs()->FindPreference(prefs::kCookieControlsMode);

  pref_object.key = kThirdPartyCookieBlockingSetting;
  pref_object.type = settings_api::PrefType::kNumber;
  pref_object.value = base::Value(static_cast<int>(GetValue()));
  pref_object.enforcement = GetEnforcement(cookie_controls_mode_pref);
  if (!cookie_controls_mode_pref->IsUserModifiable()) {
    GeneratedPref::ApplyControlledByFromPref(&pref_object,
                                             cookie_controls_mode_pref);
  }

  return pref_object;
}

void GeneratedThirdPartyCookieBlockingSettingPref::
    OnSourcePreferencesChanged() {
  NotifyObservers(kThirdPartyCookieBlockingSetting);
}

}  // namespace content_settings
