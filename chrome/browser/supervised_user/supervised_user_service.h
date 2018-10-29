// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SUPERVISED_USER_SUPERVISED_USER_SERVICE_H_
#define CHROME_BROWSER_SUPERVISED_USER_SUPERVISED_USER_SERVICE_H_

#include <stddef.h>

#include <map>
#include <memory>
#include <string>
#include <vector>

#include "base/callback.h"
#include "base/gtest_prod_util.h"
#include "base/macros.h"
#include "base/observer_list.h"
#include "base/scoped_observer.h"
#include "base/strings/string16.h"
#include "build/build_config.h"
#include "chrome/browser/net/file_downloader.h"
#include "chrome/browser/supervised_user/experimental/safe_search_url_reporter.h"
#include "chrome/browser/supervised_user/experimental/supervised_user_blacklist.h"
#include "chrome/browser/supervised_user/supervised_user_url_filter.h"
#include "chrome/browser/supervised_user/supervised_users.h"
#include "chrome/browser/ui/browser_list_observer.h"
#include "components/keyed_service/core/keyed_service.h"
#include "components/prefs/pref_change_registrar.h"
#include "components/sync/driver/sync_type_preference_provider.h"
#include "extensions/buildflags/buildflags.h"

#if BUILDFLAG(ENABLE_EXTENSIONS)
#include "extensions/browser/extension_registry_observer.h"
#include "extensions/browser/management_policy.h"
#endif

class Browser;
class PermissionRequestCreator;
class Profile;
class SupervisedUserServiceObserver;
class SupervisedUserSettingsService;
class SupervisedUserSiteList;
class SupervisedUserURLFilter;
class SupervisedUserWhitelistService;

namespace base {
class FilePath;
class Version;
}

namespace extensions {
class ExtensionRegistry;
}

namespace user_prefs {
class PrefRegistrySyncable;
}

// This class handles all the information related to a given supervised profile
// (e.g. the installed content packs, the default URL filtering behavior, or
// manual whitelist/blacklist overrides).
class SupervisedUserService : public KeyedService,
#if BUILDFLAG(ENABLE_EXTENSIONS)
                              public extensions::ExtensionRegistryObserver,
                              public extensions::ManagementPolicy::Provider,
#endif
                              public syncer::SyncTypePreferenceProvider,
#if !defined(OS_ANDROID)
                              public BrowserListObserver,
#endif
                              public SupervisedUserURLFilter::Observer {
 public:
  using SuccessCallback = base::OnceCallback<void(bool)>;

  class Delegate {
   public:
    virtual ~Delegate() {}
    // Returns true to indicate that the delegate handled the (de)activation, or
    // false to indicate that the SupervisedUserService itself should handle it.
    virtual bool SetActive(bool active) = 0;
  };

  ~SupervisedUserService() override;

  static void RegisterProfilePrefs(user_prefs::PrefRegistrySyncable* registry);

  // Initializes this object.
  void Init();

  void SetDelegate(Delegate* delegate);

  // Returns the URL filter for filtering navigations and classifying sites in
  // the history view. Both this method and the returned filter may only be used
  // on the UI thread.
  SupervisedUserURLFilter* GetURLFilter();

  // Returns the whitelist service.
  SupervisedUserWhitelistService* GetWhitelistService();

  const std::vector<scoped_refptr<SupervisedUserSiteList>>& whitelists() const {
    return whitelists_;
  }

  // Whether the user can request to get access to blocked URLs or to new
  // extensions.
  bool AccessRequestsEnabled();

  // Adds an access request for the given URL.
  void AddURLAccessRequest(const GURL& url, SuccessCallback callback);

  // Reports |url| to the SafeSearch API, because the user thinks this is an
  // inappropriate URL.
  void ReportURL(const GURL& url, SuccessCallback callback);

  // Adds an install request for the given WebStore item (App/Extension).
  void AddExtensionInstallRequest(const std::string& extension_id,
                                  const base::Version& version,
                                  SuccessCallback callback);

  // Same as above, but without a callback, just logging errors on failure.
  void AddExtensionInstallRequest(const std::string& extension_id,
                                  const base::Version& version);

  // Adds an update request for the given WebStore item (App/Extension).
  void AddExtensionUpdateRequest(const std::string& extension_id,
                                 const base::Version& version,
                                 SuccessCallback callback);

  // Same as above, but without a callback, just logging errors on failure.
  void AddExtensionUpdateRequest(const std::string& extension_id,
                                 const base::Version& version);

  // Get the string used to identify an extension install or update request.
  // Public for testing.
  static std::string GetExtensionRequestId(const std::string& extension_id,
                                           const base::Version& version);

  // Returns the email address of the custodian.
  std::string GetCustodianEmailAddress() const;

  // Returns the name of the custodian, or the email address if the name is
  // empty.
  std::string GetCustodianName() const;

  // Returns the email address of the second custodian, or the empty string
  // if there is no second custodian.
  std::string GetSecondCustodianEmailAddress() const;

  // Returns the name of the second custodian, or the email address if the name
  // is empty, or the empty string is there is no second custodian.
  std::string GetSecondCustodianName() const;

  // Returns a message saying that extensions can only be modified by the
  // custodian.
  base::string16 GetExtensionsLockedMessage() const;

#if !defined(OS_ANDROID)
  // Initializes this profile for syncing, using the provided |refresh_token| to
  // mint access tokens for Sync.
  void InitSync(const std::string& refresh_token);
#endif

  void AddObserver(SupervisedUserServiceObserver* observer);
  void RemoveObserver(SupervisedUserServiceObserver* observer);

  void AddPermissionRequestCreator(
      std::unique_ptr<PermissionRequestCreator> creator);

  void SetSafeSearchURLReporter(
      std::unique_ptr<SafeSearchURLReporter> reporter);

  // Returns true if the syncer::SESSIONS type should be included in Sync.
  // Public for testing.
  bool IncludesSyncSessionsType() const;

  // ProfileKeyedService override:
  void Shutdown() override;

  // SyncTypePreferenceProvider implementation:
  syncer::ModelTypeSet GetPreferredDataTypes() const override;

#if !defined(OS_ANDROID)
  // BrowserListObserver implementation:
  void OnBrowserSetLastActive(Browser* browser) override;
#endif  // !defined(OS_ANDROID)

  // SupervisedUserURLFilter::Observer implementation:
  void OnSiteListUpdated() override;

 private:
  friend class SupervisedUserServiceExtensionTestBase;
  friend class SupervisedUserServiceFactory;
  FRIEND_TEST_ALL_PREFIXES(
      SupervisedUserServiceExtensionTest,
      ExtensionManagementPolicyProviderWithoutSUInitiatedInstalls);
  FRIEND_TEST_ALL_PREFIXES(
      SupervisedUserServiceExtensionTest,
      ExtensionManagementPolicyProviderWithSUInitiatedInstalls);

  using CreatePermissionRequestCallback =
      base::RepeatingCallback<void(PermissionRequestCreator*, SuccessCallback)>;

  // Use |SupervisedUserServiceFactory::GetForProfile(..)| to get
  // an instance of this service.
  explicit SupervisedUserService(Profile* profile);

  void SetActive(bool active);

  bool ProfileIsSupervised() const;

  void OnCustodianInfoChanged();

#if BUILDFLAG(ENABLE_EXTENSIONS)
  // extensions::ManagementPolicy::Provider implementation:
  std::string GetDebugPolicyProviderName() const override;
  bool UserMayLoad(const extensions::Extension* extension,
                   base::string16* error) const override;
  bool UserMayModifySettings(const extensions::Extension* extension,
                             base::string16* error) const override;
  bool MustRemainInstalled(const extensions::Extension* extension,
                           base::string16* error) const override;
  bool MustRemainDisabled(const extensions::Extension* extension,
                          extensions::disable_reason::DisableReason* reason,
                          base::string16* error) const override;

  // extensions::ExtensionRegistryObserver overrides:
  void OnExtensionInstalled(content::BrowserContext* browser_context,
                            const extensions::Extension* extension,
                            bool is_update) override;

  // An extension can be in one of the following states:
  //
  // FORCED: if it is installed by the custodian.
  // REQUIRE_APPROVAL: if it is installed by the supervised user and
  //    hasn't been approved by the custodian yet.
  // ALLOWED: Components, Themes, Default extensions ..etc
  //    are generally allowed.  Extensions that have been approved by the
  //    custodian are also allowed.
  // BLOCKED: if it is not ALLOWED or FORCED
  //    and supervised users initiated installs are disabled.
  enum class ExtensionState { FORCED, BLOCKED, ALLOWED, REQUIRE_APPROVAL };

  // Returns the state of an extension whether being FORCED, BLOCKED, ALLOWED or
  // REQUIRE_APPROVAL from the Supervised User service's point of view.
  ExtensionState GetExtensionState(
      const extensions::Extension& extension) const;

  // Extensions helper to SetActive().
  void SetExtensionsActive();

  // Enables/Disables extensions upon change in approved version of the
  // extension_id.
  void ChangeExtensionStateIfNecessary(const std::string& extension_id);

  // Updates the map of approved extensions when the corresponding preference
  // is changed.
  void UpdateApprovedExtensions();
#endif

  SupervisedUserSettingsService* GetSettingsService();

  size_t FindEnabledPermissionRequestCreator(size_t start);
  void AddPermissionRequestInternal(
      const CreatePermissionRequestCallback& create_request,
      SuccessCallback callback,
      size_t index);
  void OnPermissionRequestIssued(
      const CreatePermissionRequestCallback& create_request,
      SuccessCallback callback,
      size_t index,
      bool success);

  void OnSupervisedUserIdChanged();

  void OnDefaultFilteringBehaviorChanged();

  void OnSafeSitesSettingChanged();

  void OnSiteListsChanged(
      const std::vector<scoped_refptr<SupervisedUserSiteList>>& site_lists);

  // Asynchronously loads a blacklist from a binary file at |path| and applies
  // it to the URL filters. If no file exists at |path| yet, downloads a file
  // from |url| and stores it at |path| first.
  void LoadBlacklist(const base::FilePath& path, const GURL& url);

  void OnBlacklistFileChecked(const base::FilePath& path,
                              const GURL& url,
                              bool file_exists);

  // Asynchronously loads a blacklist from a binary file at |path| and applies
  // it to the URL filters.
  void LoadBlacklistFromFile(const base::FilePath& path);

  void OnBlacklistDownloadDone(const base::FilePath& path,
                               FileDownloader::Result result);

  void OnBlacklistLoaded();

  void UpdateBlacklist();

  // Updates the manual overrides for hosts in the URL filters when the
  // corresponding preference is changed.
  void UpdateManualHosts();

  // Updates the manual overrides for URLs in the URL filters when the
  // corresponding preference is changed.
  void UpdateManualURLs();

  // Subscribes to the SupervisedUserPrefStore, refreshes
  // |includes_sync_sessions_type_| and triggers reconfiguring the
  // ProfileSyncService.
  void OnForceSessionSyncChanged();

  // The option a custodian sets to either record or prevent recording the
  // supervised user's history. Set by |FetchNewSessionSyncState()| and
  // defaults to true.
  bool includes_sync_sessions_type_;

  // Owns us via the KeyedService mechanism.
  Profile* profile_;

  bool active_;

  Delegate* delegate_;

  PrefChangeRegistrar pref_change_registrar_;

  bool is_profile_active_;

  // True only when |Init()| method has been called.
  bool did_init_;

  // True only when |Shutdown()| method has been called.
  bool did_shutdown_;

  SupervisedUserURLFilter url_filter_;

  // Stores a map from extension_id -> approved version by the custodian.
  // It is only relevant for SU-initiated installs.
  std::map<std::string, base::Version> approved_extensions_map_;

  enum class BlacklistLoadState {
    NOT_LOADED,
    LOAD_STARTED,
    LOADED
  } blacklist_state_;

  SupervisedUserBlacklist blacklist_;
  std::unique_ptr<FileDownloader> blacklist_downloader_;

  std::unique_ptr<SupervisedUserWhitelistService> whitelist_service_;

  std::vector<scoped_refptr<SupervisedUserSiteList>> whitelists_;

  // Used to create permission requests.
  std::vector<std::unique_ptr<PermissionRequestCreator>> permissions_creators_;

  // Used to report inappropriate URLs to SafeSarch API.
  std::unique_ptr<SafeSearchURLReporter> url_reporter_;

#if BUILDFLAG(ENABLE_EXTENSIONS)
  ScopedObserver<extensions::ExtensionRegistry,
                 extensions::ExtensionRegistryObserver>
      registry_observer_;
#endif

  base::ObserverList<SupervisedUserServiceObserver>::Unchecked observer_list_;

  base::WeakPtrFactory<SupervisedUserService> weak_ptr_factory_;

  DISALLOW_COPY_AND_ASSIGN(SupervisedUserService);
};

#endif  // CHROME_BROWSER_SUPERVISED_USER_SUPERVISED_USER_SERVICE_H_
