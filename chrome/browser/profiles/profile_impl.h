// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// This class gathers state related to a single user profile.

#ifndef CHROME_BROWSER_PROFILES_PROFILE_IMPL_H_
#define CHROME_BROWSER_PROFILES_PROFILE_IMPL_H_

#include <map>
#include <memory>
#include <string>
#include <vector>

#include "base/files/file_path.h"
#include "base/gtest_prod_util.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/ref_counted.h"
#include "base/time/time.h"
#include "base/timer/timer.h"
#include "build/build_config.h"
#include "build/chromeos_buildflags.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/zoom/chrome_zoom_level_prefs.h"
#include "chrome/common/buildflags.h"
#include "components/keyed_service/core/simple_factory_key.h"
#include "components/prefs/pref_change_registrar.h"
#include "content/public/browser/host_zoom_map.h"
#include "extensions/buildflags/buildflags.h"

class PrefService;

#if BUILDFLAG(IS_CHROMEOS)
namespace ash {
class KioskBaseTest;
class LocaleChangeGuard;
class Preferences;
}  // namespace ash
#endif

namespace base {
class SequencedTaskRunner;
}

namespace policy {
class ConfigurationPolicyProvider;
class ProfilePolicyConnector;
class ProfileCloudPolicyManager;
}  // namespace policy

namespace sync_preferences {
class PrefServiceSyncable;
}

namespace user_prefs {
class PrefRegistrySyncable;
}

// The default profile implementation.
class ProfileImpl : public Profile {
 public:
  ProfileImpl(const ProfileImpl&) = delete;
  ProfileImpl& operator=(const ProfileImpl&) = delete;
  ~ProfileImpl() override;

  static void RegisterProfilePrefs(user_prefs::PrefRegistrySyncable* registry);

  // content::BrowserContext implementation:
  std::unique_ptr<content::ZoomLevelDelegate> CreateZoomLevelDelegate(
      const base::FilePath& partition_path) override;
  content::DownloadManagerDelegate* GetDownloadManagerDelegate() override;
  content::BrowserPluginGuestManager* GetGuestManager() override;
  storage::SpecialStoragePolicy* GetSpecialStoragePolicy() override;
  content::PlatformNotificationService* GetPlatformNotificationService()
      override;
  content::PushMessagingService* GetPushMessagingService() override;
  content::StorageNotificationService* GetStorageNotificationService() override;
  content::SSLHostStateDelegate* GetSSLHostStateDelegate() override;
  content::BrowsingDataRemoverDelegate* GetBrowsingDataRemoverDelegate()
      override;
  content::PermissionControllerDelegate* GetPermissionControllerDelegate()
      override;
  content::ClientHintsControllerDelegate* GetClientHintsControllerDelegate()
      override;
  content::BackgroundFetchDelegate* GetBackgroundFetchDelegate() override;
  content::BackgroundSyncController* GetBackgroundSyncController() override;
  content::ReduceAcceptLanguageControllerDelegate*
  GetReduceAcceptLanguageControllerDelegate() override;
  std::unique_ptr<download::InProgressDownloadManager>
  RetrieveInProgressDownloadManager() override;
  content::FileSystemAccessPermissionContext*
  GetFileSystemAccessPermissionContext() override;
  content::ContentIndexProvider* GetContentIndexProvider() override;
  content::FederatedIdentityApiPermissionContextDelegate*
  GetFederatedIdentityApiPermissionContext() override;
  content::FederatedIdentityAutoReauthnPermissionContextDelegate*
  GetFederatedIdentityAutoReauthnPermissionContext() override;
  content::FederatedIdentityPermissionContextDelegate*
  GetFederatedIdentityPermissionContext() override;
  content::KAnonymityServiceDelegate* GetKAnonymityServiceDelegate() override;
  content::OriginTrialsControllerDelegate* GetOriginTrialsControllerDelegate()
      override;

  // Profile implementation:
  scoped_refptr<base::SequencedTaskRunner> GetIOTaskRunner() override;
  // Note that this implementation returns the Google-services username, if any,
  // not the Chrome user's display name.
  std::string GetProfileUserName() const override;
  base::FilePath GetPath() override;
  base::Time GetCreationTime() const override;
  base::FilePath GetPath() const override;
  Profile* GetOffTheRecordProfile(const OTRProfileID& otr_profile_id,
                                  bool create_if_needed) override;
  std::vector<Profile*> GetAllOffTheRecordProfiles() override;
  void DestroyOffTheRecordProfile(Profile* otr_profile) override;
  bool HasOffTheRecordProfile(const OTRProfileID& otr_profile_id) override;
  bool HasAnyOffTheRecordProfile() override;
  Profile* GetOriginalProfile() override;
  const Profile* GetOriginalProfile() const override;
  bool IsChild() const override;
  bool AllowsBrowserWindows() const override;
  ExtensionSpecialStoragePolicy* GetExtensionSpecialStoragePolicy() override;
  PrefService* GetPrefs() override;
  const PrefService* GetPrefs() const override;
  ChromeZoomLevelPrefs* GetZoomLevelPrefs() override;
  PrefService* GetReadOnlyOffTheRecordPrefs() override;
  policy::SchemaRegistryService* GetPolicySchemaRegistryService() override;
#if BUILDFLAG(IS_CHROMEOS)
  policy::UserCloudPolicyManagerAsh* GetUserCloudPolicyManagerAsh() override;
#else
  policy::UserCloudPolicyManager* GetUserCloudPolicyManager() override;
  policy::ProfileCloudPolicyManager* GetProfileCloudPolicyManager() override;
#endif  // BUILDFLAG(IS_CHROMEOS)
  policy::CloudPolicyManager* GetCloudPolicyManager() override;
  policy::ProfilePolicyConnector* GetProfilePolicyConnector() override;
  const policy::ProfilePolicyConnector* GetProfilePolicyConnector()
      const override;
  scoped_refptr<network::SharedURLLoaderFactory> GetURLLoaderFactory() override;
  bool IsSameOrParent(Profile* profile) override;
  base::Time GetStartTime() const override;
  ProfileKey* GetProfileKey() const override;
  base::FilePath last_selected_directory() override;
  void set_last_selected_directory(const base::FilePath& path) override;
  GURL GetHomePage() override;
  bool WasCreatedByVersionOrLater(const std::string& version) override;
  bool ShouldRestoreOldSessionCookies() override;
  bool ShouldPersistSessionCookies() const override;

#if BUILDFLAG(IS_CHROMEOS)
  void ChangeAppLocale(const std::string& locale, AppLocaleChangedVia) override;
  void OnLogin() override;
  void InitChromeOSPreferences() override;
#endif  // BUILDFLAG(IS_CHROMEOS)

  bool IsNewProfile() const override;

  void SetCreationTimeForTesting(base::Time creation_time) override;
  void RecordPrimaryMainFrameNavigation() override {}

 protected:
  // Profile implementation.
  bool IsSignedIn() override;

 private:
#if BUILDFLAG(IS_CHROMEOS)
  friend class ash::KioskBaseTest;
#endif
  friend class Profile;
  FRIEND_TEST_ALL_PREFIXES(StartupBrowserCreatorTest,
                           ProfilesLaunchedAfterCrash);
  FRIEND_TEST_ALL_PREFIXES(ProfileBrowserTest, DISABLED_ProfileReadmeCreated);
  FRIEND_TEST_ALL_PREFIXES(ProfileBrowserTest,
                           ProfileDeletedBeforeReadmeCreated);
  FRIEND_TEST_ALL_PREFIXES(ProfileBrowserTest, DiskCacheDirOverride);

  ProfileImpl(const base::FilePath& path,
              Delegate* delegate,
              CreateMode create_mode,
              base::Time path_creation_time,
              scoped_refptr<base::SequencedTaskRunner> io_task_runner);

#if BUILDFLAG(IS_ANDROID)
  // Takes the ownership of the pre-created PrefService and other objects if
  // they have been created.
  void TakePrefsFromStartupData();
#endif

  // Creates |prefs| from scratch in normal startup.
  void LoadPrefsForNormalStartup(bool async_prefs);

  // Does final initialization. Should be called after prefs were loaded.
  void DoFinalInit(CreateMode create_mode);

  // Switch locale (when possible) and proceed to OnLocaleReady().
  void OnPrefsLoaded(CreateMode create_mode, bool success);

  // Does final prefs initialization and calls Init().
  void OnLocaleReady(CreateMode create_mode);

#if BUILDFLAG(ENABLE_SESSION_SERVICE)
  void StopCreateSessionServiceTimer();

  void EnsureSessionServiceCreated();
#endif

  // Updates the ProfileAttributesStorage with the data from this profile.
  void UpdateSupervisedUserIdInStorage();
  void UpdateNameInStorage();
  void UpdateAvatarInStorage();
  void UpdateIsEphemeralInStorage();

  // Called to initialize Data Reduction Proxy.
  void InitializeDataReductionProxy();

  // Called after a profile is initialized, to record 'one per profile creation'
  // metrics relating to user prefs.
  void RecordPrefValuesAfterProfileInitialization();

  policy::ConfigurationPolicyProvider* configuration_policy_provider();

  base::FilePath path_;

  base::Time path_creation_time_;

  // Task runner used for file access in the profile path.
  scoped_refptr<base::SequencedTaskRunner> io_task_runner_;

  // !!! BIG HONKING WARNING !!!
  //  The order of the members below is important. Do not change it unless
  //  you know what you're doing. Also, if adding a new member here make sure
  //  that the declaration occurs AFTER things it depends on as destruction
  //  happens in reverse order of declaration.

  // TODO(mnissler, joaodasilva): The |profile_policy_connector_| provides the
  // PolicyService that the |prefs_| depend on, and must outlive |prefs_|. This
  // can be removed once |prefs_| becomes a KeyedService too.

  // - |prefs_| depends on |profile_policy_connector_|
  // - |profile_policy_connector_| depends on configuration_policy_provider(),
  //   which can be:
  //     - |user_cloud_policy_manager_|;
  //     - |user_cloud_policy_manager_ash_|;
  // - configuration_policy_provider() depends on |schema_registry_service_|

  std::unique_ptr<policy::SchemaRegistryService> schema_registry_service_;

  // configuration_policy_provider() is either of these, or nullptr in some
  // tests.
#if BUILDFLAG(IS_CHROMEOS)
  std::unique_ptr<policy::UserCloudPolicyManagerAsh>
      user_cloud_policy_manager_ash_;
#else
  std::unique_ptr<policy::UserCloudPolicyManager> user_cloud_policy_manager_;
  std::unique_ptr<policy::ProfileCloudPolicyManager>
      profile_cloud_policy_manager_;
#endif

  std::unique_ptr<policy::ProfilePolicyConnector> profile_policy_connector_;

  // Keep `prefs_` on top for destruction order because `dummy_otr_prefs_`,
  // `pref_change_registrar_` and others store pointers to `prefs_` and shall be
  // destructed first.
  scoped_refptr<user_prefs::PrefRegistrySyncable> pref_registry_;
  std::unique_ptr<sync_preferences::PrefServiceSyncable> prefs_;
  std::unique_ptr<sync_preferences::PrefServiceSyncable> dummy_otr_prefs_;
  PrefChangeRegistrar pref_change_registrar_;
#if BUILDFLAG(ENABLE_EXTENSIONS)
  scoped_refptr<ExtensionSpecialStoragePolicy>
      extension_special_storage_policy_;
#endif

#if BUILDFLAG(ENABLE_SESSION_SERVICE)
  base::OneShotTimer create_session_service_timer_;
#endif

  std::map<OTRProfileID, std::unique_ptr<Profile>> otr_profiles_;

  // See GetStartTime for details.
  base::Time start_time_;

  // The key to index KeyedService instances created by
  // SimpleKeyedServiceFactory.
  std::unique_ptr<ProfileKey> key_;

#if BUILDFLAG(IS_CHROMEOS)
  std::unique_ptr<ash::Preferences> chromeos_preferences_;

  std::unique_ptr<ash::LocaleChangeGuard> locale_change_guard_;
#endif

  // STOP!!!! DO NOT ADD ANY MORE ITEMS HERE!!!!
  //
  // Instead, make your Service/Manager/whatever object you're hanging off the
  // Profile use our new BrowserContextKeyedServiceFactory system instead.
  // You can find the design document here:
  //
  //   https://sites.google.com/a/chromium.org/dev/developers/design-documents/profile-architecture
  //
  // and you can read the raw headers here:
  //
  // components/keyed_service/content/browser_context_dependency_manager.*
  // components/keyed_service/core/keyed_service.h
  // components/keyed_service/content/browser_context_keyed_service_factory.*

  raw_ptr<Profile::Delegate> delegate_;
};

#endif  // CHROME_BROWSER_PROFILES_PROFILE_IMPL_H_
