// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/android/search_permissions/search_permissions_service.h"

#include <memory>

#include "base/feature_list.h"
#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/memory/raw_ptr.h"
#include "base/strings/utf_string_conversions.h"
#include "base/values.h"
#include "chrome/browser/content_settings/host_content_settings_map_factory.h"
#include "chrome/browser/permissions/permission_decision_auto_blocker_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/search_engines/template_url_service_factory.h"
#include "chrome/browser/search_engines/ui_thread_search_terms_data.h"
#include "chrome/common/pref_names.h"
#include "components/content_settings/core/browser/content_settings_utils.h"
#include "components/content_settings/core/browser/host_content_settings_map.h"
#include "components/content_settings/core/common/content_settings_types.h"
#include "components/permissions/features.h"
#include "components/permissions/permission_decision_auto_blocker.h"
#include "components/permissions/permission_uma_util.h"
#include "components/pref_registry/pref_registry_syncable.h"
#include "components/prefs/pref_service.h"
#include "components/search_engines/template_url.h"
#include "components/search_engines/template_url_service.h"
#include "url/gurl.h"
#include "url/url_constants.h"

namespace {

const char kDSENameKey[] = "dse_name";
const char kDSEOriginKey[] = "dse_origin";
const char kDSEGeolocationSettingKey[] = "geolocation_setting_to_restore";
const char kDSENotificationsSettingKey[] = "notifications_setting_to_restore";

// Default implementation of SearchEngineDelegate that is used for production
// code.
class SearchEngineDelegateImpl
    : public SearchPermissionsService::SearchEngineDelegate {
 public:
  explicit SearchEngineDelegateImpl(Profile* profile)
      : profile_(profile),
        template_url_service_(
            TemplateURLServiceFactory::GetForProfile(profile_)) {}

  std::u16string GetDSEName() override {
    if (template_url_service_) {
      const TemplateURL* template_url =
          template_url_service_->GetDefaultSearchProvider();
      if (template_url)
        return template_url->short_name();
    }

    return std::u16string();
  }

  url::Origin GetDSEOrigin() override {
    if (template_url_service_) {
      return template_url_service_->GetDefaultSearchProviderOrigin();
    }

    return url::Origin();
  }

 private:
  raw_ptr<Profile> profile_;

  // Will be null in unittests.
  raw_ptr<TemplateURLService> template_url_service_;
};

}  // namespace

struct SearchPermissionsService::PrefValue {
  std::u16string dse_name;
  std::string dse_origin;
  ContentSetting geolocation_setting_to_restore;
  ContentSetting notifications_setting_to_restore;
};

// static
SearchPermissionsService*
SearchPermissionsService::Factory::GetForBrowserContext(
    content::BrowserContext* context) {
  return static_cast<SearchPermissionsService*>(
      GetInstance()->GetServiceForBrowserContext(context, true));
}

// static
SearchPermissionsService::Factory*
SearchPermissionsService::Factory::GetInstance() {
  return base::Singleton<SearchPermissionsService::Factory>::get();
}

SearchPermissionsService::Factory::Factory()
    : ProfileKeyedServiceFactory(
          "SearchPermissionsService",
          ProfileSelections::Builder()
              .WithRegular(ProfileSelection::kOriginalOnly)
              // TODO(crbug.com/40257657): Check if this service is needed in
              // Guest mode.
              .WithGuest(ProfileSelection::kOriginalOnly)
              .Build()) {
  DependsOn(HostContentSettingsMapFactory::GetInstance());
  DependsOn(TemplateURLServiceFactory::GetInstance());
}

SearchPermissionsService::Factory::~Factory() {}

bool SearchPermissionsService::Factory::ServiceIsCreatedWithBrowserContext()
    const {
  return true;
}

KeyedService* SearchPermissionsService::Factory::BuildServiceInstanceFor(
    content::BrowserContext* context) const {
  return new SearchPermissionsService(Profile::FromBrowserContext(context));
}

void SearchPermissionsService::Factory::RegisterProfilePrefs(
    user_prefs::PrefRegistrySyncable* registry) {
  registry->RegisterDictionaryPref(prefs::kDSEGeolocationSettingDeprecated);
  registry->RegisterDictionaryPref(prefs::kDSEPermissionsSettings);
  registry->RegisterBooleanPref(prefs::kDSEWasDisabledByPolicy, false);
}

SearchPermissionsService::SearchPermissionsService(Profile* profile)
    : profile_(profile),
      pref_service_(profile_->GetPrefs()),
      host_content_settings_map_(
          HostContentSettingsMapFactory::GetForProfile(profile_)) {
  // This class should never be constructed in incognito.
  DCHECK(!profile_->IsOffTheRecord());

  delegate_ = std::make_unique<SearchEngineDelegateImpl>(profile_);

  InitializeSettingsIfNeeded();
}

bool SearchPermissionsService::IsDseOrigin(const url::Origin& origin) {
  return origin.scheme() == url::kHttpsScheme &&
         origin.IsSameOriginWith(delegate_->GetDSEOrigin());
}

void SearchPermissionsService::Shutdown() {
  delegate_.reset();
}

SearchPermissionsService::~SearchPermissionsService() {}

ContentSetting SearchPermissionsService::RestoreOldSettingAndReturnPrevious(
    const GURL& dse_origin,
    ContentSettingsType type,
    ContentSetting setting_to_restore,
    bool preserve_block_setting) {
  // Read the current value of the old DSE. This is the DSE setting that we want
  // to try to apply to the new DSE origin.
  ContentSetting dse_setting = GetContentSetting(dse_origin, type);

  // The user's setting should never be ASK while an origin is the DSE. There
  // should be no way for the user to reset the DSE content setting to ASK.
  if (dse_setting == CONTENT_SETTING_ASK) {
    // The style guide suggests not to handle cases which are invalid code paths
    // however in this case there are security risks to state being invalid so
    // we ensure the dse_setting is reverted to BLOCK.
    dse_setting = CONTENT_SETTING_BLOCK;
  }

  // If `preserve_block_setting` is set we don't restore a "BLOCK" setting.
  if (dse_setting == CONTENT_SETTING_BLOCK && preserve_block_setting)
    setting_to_restore = CONTENT_SETTING_BLOCK;

  // Restore the setting for the old origin. If the user has changed the setting
  // since the origin became the DSE, we reset the setting so the user will be
  // prompted.
  if (setting_to_restore != dse_setting)
    setting_to_restore = CONTENT_SETTING_ASK;
  SetContentSetting(dse_origin, type, setting_to_restore);

  return dse_setting;
}

void SearchPermissionsService::InitializeSettingsIfNeeded() {
  GURL dse_origin = delegate_->GetDSEOrigin().GetURL();

  // `dse_origin` can be invalid in tests or if the DSE is disabled by policy.
  // If that's the case or if `RevertDSEAutomaticPermissions` is enabled, we
  // restore the old settings and erase the pref.
  const bool disabled_by_policy = !dse_origin.is_valid();
  if (pref_service_->HasPrefPath(prefs::kDSEPermissionsSettings)) {
    if (disabled_by_policy)
      pref_service_->SetBoolean(prefs::kDSEWasDisabledByPolicy, true);

    PrefValue pref = GetDSEPref();
    GURL old_dse_origin(pref.dse_origin);

    ContentSetting effective_setting = RestoreOldSettingAndReturnPrevious(
        old_dse_origin, ContentSettingsType::GEOLOCATION,
        pref.geolocation_setting_to_restore, !disabled_by_policy);
    if (!disabled_by_policy) {
      RecordAutoDSEPermissionReverted(ContentSettingsType::GEOLOCATION,
                                      pref.geolocation_setting_to_restore,
                                      effective_setting, dse_origin);
    }

    if (pref.notifications_setting_to_restore != CONTENT_SETTING_DEFAULT) {
      effective_setting = RestoreOldSettingAndReturnPrevious(
          old_dse_origin, ContentSettingsType::NOTIFICATIONS,
          pref.notifications_setting_to_restore, !disabled_by_policy);
      if (!disabled_by_policy) {
        RecordAutoDSEPermissionReverted(ContentSettingsType::NOTIFICATIONS,
                                        pref.notifications_setting_to_restore,
                                        effective_setting, dse_origin);
      }
    }
    pref_service_->ClearPref(prefs::kDSEPermissionsSettings);
  }

  RecordEffectiveDSEOriginPermissions();
}

SearchPermissionsService::PrefValue SearchPermissionsService::GetDSEPref() {
  const base::Value::Dict& dict =
      pref_service_->GetDict(prefs::kDSEPermissionsSettings);

  PrefValue pref;
  const std::string* dse_name = dict.FindString(kDSENameKey);
  const std::string* dse_origin = dict.FindString(kDSEOriginKey);
  std::optional<int> geolocation_setting_to_restore =
      dict.FindInt(kDSEGeolocationSettingKey);
  std::optional<int> notifications_setting_to_restore =
      dict.FindInt(kDSENotificationsSettingKey);

  if (dse_name && dse_origin && geolocation_setting_to_restore &&
      notifications_setting_to_restore) {
    pref.dse_name = base::UTF8ToUTF16(*dse_name);
    pref.dse_origin = *dse_origin;
    pref.geolocation_setting_to_restore =
        IntToContentSetting(*geolocation_setting_to_restore);
    pref.notifications_setting_to_restore =
        IntToContentSetting(*notifications_setting_to_restore);
  }

  return pref;
}

ContentSetting SearchPermissionsService::GetContentSetting(
    const GURL& origin,
    ContentSettingsType type) {
  return host_content_settings_map_->GetUserModifiableContentSetting(
      origin, origin, type);
}

void SearchPermissionsService::SetContentSetting(const GURL& origin,
                                                 ContentSettingsType type,
                                                 ContentSetting setting) {
  // Clear a setting before setting it. This is needed because in general
  // notifications settings can't be changed from ALLOW<->BLOCK on Android O+.
  // We need to change the setting from ALLOW->BLOCK in one case, where the
  // previous DSE had permission blocked but the new DSE we're changing to has
  // permission allowed. Thus this works around that restriction.
  // WARNING: This is a special case and in general notification settings should
  // never be changed between ALLOW<->BLOCK on Android. Do not copy this code.
  // Check with the notifications team if you need to do something like this.
  host_content_settings_map_->SetContentSettingDefaultScope(
      origin, origin, type, CONTENT_SETTING_DEFAULT);

  // If we're restoring an ASK setting, it really implies that we should delete
  // the user-defined setting to fall back to the default.
  if (setting == CONTENT_SETTING_ASK)
    return;  // We deleted the setting above already.

  host_content_settings_map_->SetContentSettingDefaultScope(origin, origin,
                                                            type, setting);
}

void SearchPermissionsService::SetSearchEngineDelegateForTest(
    std::unique_ptr<SearchEngineDelegate> delegate) {
  delegate_ = std::move(delegate);
}

void SearchPermissionsService::RecordAutoDSEPermissionReverted(
    ContentSettingsType permission_type,
    ContentSetting backed_up_setting,
    ContentSetting effective_setting,
    const GURL& origin) {
  ContentSetting end_state_setting = GetContentSetting(origin, permission_type);
  permissions::PermissionUmaUtil::RecordAutoDSEPermissionReverted(
      permission_type, backed_up_setting, effective_setting, end_state_setting);
}

void SearchPermissionsService::RecordEffectiveDSEOriginPermissions() {
  GURL dse_origin = delegate_->GetDSEOrigin().GetURL();
  if (!dse_origin.is_valid())
    return;

  permissions::PermissionUmaUtil::RecordDSEEffectiveSetting(
      ContentSettingsType::NOTIFICATIONS,
      GetContentSetting(dse_origin, ContentSettingsType::NOTIFICATIONS));

  permissions::PermissionUmaUtil::RecordDSEEffectiveSetting(
      ContentSettingsType::GEOLOCATION,
      GetContentSetting(dse_origin, ContentSettingsType::GEOLOCATION));
}

void SearchPermissionsService::SetDSEPrefForTesting(
    ContentSetting geolocation_setting_to_restore,
    ContentSetting notifications_setting_to_restore) {
  base::Value::Dict dict;
  dict.Set(kDSENameKey, delegate_->GetDSEName());
  dict.Set(kDSEOriginKey, delegate_->GetDSEOrigin().GetURL().spec());
  dict.Set(kDSEGeolocationSettingKey, geolocation_setting_to_restore);
  dict.Set(kDSENotificationsSettingKey, notifications_setting_to_restore);
  pref_service_->SetDict(prefs::kDSEPermissionsSettings, std::move(dict));
}
