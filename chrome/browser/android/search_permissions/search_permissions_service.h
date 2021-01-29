// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ANDROID_SEARCH_PERMISSIONS_SEARCH_PERMISSIONS_SERVICE_H_
#define CHROME_BROWSER_ANDROID_SEARCH_PERMISSIONS_SEARCH_PERMISSIONS_SERVICE_H_

#include "base/callback_forward.h"
#include "base/memory/singleton.h"
#include "base/strings/string16.h"
#include "components/content_settings/core/common/content_settings.h"
#include "components/keyed_service/content/browser_context_keyed_service_factory.h"
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
    virtual base::string16 GetDSEName() = 0;

    // Returns the origin of the DSE. If the current DSE is Google this will
    // return the current CCTLD.
    virtual url::Origin GetDSEOrigin() = 0;

    // Set a callback that will be called if the DSE or CCTLD changes for any
    // reason.
    virtual void SetDSEChangedCallback(base::RepeatingClosure callback) = 0;
  };

  // Factory implementation will not create a service in incognito.
  class Factory : public BrowserContextKeyedServiceFactory {
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

  // Returns whether the given permission is being configured for the DSE for
  // the given origin.
  bool IsPermissionControlledByDSE(ContentSettingsType type,
                                   const url::Origin& requesting_origin);

  // Resets the DSE permission for a single ContentSettingsType.
  void ResetDSEPermission(ContentSettingsType type);

  // Reset all supported DSE permissions.
  void ResetDSEPermissions();

  // KeyedService:
  void Shutdown() override;

 private:
  friend class ChromeBrowsingDataRemoverDelegateTest;
  friend class SearchPermissionsServiceTest;
  FRIEND_TEST_ALL_PREFIXES(GeolocationPermissionContextDelegateTests,
                           SearchGeolocationInIncognito);
  struct PrefValue;

  ~SearchPermissionsService() override;

  // When the DSE CCTLD changes (either by changing their DSE or by changing
  // their CCTLD) we carry over the geolocation/notification permissions from
  // the last DSE CCTLD. Before carrying them over, we store the old value
  // of the permissions in a pref so the user's settings can be restored if
  // they change DSE in the future.
  // We resolve conflicts in the following way:
  // * If the DSE CCTLD origin permission is BLOCK, but the old DSE's permission
  //   is ALLOW, change the DSE permission setting to BLOCK.
  // * If the DSE CCTLD origin permission is ALLOW, but the old DSE's permission
  //   is BLOCK, change the DSE permission setting to BLOCK but restore it to
  //   ASK later.
  // * If the user changes the DSE CCTLD origin permission, we restore it back
  //   to ASK when they change DSE.
  // Also, if the DSE changes and geolocation is enabled, we reset the
  // geolocation disclosure so that it will be shown again.
  void OnDSEChanged();

  // Restore the setting for an origin before it became the DSE. Returns the
  // setting that the origin was set to before restoring the old value.
  ContentSetting RestoreOldSettingAndReturnPrevious(
      const GURL& dse_origin,
      ContentSettingsType type,
      ContentSetting setting_to_restore);

  // Helper function for OnDSEChanged which transitions the DSE setting for a
  // specific permission. It returns the content setting to be restored later
  // for |new_dse_origin|.
  ContentSetting UpdatePermissionAndReturnPrevious(ContentSettingsType type,
                                                   const GURL& old_dse_origin,
                                                   const GURL& new_dse_origin,
                                                   ContentSetting old_setting,
                                                   bool dse_name_changed);

  // Initialize the DSE permission settings if they haven't already been
  // initialized. Also, if they haven't been initialized, reset whether the DSE
  // geolocation disclosure has been shown to ensure user who may have seen it
  // on earlier versions (due to Finch experiments) see it again.
  void InitializeSettingsIfNeeded();

  PrefValue GetDSEPref();
  void SetDSEPref(const PrefValue& pref);

  // Retrieve the content setting for the given permission/origin.
  ContentSetting GetContentSetting(const GURL& origin,
                                   ContentSettingsType type);
  // Set the content setting for the given permission/origin.
  void SetContentSetting(const GURL& origin,
                         ContentSettingsType type,
                         ContentSetting setting);

  void SetSearchEngineDelegateForTest(
      std::unique_ptr<SearchEngineDelegate> delegate);

  Profile* profile_;
  PrefService* pref_service_;
  HostContentSettingsMap* host_content_settings_map_;
  std::unique_ptr<SearchEngineDelegate> delegate_;
};

#endif  // CHROME_BROWSER_ANDROID_SEARCH_PERMISSIONS_SEARCH_PERMISSIONS_SERVICE_H_
