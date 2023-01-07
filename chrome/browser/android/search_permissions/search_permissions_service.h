// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ANDROID_SEARCH_PERMISSIONS_SEARCH_PERMISSIONS_SERVICE_H_
#define CHROME_BROWSER_ANDROID_SEARCH_PERMISSIONS_SEARCH_PERMISSIONS_SERVICE_H_

#include <string>

#include "base/functional/callback_forward.h"
#include "base/gtest_prod_util.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/singleton.h"
#include "chrome/browser/profiles/profile_keyed_service_factory.h"
#include "components/content_settings/core/common/content_settings.h"
#include "components/keyed_service/core/keyed_service.h"
#include "url/origin.h"

namespace content {
class BrowserContext;
}

namespace user_prefs {
class PrefRegistrySyncable;
}

class HostContentSettingsMap;
class PrefService;
class Profile;

// NOTE(crbug/1230193): The DSE auto-granted permissions have been disabled and
// all of the previously granted permissions are reverted in the initialization
// step.
// Helper class to manage DSE permissions. It keeps the setting valid by
// watching change to the CCTLD and DSE.
// Glossary:
//     DSE: Default Search Engine
//     CCTLD: Country Code Top Level Domain (e.g. google.com.au)
class SearchPermissionsService : public KeyedService {
 public:
  // Delegate for search engine related functionality. Can be overridden for
  // testing.
  class SearchEngineDelegate {
   public:
    virtual ~SearchEngineDelegate() {}

    // Returns the name of the current DSE.
    virtual std::u16string GetDSEName() = 0;

    // Returns the origin of the DSE. If the current DSE is Google this will
    // return the current CCTLD.
    virtual url::Origin GetDSEOrigin() = 0;
  };

  // Factory implementation will not create a service in incognito.
  class Factory : public ProfileKeyedServiceFactory {
   public:
    static SearchPermissionsService* GetForBrowserContext(
        content::BrowserContext* context);

    static Factory* GetInstance();

   private:
    friend struct base::DefaultSingletonTraits<Factory>;

    Factory();
    ~Factory() override;

    // BrowserContextKeyedServiceFactory
    bool ServiceIsCreatedWithBrowserContext() const override;
    KeyedService* BuildServiceInstanceFor(
        content::BrowserContext* profile) const override;
    void RegisterProfilePrefs(
        user_prefs::PrefRegistrySyncable* registry) override;
  };

  explicit SearchPermissionsService(Profile* profile);

  // Returns whether the given origin matches the DSE origin.
  bool IsDseOrigin(const url::Origin& origin);

  // KeyedService:
  void Shutdown() override;

 private:
  friend class ChromeBrowsingDataRemoverDelegateTest;
  friend class SearchPermissionsServiceTest;
  FRIEND_TEST_ALL_PREFIXES(GeolocationPermissionContextDelegateTests,
                           SearchGeolocationInIncognito);
  struct PrefValue;

  ~SearchPermissionsService() override;

  // Restore the setting for an origin before it became the DSE. Returns the
  // setting that the origin was set to before restoring the old value.
  ContentSetting RestoreOldSettingAndReturnPrevious(
      const GURL& dse_origin,
      ContentSettingsType type,
      ContentSetting setting_to_restore,
      bool preserve_block_setting);

  // Initialize the DSE permission settings if they haven't already been
  // initialized. Also, if they haven't been initialized, reset whether the DSE
  // geolocation disclosure has been shown to ensure user who may have seen it
  // on earlier versions (due to Finch experiments) see it again.
  void InitializeSettingsIfNeeded();

  PrefValue GetDSEPref();

  // Retrieve the content setting for the given permission/origin.
  ContentSetting GetContentSetting(const GURL& origin,
                                   ContentSettingsType type);
  // Set the content setting for the given permission/origin.
  void SetContentSetting(const GURL& origin,
                         ContentSettingsType type,
                         ContentSetting setting);

  // Record how the content setting transitions when DSE permissions autogrant
  // is disabled via feature.
  void RecordAutoDSEPermissionReverted(ContentSettingsType permission_type,
                                       ContentSetting backed_up_setting,
                                       ContentSetting effective_setting,
                                       const GURL& origin);

  // Record the content settings for notifications and geolocation on the DSE
  // origin. Called at initialization or when the DSE origin changes.
  void RecordEffectiveDSEOriginPermissions();

  void SetSearchEngineDelegateForTest(
      std::unique_ptr<SearchEngineDelegate> delegate);

  // Simulate an existing `prefs::kDSEPermissionsSettings` entry with the
  // provided settings. Used to test automatically reverting the pre-granted DSE
  // permissions.
  void SetDSEPrefForTesting(ContentSetting geolocation_setting_to_restore,
                            ContentSetting notifications_setting_to_restore);

  raw_ptr<Profile> profile_;
  raw_ptr<PrefService> pref_service_;
  raw_ptr<HostContentSettingsMap> host_content_settings_map_;
  std::unique_ptr<SearchEngineDelegate> delegate_;
};

#endif  // CHROME_BROWSER_ANDROID_SEARCH_PERMISSIONS_SEARCH_PERMISSIONS_SERVICE_H_
