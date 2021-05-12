// Copyright (c) 2012 The Chromium Authors. All rights reserved.
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
#include "base/memory/ref_counted.h"
#include "base/optional.h"
#include "base/time/time.h"
#include "base/timer/timer.h"
#include "build/build_config.h"
#include "build/chromeos_buildflags.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/common/buildflags.h"
#include "components/keyed_service/core/simple_factory_key.h"
#include "components/prefs/pref_change_registrar.h"
#include "content/public/browser/content_browser_client.h"
#include "extensions/buildflags/buildflags.h"

#if !defined(OS_ANDROID)
#include "chrome/browser/ui/zoom/chrome_zoom_level_prefs.h"
#include "content/public/browser/host_zoom_map.h"
#endif

class MediaDeviceIDSalt;
class PrefService;

#if BUILDFLAG(IS_CHROMEOS_ASH)
namespace chromeos {
class KioskTest;
class LocaleChangeGuard;
class Preferences;
class SupervisedUserTestBase;
}  // namespace chromeos
#endif

namespace base {
class SequencedTaskRunner;
}

namespace policy {
class ConfigurationPolicyProvider;
class ProfilePolicyConnector;
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
  // Value written to prefs when the exit type is EXIT_NORMAL. Public for tests.
  static const char kPrefExitTypeNormal[];

  ProfileImpl(const ProfileImpl&) = delete;
  ProfileImpl& operator=(const ProfileImpl&) = delete;
  ~ProfileImpl() override;

  static void RegisterProfilePrefs(user_prefs::PrefRegistrySyncable* registry);

  // content::BrowserContext implementation:
#if !defined(OS_ANDROID)
  std::unique_ptr<content::ZoomLevelDelegate> CreateZoomLevelDelegate(
      const base::FilePath& partition_path) override;
#endif
  content::DownloadManagerDelegate* GetDownloadManagerDelegate() override;
  content::BrowserPluginGuestManager* GetGuestManager() override;
  storage::SpecialStoragePolicy* GetSpecialStoragePolicy() override;
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
  std::string GetMediaDeviceIDSalt() override;
  download::InProgressDownloadManager* RetriveInProgressDownloadManager()
      override;
  content::FileSystemAccessPermissionContext*
  GetFileSystemAccessPermissionContext() override;
  content::ContentIndexProvider* GetContentIndexProvider() override;

  // Profile implementation:
  scoped_refptr<base::SequencedTaskRunner> GetIOTaskRunner() override;
  // Note that this implementation returns the Google-services username, if any,
  // not the Chrome user's display name.
  std::string GetProfileUserName() const override;
  base::FilePath GetPath() override;
  base::Time GetCreationTime() const override;
  bool IsOffTheRecord() override;
  bool IsOffTheRecord() const override;
#if BUILDFLAG(IS_CHROMEOS_LACROS)
  bool IsMainProfile() const override;
#endif  // BUILDFLAG(IS_CHROMEOS_LACROS)
  const OTRProfileID& GetOTRProfileID() const override;
  base::FilePath GetPath() const override;
  Profile* GetOffTheRecordProfile(const OTRProfileID& otr_profile_id,
                                  bool create_if_needed) override;
  std::vector<Profile*> GetAllOffTheRecordProfiles() override;
  void DestroyOffTheRecordProfile(Profile* otr_profile) override;
  bool HasOffTheRecordProfile(const OTRProfileID& otr_profile_id) override;
  bool HasAnyOffTheRecordProfile() override;
  Profile* GetOriginalProfile() override;
  const Profile* GetOriginalProfile() const override;
  bool IsSupervised() const override;
  bool IsChild() const override;
  bool AllowsBrowserWindows() const override;
  ExtensionSpecialStoragePolicy* GetExtensionSpecialStoragePolicy() override;
  PrefService* GetPrefs() override;
  const PrefService* GetPrefs() const override;
#if !defined(OS_ANDROID)
  ChromeZoomLevelPrefs* GetZoomLevelPrefs() override;
#endif
  // TODO(https://crbug.com/1065444): Only supports primary OTR profile. Either
  // update to support all OTR profiles or remove this function.
  PrefService* GetOffTheRecordPrefs() override;
  PrefService* GetReadOnlyOffTheRecordPrefs() override;
  policy::SchemaRegistryService* GetPolicySchemaRegistryService() override;
#if BUILDFLAG(IS_CHROMEOS_ASH)
  policy::UserCloudPolicyManagerChromeOS* GetUserCloudPolicyManagerChromeOS()
      override;
  policy::ActiveDirectoryPolicyManager* GetActiveDirectoryPolicyManager()
      override;
#else
  policy::UserCloudPolicyManager* GetUserCloudPolicyManager() override;
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)
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
  void SetExitType(ExitType exit_type) override;
  ExitType GetLastSessionExitType() const override;
  bool ShouldRestoreOldSessionCookies() const override;
  bool ShouldPersistSessionCookies() const override;

#if BUILDFLAG(IS_CHROMEOS_ASH)
  void ChangeAppLocale(const std::string& locale, AppLocaleChangedVia) override;
  void OnLogin() override;
  void InitChromeOSPreferences() override;
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)

  bool IsNewProfile() const override;

  void SetCreationTimeForTesting(base::Time creation_time) override;
  void RecordMainFrameNavigation() override {}

 protected:
  // Profile implementation.
  bool IsSignedIn() override;

 private:
#if BUILDFLAG(IS_CHROMEOS_ASH)
  friend class chromeos::KioskTest;
  friend class chromeos::SupervisedUserTestBase;
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

#if defined(OS_ANDROID)
  // Takes the ownership of the pre-created PrefService and other objects if
  // they have been created.
  void TakePrefsFromStartupData();
#endif

  // Creates |prefs| from scratch in normal startup.
  void LoadPrefsForNormalStartup(bool async_prefs);

  // Does final initialization. Should be called after prefs were loaded.
  void DoFinalInit();

  // Switch locale (when possible) and proceed to OnLocaleReady().
  void OnPrefsLoaded(CreateMode create_mode, bool success);

  // Does final prefs initialization and calls Init().
  void OnLocaleReady();

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

  policy::ConfigurationPolicyProvider* configuration_policy_provider();

  PrefChangeRegistrar pref_change_registrar_;

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
  //     - |user_cloud_policy_manager_chromeos_|;
  //     - or |active_directory_policy_manager_|.
  // - configuration_policy_provider() depends on |schema_registry_service_|

  std::unique_ptr<policy::SchemaRegistryService> schema_registry_service_;

  // configuration_policy_provider() is either of these, or nullptr in some
  // tests.
#if BUILDFLAG(IS_CHROMEOS_ASH)
  std::unique_ptr<policy::UserCloudPolicyManagerChromeOS>
      user_cloud_policy_manager_chromeos_;
  std::unique_ptr<policy::ActiveDirectoryPolicyManager>
      active_directory_policy_manager_;
#else
  std::unique_ptr<policy::UserCloudPolicyManager> user_cloud_policy_manager_;
#endif

  std::unique_ptr<policy::ProfilePolicyConnector> profile_policy_connector_;

  // Keep |prefs_| on top for destruction order because |extension_prefs_|,
  // |io_data_| and others store pointers to |prefs_| and shall be destructed
  // first.
  scoped_refptr<user_prefs::PrefRegistrySyncable> pref_registry_;
  std::unique_ptr<sync_preferences::PrefServiceSyncable> prefs_;
  // See comment in GetOffTheRecordPrefs. Field exists so something owns the
  // dummy.
  std::unique_ptr<sync_preferences::PrefServiceSyncable> dummy_otr_prefs_;
#if BUILDFLAG(ENABLE_EXTENSIONS)
  scoped_refptr<ExtensionSpecialStoragePolicy>
      extension_special_storage_policy_;
#endif

  // Exit type the last time the profile was opened. This is set only once from
  // prefs.
  ExitType last_session_exit_type_;

#if BUILDFLAG(ENABLE_SESSION_SERVICE)
  base::OneShotTimer create_session_service_timer_;
#endif

  std::map<OTRProfileID, std::unique_ptr<Profile>> otr_profiles_;

  // See GetStartTime for details.
  base::Time start_time_;

  // The key to index KeyedService instances created by
  // SimpleKeyedServiceFactory.
  std::unique_ptr<ProfileKey> key_;

#if BUILDFLAG(IS_CHROMEOS_ASH)
  std::unique_ptr<chromeos::Preferences> chromeos_preferences_;

  std::unique_ptr<chromeos::LocaleChangeGuard> locale_change_guard_;
#endif

  // TODO(mmenke):  This should be removed from the Profile, and use a
  // BrowserContextKeyedService instead.
  // See https://crbug.com/713733
  scoped_refptr<MediaDeviceIDSalt> media_device_id_salt_;

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

  Profile::Delegate* delegate_;
};

#endif  // CHROME_BROWSER_PROFILES_PROFILE_IMPL_H_
