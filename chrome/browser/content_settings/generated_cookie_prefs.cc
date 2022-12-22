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

namespace settings_api = extensions::api::settings_private;
namespace settings_private = extensions::settings_private;

namespace content_settings {

namespace {

bool IsDefaultCookieContentSettingUserControlled(HostContentSettingsMap* map) {
  std::string content_setting_provider;
  map->GetDefaultContentSetting(ContentSettingsType::COOKIES,
                                &content_setting_provider);
  auto content_setting_source =
      HostContentSettingsMap::GetSettingSourceFromProviderName(
          content_setting_provider);
  return content_setting_source == SettingSource::SETTING_SOURCE_USER;
}

// Updates all user modifiable cookie content settings and preferences to match
// the provided |controls_mode| and |content_setting|. This provides a
// consistent interface to updating these when they are partially managed.
// Returns SetPrefResult::SUCCESS if any settings could be changed, and
// SetPrefResult::PREF_NOT_MODIFIABLE if no setting could be changed.
extensions::settings_private::SetPrefResult SetAllCookieSettings(
    Profile* profile,
    CookieControlsMode controls_mode,
    ContentSetting content_setting) {
  bool setting_changed = false;

  auto* map = HostContentSettingsMapFactory::GetForProfile(profile);
  if (IsDefaultCookieContentSettingUserControlled(map)) {
    map->SetDefaultContentSetting(ContentSettingsType::COOKIES,
                                  content_setting);
    setting_changed = true;
  }

  auto* pref_service = profile->GetPrefs();
  if (pref_service->FindPreference(prefs::kCookieControlsMode)
          ->IsUserModifiable()) {
    pref_service->SetInteger(prefs::kCookieControlsMode,
                             static_cast<int>(controls_mode));
    setting_changed = true;
  }

  return setting_changed
             ? extensions::settings_private::SetPrefResult::SUCCESS
             : extensions::settings_private::SetPrefResult::PREF_NOT_MODIFIABLE;
}

CookiePrimarySetting ToCookiePrimarySetting(
    CookieControlsMode cookie_controls_mode) {
  switch (cookie_controls_mode) {
    case CookieControlsMode::kBlockThirdParty:
      return CookiePrimarySetting::BLOCK_THIRD_PARTY;
    case CookieControlsMode::kIncognitoOnly:
      return CookiePrimarySetting::BLOCK_THIRD_PARTY_INCOGNITO;
    case CookieControlsMode::kOff:
      return CookiePrimarySetting::ALLOW_ALL;
  }
  NOTREACHED();
}

}  // namespace

const char kCookiePrimarySetting[] = "generated.cookie_primary_setting";
const char kCookieSessionOnly[] = "generated.cookie_session_only";
const char kCookieDefaultContentSetting[] =
    "generated.cookie_default_content_setting";

GeneratedCookiePrefBase::GeneratedCookiePrefBase(Profile* profile,
                                                 const std::string& pref_name)
    : profile_(profile), pref_name_(pref_name) {
  host_content_settings_map_ =
      HostContentSettingsMapFactory::GetForProfile(profile_);
  content_settings_observation_.Observe(host_content_settings_map_.get());

  user_prefs_registrar_.Init(profile->GetPrefs());
  user_prefs_registrar_.Add(
      prefs::kCookieControlsMode,
      base::BindRepeating(&GeneratedCookiePrefBase::OnCookiePreferencesChanged,
                          base::Unretained(this)));
}

GeneratedCookiePrefBase::~GeneratedCookiePrefBase() = default;

void GeneratedCookiePrefBase::OnContentSettingChanged(
    const ContentSettingsPattern& primary_pattern,
    const ContentSettingsPattern& secondary_pattern,
    ContentSettingsTypeSet content_type_set) {
  if (content_type_set.Contains(ContentSettingsType::COOKIES)) {
    NotifyObservers(pref_name_);
  }
}

void GeneratedCookiePrefBase::OnCookiePreferencesChanged() {
  NotifyObservers(pref_name_);
}

GeneratedCookiePrimarySettingPref::GeneratedCookiePrimarySettingPref(
    Profile* profile)
    : GeneratedCookiePrefBase(profile, kCookiePrimarySetting) {}

extensions::settings_private::SetPrefResult
GeneratedCookiePrimarySettingPref::SetPref(const base::Value* value) {
  if (!value->is_int())
    return extensions::settings_private::SetPrefResult::PREF_TYPE_MISMATCH;

  auto current_content_setting =
      host_content_settings_map_->GetDefaultContentSetting(
          ContentSettingsType::COOKIES, nullptr);

  auto allow_setting =
      current_content_setting != ContentSetting::CONTENT_SETTING_BLOCK
          ? current_content_setting
          : ContentSetting::CONTENT_SETTING_ALLOW;

  auto selection = static_cast<CookiePrimarySetting>(value->GetInt());
  switch (selection) {
    case (CookiePrimarySetting::ALLOW_ALL):
      return SetAllCookieSettings(profile_, CookieControlsMode::kOff,
                                  allow_setting);
    case (CookiePrimarySetting::BLOCK_THIRD_PARTY_INCOGNITO):
      return SetAllCookieSettings(profile_, CookieControlsMode::kIncognitoOnly,
                                  allow_setting);
    case (CookiePrimarySetting::BLOCK_THIRD_PARTY):
      return SetAllCookieSettings(
          profile_, CookieControlsMode::kBlockThirdParty, allow_setting);
    case (CookiePrimarySetting::BLOCK_ALL):
      return SetAllCookieSettings(profile_,
                                  CookieControlsMode::kBlockThirdParty,
                                  ContentSetting::CONTENT_SETTING_BLOCK);
    default:
      return extensions::settings_private::SetPrefResult::PREF_TYPE_MISMATCH;
  }
}

std::unique_ptr<extensions::api::settings_private::PrefObject>
GeneratedCookiePrimarySettingPref::GetPrefObject() const {
  auto pref_object =
      std::make_unique<extensions::api::settings_private::PrefObject>();
  pref_object->key = pref_name_;
  pref_object->type = extensions::api::settings_private::PREF_TYPE_NUMBER;

  auto content_setting = host_content_settings_map_->GetDefaultContentSetting(
      ContentSettingsType::COOKIES, nullptr);

  auto cookie_controls_mode = static_cast<CookieControlsMode>(
      profile_->GetPrefs()->GetInteger(prefs::kCookieControlsMode));

  if (content_setting == ContentSetting::CONTENT_SETTING_BLOCK) {
    pref_object->value =
        base::Value(static_cast<int>(CookiePrimarySetting::BLOCK_ALL));
  } else {
    pref_object->value = base::Value(
        static_cast<int>(ToCookiePrimarySetting(cookie_controls_mode)));
  }

  ApplyPrimaryCookieSettingManagedState(pref_object.get(), profile_);

  // Ensure that if any user selectable values were added, at least two values
  // were, so the user is able to select between them.
  DCHECK(!pref_object->user_selectable_values ||
         pref_object->user_selectable_values->size() >= 2);

  if (pref_object->user_selectable_values) {
    // Sort user selectable values to make interacting with them simpler in C++.
    // This is not required by the SettingsPrivate API, but is expected in the
    // unit_tests associated with this file.
    std::sort(pref_object->user_selectable_values->begin(),
              pref_object->user_selectable_values->end(),
              [](const base::Value& a, const base::Value& b) {
                return a.GetInt() < b.GetInt();
              });
  }
  return pref_object;
}

/* static */
void GeneratedCookiePrimarySettingPref::ApplyPrimaryCookieSettingManagedState(
    settings_api::PrefObject* pref_object,
    Profile* profile) {
  HostContentSettingsMap* map =
      HostContentSettingsMapFactory::GetForProfile(profile);
  std::string content_setting_provider;
  auto content_setting = map->GetDefaultContentSetting(
      ContentSettingsType::COOKIES, &content_setting_provider);
  auto content_setting_source =
      HostContentSettingsMap::GetSettingSourceFromProviderName(
          content_setting_provider);
  bool content_setting_enforced =
      content_setting_source !=
      content_settings::SettingSource::SETTING_SOURCE_USER;

  // Both the content setting and the block_third_party preference can
  // be controlled via policy.
  const PrefService::Preference* cookie_controls_mode_pref =
      profile->GetPrefs()->FindPreference(prefs::kCookieControlsMode);
  bool cookie_controls_mode_enforced =
      !cookie_controls_mode_pref->IsUserModifiable();
  // IsRecommended() cannot be used as we care if a recommended value exists at
  // all, even if a user has overwritten it.
  bool cookie_controls_mode_recommended =
      cookie_controls_mode_pref->GetRecommendedValue();

  if (!content_setting_enforced && !cookie_controls_mode_enforced &&
      !cookie_controls_mode_recommended) {
    // No cookie controls are managed or recommended.
    return;
  }

  if (content_setting_enforced && content_setting == CONTENT_SETTING_BLOCK) {
    // Preference is fully managed by the content setting.
    pref_object->enforcement = settings_api::Enforcement::ENFORCEMENT_ENFORCED;
    settings_private::GeneratedPref::ApplyControlledByFromContentSettingSource(
        pref_object, content_setting_source);
    return;
  }

  if (content_setting_enforced && cookie_controls_mode_enforced) {
    // Preference is considered fully managed by the third party preference.
    pref_object->enforcement = settings_api::Enforcement::ENFORCEMENT_ENFORCED;
    settings_private::GeneratedPref::ApplyControlledByFromPref(
        pref_object, cookie_controls_mode_pref);
    return;
  }

  // At this stage the content setting is not enforcing a BLOCK state. Given
  // this, allow and block_third_party are still valid choices that do not
  // contradict the content setting. They can thus be controlled or recommended
  // by the block_third_party preference.
  DCHECK(!content_setting_enforced || !cookie_controls_mode_enforced);

  if (cookie_controls_mode_recommended) {
    auto recommended_value = static_cast<CookieControlsMode>(
        cookie_controls_mode_pref->GetRecommendedValue()->GetInt());
    pref_object->recommended_value = base::Value(
        static_cast<int>(ToCookiePrimarySetting(recommended_value)));

    // Based on state assessed so far the enforcement is only recommended. This
    // may be changed to ENFORCED later in this function.
    pref_object->enforcement =
        settings_api::Enforcement::ENFORCEMENT_RECOMMENDED;
  }

  // If cookie controls are enforced and the content settings is not enforced,
  // you can choose between the selected cookie controls setting and "BLOCK"
  if (cookie_controls_mode_enforced) {
    pref_object->enforcement = settings_api::Enforcement::ENFORCEMENT_ENFORCED;
    settings_private::GeneratedPref::ApplyControlledByFromPref(
        pref_object, cookie_controls_mode_pref);
    auto value = static_cast<CookieControlsMode>(
        cookie_controls_mode_pref->GetValue()->GetInt());
    settings_private::GeneratedPref::AddUserSelectableValue(
        pref_object, static_cast<int>(ToCookiePrimarySetting(value)));
    settings_private::GeneratedPref::AddUserSelectableValue(
        pref_object, static_cast<int>(CookiePrimarySetting::BLOCK_ALL));
    return;
  }

  // The content setting is enforced to either ALLOW OR SESSION_ONLY
  if (content_setting_enforced) {
    DCHECK(content_setting == CONTENT_SETTING_ALLOW ||
           content_setting == CONTENT_SETTING_SESSION_ONLY);
    pref_object->enforcement = settings_api::Enforcement::ENFORCEMENT_ENFORCED;
    settings_private::GeneratedPref::ApplyControlledByFromContentSettingSource(
        pref_object, content_setting_source);

    settings_private::GeneratedPref::AddUserSelectableValue(
        pref_object, static_cast<int>(CookiePrimarySetting::ALLOW_ALL));
    settings_private::GeneratedPref::AddUserSelectableValue(
        pref_object, static_cast<int>(CookiePrimarySetting::BLOCK_THIRD_PARTY));
    settings_private::GeneratedPref::AddUserSelectableValue(
        pref_object,
        static_cast<int>(CookiePrimarySetting::BLOCK_THIRD_PARTY_INCOGNITO));
  }
}

GeneratedCookieSessionOnlyPref::GeneratedCookieSessionOnlyPref(Profile* profile)
    : GeneratedCookiePrefBase(profile, kCookieSessionOnly) {}

extensions::settings_private::SetPrefResult
GeneratedCookieSessionOnlyPref::SetPref(const base::Value* value) {
  if (!value->is_bool())
    return extensions::settings_private::SetPrefResult::PREF_TYPE_MISMATCH;

  if (!IsDefaultCookieContentSettingUserControlled(host_content_settings_map_))
    return extensions::settings_private::SetPrefResult::PREF_NOT_MODIFIABLE;

  if (host_content_settings_map_->GetDefaultContentSetting(
          ContentSettingsType::COOKIES, nullptr) ==
      ContentSetting::CONTENT_SETTING_BLOCK)
    return extensions::settings_private::SetPrefResult::PREF_NOT_MODIFIABLE;

  host_content_settings_map_->SetDefaultContentSetting(
      ContentSettingsType::COOKIES,
      value->GetBool() ? ContentSetting::CONTENT_SETTING_SESSION_ONLY
                       : ContentSetting::CONTENT_SETTING_ALLOW);

  return extensions::settings_private::SetPrefResult::SUCCESS;
}

std::unique_ptr<settings_api::PrefObject>
GeneratedCookieSessionOnlyPref::GetPrefObject() const {
  auto pref_object = std::make_unique<settings_api::PrefObject>();
  pref_object->key = pref_name_;
  pref_object->type = settings_api::PREF_TYPE_BOOLEAN;

  std::string content_setting_provider;
  auto content_setting = host_content_settings_map_->GetDefaultContentSetting(
      ContentSettingsType::COOKIES, &content_setting_provider);

  pref_object->user_control_disabled =
      content_setting == ContentSetting::CONTENT_SETTING_BLOCK;
  pref_object->value = base::Value(
      content_setting == ContentSetting::CONTENT_SETTING_SESSION_ONLY);

  // Content settings can be managed via policy, extension or supervision, but
  // cannot be recommended.
  auto content_setting_source =
      HostContentSettingsMap::GetSettingSourceFromProviderName(
          content_setting_provider);
  if (content_setting_source == SettingSource::SETTING_SOURCE_POLICY) {
    pref_object->controlled_by =
        settings_api::ControlledBy::CONTROLLED_BY_DEVICE_POLICY;
    pref_object->enforcement = settings_api::Enforcement::ENFORCEMENT_ENFORCED;
  }
  if (content_setting_source == SettingSource::SETTING_SOURCE_EXTENSION) {
    pref_object->controlled_by =
        settings_api::ControlledBy::CONTROLLED_BY_EXTENSION;
    pref_object->enforcement = settings_api::Enforcement::ENFORCEMENT_ENFORCED;
  }
  if (content_setting_source == SettingSource::SETTING_SOURCE_SUPERVISED) {
    pref_object->controlled_by =
        settings_api::ControlledBy::CONTROLLED_BY_CHILD_RESTRICTION;
    pref_object->enforcement = settings_api::Enforcement::ENFORCEMENT_ENFORCED;
  }

  return pref_object;
}

GeneratedCookieDefaultContentSettingPref::
    GeneratedCookieDefaultContentSettingPref(Profile* profile)
    : GeneratedCookiePrefBase(profile, kCookieDefaultContentSetting) {}

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

  if (!IsDefaultCookieContentSettingUserControlled(host_content_settings_map_))
    return extensions::settings_private::SetPrefResult::PREF_NOT_MODIFIABLE;

  host_content_settings_map_->SetDefaultContentSetting(
      ContentSettingsType::COOKIES, static_cast<ContentSetting>(setting));

  return extensions::settings_private::SetPrefResult::SUCCESS;
}

std::unique_ptr<settings_api::PrefObject>
GeneratedCookieDefaultContentSettingPref::GetPrefObject() const {
  auto pref_object = std::make_unique<settings_api::PrefObject>();
  pref_object->key = pref_name_;
  pref_object->type = settings_api::PREF_TYPE_STRING;

  std::string content_setting_provider;
  auto content_setting = host_content_settings_map_->GetDefaultContentSetting(
      ContentSettingsType::COOKIES, &content_setting_provider);

  pref_object->value =
      base::Value(content_settings::ContentSettingToString(content_setting));

  // Cookies content setting can be managed via policy, extension or
  // supervision, but cannot be recommended.
  auto content_setting_source =
      HostContentSettingsMap::GetSettingSourceFromProviderName(
          content_setting_provider);
  if (content_setting_source == SettingSource::SETTING_SOURCE_POLICY) {
    pref_object->controlled_by =
        settings_api::ControlledBy::CONTROLLED_BY_DEVICE_POLICY;
    pref_object->enforcement = settings_api::Enforcement::ENFORCEMENT_ENFORCED;
  }
  if (content_setting_source == SettingSource::SETTING_SOURCE_EXTENSION) {
    pref_object->controlled_by =
        settings_api::ControlledBy::CONTROLLED_BY_EXTENSION;
    pref_object->enforcement = settings_api::Enforcement::ENFORCEMENT_ENFORCED;
  }
  if (content_setting_source == SettingSource::SETTING_SOURCE_SUPERVISED) {
    pref_object->controlled_by =
        settings_api::ControlledBy::CONTROLLED_BY_CHILD_RESTRICTION;
    pref_object->enforcement = settings_api::Enforcement::ENFORCEMENT_ENFORCED;
  }

  return pref_object;
}

}  // namespace content_settings
