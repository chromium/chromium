// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SUPERVISED_USER_SUPERVISED_USER_SERVICE_H_
#define CHROME_BROWSER_SUPERVISED_USER_SUPERVISED_USER_SERVICE_H_

#include <stddef.h>

#include <memory>
#include <set>
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
#include "chrome/browser/supervised_user/supervised_user_denylist.h"
#include "chrome/browser/supervised_user/supervised_user_url_filter.h"
#include "chrome/browser/supervised_user/supervised_users.h"
#include "components/keyed_service/core/keyed_service.h"
#include "components/prefs/pref_change_registrar.h"
#include "components/sync/driver/sync_type_preference_provider.h"
#include "extensions/buildflags/buildflags.h"

#if BUILDFLAG(ENABLE_EXTENSIONS)
#include "base/strings/string16.h"
#include "chrome/browser/ui/supervised_user/parent_permission_dialog.h"
#include "extensions/browser/extension_registry.h"
#include "extensions/browser/extension_registry_observer.h"
#include "extensions/browser/management_policy.h"
#endif

#if !defined(OS_ANDROID)
#include "chrome/browser/ui/browser_list_observer.h"
#endif  // !defined(OS_ANDROID)

class PermissionRequestCreator;
class PrefService;
class Profile;
class SupervisedUserServiceObserver;
class SupervisedUserSettingsService;
class SupervisedUserSiteList;
class SupervisedUserURLFilter;
class SupervisedUserAllowlistService;

namespace base {
class FilePath;
class Version;
}

#if BUILDFLAG(ENABLE_EXTENSIONS)
namespace extensions {
class Extension;
}
#endif

namespace user_prefs {
class PrefRegistrySyncable;
}

#if !defined(OS_ANDROID)
class Browser;
#endif  // !defined(OS_ANDROID)

// This class handles all the information related to a given supervised profile
// (e.g. the installed content packs, the default URL filtering behavior, or
// manual allowlist/denylist overrides).
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

#if BUILDFLAG(ENABLE_EXTENSIONS)
  // These enum values represent operations to manage the
  // kSupervisedUserApprovedExtensions user pref, which stores parent approved
  // extension ids.
  enum class ApprovedExtensionChange {
    // Adds a new approved extension to the pref.
    kAdd,
    // Removes extension approval.
    kRemove
  };
#endif  // BUILDFLAG(ENABLE_EXTENSIONS)

  ~SupervisedUserService() override;

  static void RegisterProfilePrefs(user_prefs::PrefRegistrySyncable* registry);

  // Initializes this object.
  void Init();

  void SetDelegate(Delegate* delegate);

  // Returns the URL filter for filtering navigations and classifying sites in
  // the history view. Both this method and the returned filter may only be used
  // on the UI thread.
  SupervisedUserURLFilter* GetURLFilter();

  // Returns the allowlist service.
  SupervisedUserAllowlistService* GetAllowlistService();

  const std::vector<scoped_refptr<SupervisedUserSiteList>>& allowlists() const {
    return allowlists_;
  }

  // Whether the user can request to get access to blocked URLs or to new
  // extensions.
  bool AccessRequestsEnabled();

  // Adds an access request for the given URL.
  void AddURLAccessRequest(const GURL& url, SuccessCallback callback);

  // Get the string used to identify an extension install or update request.
  // Public for testing.
  static std::string GetExtensionRequestId(const std::string& extension_id,
                                           const base::Version& version);

  // Returns the email address of the custodian.
  std::string GetCustodianEmailAddress() const;

  // Returns the obfuscated GAIA id of the custodian.
  std::string GetCustodianObfuscatedGaiaId() const;

  // Returns the name of the custodian, or the email address if the name is
  // empty.
  std::string GetCustodianName() const;

  // Returns the email address of the second custodian, or the empty string
  // if there is no second custodian.
  std::string GetSecondCustodianEmailAddress() const;

  // Returns the obfuscated GAIA id of the second custodian or the empty
  // string if there is no second custodian.
  std::string GetSecondCustodianObfuscatedGaiaId() const;

  // Returns the name of the second custodian, or the email address if the name
  // is empty, or the empty string if there is no second custodian.
  std::string GetSecondCustodianName() const;

  // Returns a message saying that extensions can only be modified by the
  // custodian.
  base::string16 GetExtensionsLockedMessage() const;

  bool IsSupervisedUserIframeFilterEnabled() const;

  static std::string GetEduCoexistenceLoginUrl();

  // Returns true if the user is a type of Family Link Child account,
  // but will not return true for a Legacy Supervised user (or non child users).
  bool IsChild() const;

  bool IsSupervisedUserExtensionInstallEnabled() const;

  // Returns true if there is a custodian for the child.  A child can have
  // up to 2 custodians, and this returns true if they have at least 1.
  bool HasACustodian() const;

  void AddObserver(SupervisedUserServiceObserver* observer);
  void RemoveObserver(SupervisedUserServiceObserver* observer);

  void AddPermissionRequestCreator(
      std::unique_ptr<PermissionRequestCreator> creator);

  // ProfileKeyedService override:
  void Shutdown() override;

  // SyncTypePreferenceProvider implementation:
  bool IsEncryptEverythingAllowed() const override;

#if !defined(OS_ANDROID)
  // BrowserListObserver implementation:
  void OnBrowserSetLastActive(Browser* browser) override;
#endif  // !defined(OS_ANDROID)

  // SupervisedUserURLFilter::Observer implementation:
  void OnSiteListUpdated() override;

#if !defined(OS_ANDROID)
  bool signout_required_after_supervision_enabled() {
    return signout_required_after_supervision_enabled_;
  }
  void set_signout_required_after_supervision_enabled() {
    signout_required_after_supervision_enabled_ = true;
  }
#endif  // !defined(OS_ANDROID)

  void SetPrimaryPermissionCreatorForTest(
      std::unique_ptr<PermissionRequestCreator> permission_creator);

#if BUILDFLAG(ENABLE_EXTENSIONS)
  // Updates the set of approved extensions to add approval for |extension|.
  void AddExtensionApproval(const extensions::Extension& extension);

  // Updates the set of approved extensions to remove approval for |extension|.
  void RemoveExtensionApproval(const extensions::Extension& extension);

  // Wraps UpdateApprovedExtension() for testing. Use this to simulate adding or
  // removing custodian approval for an extension via sync.
  void UpdateApprovedExtensionForTesting(const std::string& extension_id,
                                         ApprovedExtensionChange type);

  bool GetSupervisedUserExtensionsMayRequestPermissionsPref() const;

  void SetSupervisedUserExtensionsMayRequestPermissionsPrefForTesting(
      bool enabled);

  bool CanInstallExtensions() const;

  bool IsExtensionAllowed(const extensions::Extension& extension) const;

  void RecordExtensionEnablementUmaMetrics(bool enabled) const;
#endif  // BUILDFLAG(ENABLE_EXTENSIONS)

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

  void OnCustodianInfoChanged();

#if BUILDFLAG(ENABLE_EXTENSIONS)
  // extensions::ManagementPolicy::Provider implementation:
  std::string GetDebugPolicyProviderName() const override;
  bool UserMayLoad(const extensions::Extension* extension,
                   base::string16* error) const override;
  bool MustRemainDisabled(const extensions::Extension* extension,
                          extensions::disable_reason::DisableReason* reason,
                          base::string16* error) const override;

  // extensions::ExtensionRegistryObserver overrides:
  void OnExtensionInstalled(content::BrowserContext* browser_context,
                            const extensions::Extension* extension,
                            bool is_update) override;

  void OnExtensionUninstalled(content::BrowserContext* browser_context,
                              const extensions::Extension* extension,
                              extensions::UninstallReason reason) override;

  // An extension can be in one of the following states:
  //
  // BLOCKED: if kSupervisedUserExtensionsMayRequestPermissions is false and the
  // child user is attempting to install a new extension or an existing
  // extension is asking for additional permissions.
  // ALLOWED: Components, Themes, Default extensions ..etc
  //    are generally allowed.  Extensions that have been approved by the
  //    custodian are also allowed.
  // REQUIRE_APPROVAL: if it is installed by the child user and
  //    hasn't been approved by the custodian yet.
  enum class ExtensionState { BLOCKED, ALLOWED, REQUIRE_APPROVAL };

  // Returns the state of an extension whether being BLOCKED, ALLOWED or
  // REQUIRE_APPROVAL from the Supervised User service's point of view.
  ExtensionState GetExtensionState(
      const extensions::Extension& extension) const;

  // Returns whether we should block an extension based on the state of the
  // "Permissions for sites, apps and extensions" toggle.
  bool ShouldBlockExtension(const std::string& extension_id) const;

  // Enables/Disables extensions upon change in approvals. This function is
  // idempotent.
  void ChangeExtensionStateIfNecessary(const std::string& extension_id);

  // Updates the synced set of approved extension ids.
  // Use AddExtensionApproval() or RemoveExtensionApproval() for public access.
  // If |type| is kAdd, then add approval.
  // If |type| is kRemove, then remove approval.
  // Triggers a call to RefreshApprovedExtensionsFromPrefs() via a listener.
  // TODO(crbug/1072857): We don't need the extension version information. It's
  // only included for backwards compatibility with previous versions of Chrome.
  // Remove the version information once a sufficient number of users have
  // migrated away from M83.
  void UpdateApprovedExtension(const std::string& extension_id,
                               const std::string& version,
                               ApprovedExtensionChange type);

  // Updates the set of approved extensions when the corresponding preference is
  // changed.
  void RefreshApprovedExtensionsFromPrefs();

  // Extensions helper to SetActive().
  void SetExtensionsActive();
#endif  // BUILDFLAG(ENABLE_EXTENSIONS)

  // Returns the SupervisedUserSettingsService associated with |profile_|.
  SupervisedUserSettingsService* GetSettingsService();

  // Returns the PrefService associated with |profile_|.
  PrefService* GetPrefService();

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

  void UpdateAsyncUrlChecker();

  void OnSiteListsChanged(
      const std::vector<scoped_refptr<SupervisedUserSiteList>>& site_lists);

  // Asynchronously loads a denylist from a binary file at |path| and applies
  // it to the URL filters. If no file exists at |path| yet, downloads a file
  // from |url| and stores it at |path| first.
  void LoadDenylist(const base::FilePath& path, const GURL& url);

  void OnDenylistFileChecked(const base::FilePath& path,
                             const GURL& url,
                             bool file_exists);

  // Asynchronously loads a denylist from a binary file at |path| and applies
  // it to the URL filters.
  void LoadDenylistFromFile(const base::FilePath& path);

  void OnDenylistDownloadDone(const base::FilePath& path,
                              FileDownloader::Result result);

  void OnDenylistLoaded();

  void UpdateDenylist();

  // Updates the manual overrides for hosts in the URL filters when the
  // corresponding preference is changed.
  void UpdateManualHosts();

  // Updates the manual overrides for URLs in the URL filters when the
  // corresponding preference is changed.
  void UpdateManualURLs();

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

  // Store a set of extension ids approved by the custodian.
  // It is only relevant for SU-initiated installs.
  std::set<std::string> approved_extensions_set_;

  enum class DenylistLoadState {
    NOT_LOADED,
    LOAD_STARTED,
    LOADED
  } denylist_state_;

  SupervisedUserDenylist denylist_;
  std::unique_ptr<FileDownloader> denylist_downloader_;

  std::unique_ptr<SupervisedUserAllowlistService> allowlist_service_;

  std::vector<scoped_refptr<SupervisedUserSiteList>> allowlists_;

  // Used to create permission requests.
  std::vector<std::unique_ptr<PermissionRequestCreator>> permissions_creators_;

#if BUILDFLAG(ENABLE_EXTENSIONS)
  ScopedObserver<extensions::ExtensionRegistry,
                 extensions::ExtensionRegistryObserver>
      registry_observer_{this};
#endif

  base::ObserverList<SupervisedUserServiceObserver>::Unchecked observer_list_;

#if !defined(OS_ANDROID)
  bool signout_required_after_supervision_enabled_ = false;
#endif

  base::WeakPtrFactory<SupervisedUserService> weak_ptr_factory_{this};

  DISALLOW_COPY_AND_ASSIGN(SupervisedUserService);
};

#endif  // CHROME_BROWSER_SUPERVISED_USER_SUPERVISED_USER_SERVICE_H_
