// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/android/search_permissions/search_permissions_service.h"

#include "base/bind.h"
#include "base/callback.h"
#include "base/values.h"
#include "chrome/browser/android/search_permissions/search_geolocation_disclosure_tab_helper.h"
#include "chrome/browser/content_settings/host_content_settings_map_factory.h"
#include "chrome/browser/permissions/permission_decision_auto_blocker.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/search_engines/template_url_service_factory.h"
#include "chrome/browser/search_engines/ui_thread_search_terms_data.h"
#include "chrome/common/pref_names.h"
#include "components/content_settings/core/browser/content_settings_utils.h"
#include "components/content_settings/core/browser/host_content_settings_map.h"
#include "components/content_settings/core/common/content_settings_types.h"
#include "components/keyed_service/content/browser_context_dependency_manager.h"
#include "components/pref_registry/pref_registry_syncable.h"
#include "components/prefs/pref_service.h"
#include "components/search_engines/template_url.h"
#include "components/search_engines/template_url_service.h"
#include "components/search_engines/template_url_service_observer.h"
#include "url/gurl.h"
#include "url/url_constants.h"

namespace {

const char kDSENameKey[] = "dse_name";
const char kDSEOriginKey[] = "dse_origin";
const char kDSEGeolocationSettingKey[] = "geolocation_setting_to_restore";
const char kDSENotificationsSettingKey[] = "notifications_setting_to_restore";
const char kDSESettingKeyDeprecated[] = "dse_setting";

// Default implementation of SearchEngineDelegate that is used for production
// code.
class SearchEngineDelegateImpl
    : public SearchPermissionsService::SearchEngineDelegate,
      public TemplateURLServiceObserver {
 public:
  explicit SearchEngineDelegateImpl(Profile* profile)
      : profile_(profile),
        template_url_service_(
            TemplateURLServiceFactory::GetForProfile(profile_)) {
    if (template_url_service_)
      template_url_service_->AddObserver(this);
  }

  ~SearchEngineDelegateImpl() override {
    if (template_url_service_)
      template_url_service_->RemoveObserver(this);
  }

  base::string16 GetDSEName() override {
    if (template_url_service_) {
      const TemplateURL* template_url =
          template_url_service_->GetDefaultSearchProvider();
      if (template_url)
        return template_url->short_name();
    }

    return base::string16();
  }

  url::Origin GetDSEOrigin() override {
    if (template_url_service_) {
      const TemplateURL* template_url =
          template_url_service_->GetDefaultSearchProvider();
      if (template_url) {
        GURL search_url = template_url->GenerateSearchURL(
            template_url_service_->search_terms_data());
        return url::Origin::Create(search_url);
      }
    }

    return url::Origin();
  }

  void SetDSEChangedCallback(const base::Closure& callback) override {
    dse_changed_callback_ = callback;
  }

  // TemplateURLServiceObserver
  void OnTemplateURLServiceChanged() override { dse_changed_callback_.Run(); }

 private:
  Profile* profile_;

  // Will be null in unittests.
  TemplateURLService* template_url_service_;

  base::Closure dse_changed_callback_;
};

}  // namespace

struct SearchPermissionsService::PrefValue {
  base::string16 dse_name;
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
    : BrowserContextKeyedServiceFactory(
          "SearchPermissionsService",
          BrowserContextDependencyManager::GetInstance()) {
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

  delegate_.reset(new SearchEngineDelegateImpl(profile_));
  delegate_->SetDSEChangedCallback(base::Bind(
      &SearchPermissionsService::OnDSEChanged, base::Unretained(this)));

  // Under normal circumstances we wouldn't need to call OnDSEChanged here, just
  // InitializeSettingsIfNeeded but it's possible that somehow the underlying
  // pref became out of sync with what the current DSE is (e.g. if Chrome
  // crashed between changing DSE and updating the pref). These cases could
  // result in granting permission unintentionally so it's important to call
  // OnDSEChanged to keep state consistent. If the current DSE and stored DSE
  // are the same, OnDSEChanged will not do anything. OnDSEChanged will also
  // initialize the pref correctly if needed.
  OnDSEChanged();
}

bool SearchPermissionsService::IsPermissionControlledByDSE(
    ContentSettingsType type,
    const url::Origin& requesting_origin) {
  if (type != ContentSettingsType::GEOLOCATION &&
      type != ContentSettingsType::NOTIFICATIONS) {
    return false;
  }

  if (requesting_origin.scheme() != url::kHttpsScheme)
    return false;

  if (!requesting_origin.IsSameOriginWith(delegate_->GetDSEOrigin()))
    return false;

  return true;
}

void SearchPermissionsService::ResetDSEPermission(ContentSettingsType type) {
  url::Origin dse_origin = delegate_->GetDSEOrigin();
  GURL dse_url = dse_origin.GetURL();
  DCHECK(dse_url.is_empty() || IsPermissionControlledByDSE(type, dse_origin));

  if (!dse_url.is_empty())
    SetContentSetting(dse_url, type, CONTENT_SETTING_ALLOW);
}

void SearchPermissionsService::ResetDSEPermissions() {
  ResetDSEPermission(ContentSettingsType::GEOLOCATION);
  ResetDSEPermission(ContentSettingsType::NOTIFICATIONS);
}

void SearchPermissionsService::Shutdown() {
  delegate_.reset();
}

SearchPermissionsService::~SearchPermissionsService() {}

void SearchPermissionsService::OnDSEChanged() {
  InitializeSettingsIfNeeded();

  // If we didn't initialize properly because there is no DSE don't do anything.
  if (!pref_service_->HasPrefPath(prefs::kDSEPermissionsSettings))
    return;

  PrefValue pref = GetDSEPref();

  base::string16 new_dse_name = delegate_->GetDSEName();
  base::string16 old_dse_name = pref.dse_name;

  GURL old_dse_origin(pref.dse_origin);
  GURL new_dse_origin = delegate_->GetDSEOrigin().GetURL();

  // Don't do anything if the DSE origin hasn't changed.
  if (old_dse_origin == new_dse_origin)
    return;

  ContentSetting geolocation_setting_to_restore =
      UpdatePermissionAndReturnPrevious(
          ContentSettingsType::GEOLOCATION, old_dse_origin, new_dse_origin,
          pref.geolocation_setting_to_restore, old_dse_name != new_dse_name);
  ContentSetting notifications_setting_to_restore =
      pref.notifications_setting_to_restore;
  notifications_setting_to_restore = UpdatePermissionAndReturnPrevious(
      ContentSettingsType::NOTIFICATIONS, old_dse_origin, new_dse_origin,
      pref.notifications_setting_to_restore, old_dse_name != new_dse_name);

  // Write the pref for restoring the old values when the DSE changes.
  pref.dse_name = new_dse_name;
  pref.dse_origin = new_dse_origin.spec();
  pref.geolocation_setting_to_restore = geolocation_setting_to_restore;
  pref.notifications_setting_to_restore = notifications_setting_to_restore;
  SetDSEPref(pref);
}

ContentSetting SearchPermissionsService::RestoreOldSettingAndReturnPrevious(
    const GURL& dse_origin,
    ContentSettingsType type,
    ContentSetting setting_to_restore) {
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

  // Restore the setting for the old origin. If the user has changed the setting
  // since the origin became the DSE, we reset the setting so the user will be
  // prompted.
  if (setting_to_restore != dse_setting)
    setting_to_restore = CONTENT_SETTING_ASK;
  SetContentSetting(dse_origin, type, setting_to_restore);

  return dse_setting;
}

ContentSetting SearchPermissionsService::UpdatePermissionAndReturnPrevious(
    ContentSettingsType type,
    const GURL& old_dse_origin,
    const GURL& new_dse_origin,
    ContentSetting old_dse_setting_to_restore,
    bool dse_name_changed) {
  // Remove any embargo on the URL.
  PermissionDecisionAutoBlocker::GetForProfile(profile_)->RemoveEmbargoByUrl(
      new_dse_origin, type);

  ContentSetting dse_setting = RestoreOldSettingAndReturnPrevious(
      old_dse_origin, type, old_dse_setting_to_restore);

  ContentSetting new_dse_setting_to_restore =
      GetContentSetting(new_dse_origin, type);
  // If the DSE we're changing to is already blocked, we just leave it in the
  // blocked state.
  if (new_dse_setting_to_restore != CONTENT_SETTING_BLOCK) {
    // If the DSE we're changing to is allowed, but the DSE setting is blocked,
    // we change the setting to block, but when we restore the setting, we go
    // back to ask.
    if (new_dse_setting_to_restore == CONTENT_SETTING_ALLOW &&
        dse_setting == CONTENT_SETTING_BLOCK) {
      SetContentSetting(new_dse_origin, type, CONTENT_SETTING_BLOCK);
      new_dse_setting_to_restore = CONTENT_SETTING_ASK;
    } else {
      SetContentSetting(new_dse_origin, type, dse_setting);
    }
  }

  // Reset the disclosure if needed.
  if (type == ContentSettingsType::GEOLOCATION && dse_name_changed &&
      dse_setting == CONTENT_SETTING_ALLOW) {
    SearchGeolocationDisclosureTabHelper::ResetDisclosure(profile_);
  }

  return new_dse_setting_to_restore;
}

void SearchPermissionsService::InitializeSettingsIfNeeded() {
  GURL dse_origin = delegate_->GetDSEOrigin().GetURL();

  // This can happen in tests or if the DSE is disabled by policy. If that's
  // the case, we restore the old settings and erase the pref.
  if (!dse_origin.is_valid()) {
    if (pref_service_->HasPrefPath(prefs::kDSEPermissionsSettings)) {
      pref_service_->SetBoolean(prefs::kDSEWasDisabledByPolicy, true);

      PrefValue pref = GetDSEPref();
      GURL old_dse_origin(pref.dse_origin);
      RestoreOldSettingAndReturnPrevious(old_dse_origin,
                                         ContentSettingsType::GEOLOCATION,
                                         pref.geolocation_setting_to_restore);
      if (pref.notifications_setting_to_restore != CONTENT_SETTING_DEFAULT) {
        RestoreOldSettingAndReturnPrevious(
            old_dse_origin, ContentSettingsType::NOTIFICATIONS,
            pref.notifications_setting_to_restore);
      }
      pref_service_->ClearPref(prefs::kDSEPermissionsSettings);
    }

    // Defer initialization until a search engine becomes the DSE.
    return;
  }

  // If we get to here, the DSE is not disabled by enterprise policy. If it was
  // previously enterprise controlled, we initialize the setting to BLOCK since
  // we don't know what the user's setting was previously.
  bool was_enterprise_controlled =
      pref_service_->GetBoolean(prefs::kDSEWasDisabledByPolicy);
  pref_service_->ClearPref(prefs::kDSEWasDisabledByPolicy);

  // Initialize the pref for geolocation if it hasn't been initialized yet.
  if (!pref_service_->HasPrefPath(prefs::kDSEPermissionsSettings)) {
    ContentSetting geolocation_setting_to_restore =
        GetContentSetting(dse_origin, ContentSettingsType::GEOLOCATION);
    ContentSetting dse_geolocation_setting = geolocation_setting_to_restore;

    bool reset_disclosure = true;
    // Migrate the old geolocation pref if it exists.
    if (pref_service_->HasPrefPath(prefs::kDSEGeolocationSettingDeprecated)) {
      // If the DSE geolocation setting is already initialized, it means we've
      // already setup the disclosure to be shown so we don't need to do it
      // again.
      reset_disclosure = false;

      const base::DictionaryValue* dict =
          pref_service_->GetDictionary(prefs::kDSEGeolocationSettingDeprecated);

      // If the user's content setting is being overridden by the DSE setting,
      // we migrate the DSE setting to be stored in the user's content setting.
      bool dse_setting = false;
      dict->GetBoolean(kDSESettingKeyDeprecated, &dse_setting);
      if (dse_geolocation_setting == CONTENT_SETTING_ASK) {
        dse_geolocation_setting =
            dse_setting ? CONTENT_SETTING_ALLOW : CONTENT_SETTING_BLOCK;
      }

      // Delete setting.
      pref_service_->ClearPref(prefs::kDSEGeolocationSettingDeprecated);
    } else if (dse_geolocation_setting == CONTENT_SETTING_ASK) {
      // If the user hasn't explicitly allowed or blocked geolocation for the
      // DSE, initialize it to allowed.
      dse_geolocation_setting = was_enterprise_controlled
                                    ? CONTENT_SETTING_BLOCK
                                    : CONTENT_SETTING_ALLOW;
    }

    // Update the content setting with the auto-grants for the DSE.
    SetContentSetting(dse_origin, ContentSettingsType::GEOLOCATION,
                      dse_geolocation_setting);

    if (reset_disclosure)
      SearchGeolocationDisclosureTabHelper::ResetDisclosure(profile_);

    PrefValue pref;
    pref.dse_name = delegate_->GetDSEName();
    pref.dse_origin = dse_origin.spec();
    pref.geolocation_setting_to_restore = geolocation_setting_to_restore;
    pref.notifications_setting_to_restore = CONTENT_SETTING_DEFAULT;
    SetDSEPref(pref);
  }

  // Initialize the notifications part of the pref if needed.
  PrefValue pref = GetDSEPref();
  if (pref.notifications_setting_to_restore == CONTENT_SETTING_DEFAULT) {
    ContentSetting notifications_setting_to_restore =
        GetContentSetting(dse_origin, ContentSettingsType::NOTIFICATIONS);
    ContentSetting dse_notifications_setting = notifications_setting_to_restore;
    // If the user hasn't explicitly allowed or blocked notifications for the
    // DSE, initialize it to allowed.
    if (dse_notifications_setting == CONTENT_SETTING_ASK) {
      dse_notifications_setting = was_enterprise_controlled
                                      ? CONTENT_SETTING_BLOCK
                                      : CONTENT_SETTING_ALLOW;
    }

    // Update the content setting with the auto-grants for the DSE.
    SetContentSetting(dse_origin, ContentSettingsType::NOTIFICATIONS,
                      dse_notifications_setting);

    // Write the pref for restoring the old values when the DSE changes.
    pref.notifications_setting_to_restore = notifications_setting_to_restore;
    SetDSEPref(pref);
  }
}

SearchPermissionsService::PrefValue SearchPermissionsService::GetDSEPref() {
  const base::DictionaryValue* dict =
      pref_service_->GetDictionary(prefs::kDSEPermissionsSettings);

  PrefValue pref;
  base::string16 dse_name;
  std::string dse_origin;
  int geolocation_setting_to_restore;
  int notifications_setting_to_restore;

  if (dict->GetString(kDSENameKey, &dse_name) &&
      dict->GetString(kDSEOriginKey, &dse_origin) &&
      dict->GetInteger(kDSEGeolocationSettingKey,
                       &geolocation_setting_to_restore) &&
      dict->GetInteger(kDSENotificationsSettingKey,
                       &notifications_setting_to_restore)) {
    pref.dse_name = dse_name;
    pref.dse_origin = dse_origin;
    pref.geolocation_setting_to_restore =
        IntToContentSetting(geolocation_setting_to_restore);
    pref.notifications_setting_to_restore =
        IntToContentSetting(notifications_setting_to_restore);
  }

  return pref;
}

void SearchPermissionsService::SetDSEPref(
    const SearchPermissionsService::PrefValue& pref) {
  base::DictionaryValue dict;
  dict.SetString(kDSENameKey, pref.dse_name);
  dict.SetString(kDSEOriginKey, pref.dse_origin);
  dict.SetInteger(kDSEGeolocationSettingKey,
                  pref.geolocation_setting_to_restore);
  dict.SetInteger(kDSENotificationsSettingKey,
                  pref.notifications_setting_to_restore);
  pref_service_->Set(prefs::kDSEPermissionsSettings, dict);
}

ContentSetting SearchPermissionsService::GetContentSetting(
    const GURL& origin,
    ContentSettingsType type) {
  return host_content_settings_map_->GetUserModifiableContentSetting(
      origin, origin, type, std::string());
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
      origin, origin, type, std::string(), CONTENT_SETTING_DEFAULT);

  // If we're restoring an ASK setting, it really implies that we should delete
  // the user-defined setting to fall back to the default.
  if (setting == CONTENT_SETTING_ASK)
    return;  // We deleted the setting above already.

  host_content_settings_map_->SetContentSettingDefaultScope(
      origin, origin, type, std::string(), setting);
}

void SearchPermissionsService::SetSearchEngineDelegateForTest(
    std::unique_ptr<SearchEngineDelegate> delegate) {
  delegate_ = std::move(delegate);
  delegate_->SetDSEChangedCallback(base::Bind(
      &SearchPermissionsService::OnDSEChanged, base::Unretained(this)));
}
