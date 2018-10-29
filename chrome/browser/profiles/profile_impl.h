// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// This class gathers state related to a single user profile.

#ifndef CHROME_BROWSER_PROFILES_PROFILE_IMPL_H_
#define CHROME_BROWSER_PROFILES_PROFILE_IMPL_H_

#include <memory>
#include <string>

#include "base/files/file_path.h"
#include "base/gtest_prod_util.h"
#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "build/build_config.h"
#include "chrome/browser/net/reporting_permissions_checker.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_impl_io_data.h"
#include "chrome/common/buildflags.h"
#include "components/prefs/pref_change_registrar.h"
#include "content/public/browser/content_browser_client.h"
#include "extensions/buildflags/buildflags.h"

#if !defined(OS_ANDROID)
#include "chrome/browser/ui/zoom/chrome_zoom_level_prefs.h"
#include "content/public/browser/host_zoom_map.h"
#endif

class MediaDeviceIDSalt;
class PrefService;

#if defined(OS_CHROMEOS)
namespace chromeos {
class KioskTest;
class LocaleChangeGuard;
class Preferences;
class SupervisedUserTestBase;
}
#endif

namespace base {
class SequencedTaskRunner;
}

namespace domain_reliability {
class DomainReliabilityMonitor;
}

namespace policy {
class ConfigurationPolicyProvider;
class ProfilePolicyConnector;
class SchemaRegistryService;
}

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

  ~ProfileImpl() override;

  static void RegisterProfilePrefs(user_prefs::PrefRegistrySyncable* registry);

  // content::BrowserContext implementation:
#if !defined(OS_ANDROID)
  std::unique_ptr<content::ZoomLevelDelegate> CreateZoomLevelDelegate(
      const base::FilePath& partition_path) override;
#endif
  base::FilePath GetPath() const override;
  base::FilePath GetCachePath() const override;
  content::DownloadManagerDelegate* GetDownloadManagerDelegate() override;
  content::ResourceContext* GetResourceContext() override;
  content::BrowserPluginGuestManager* GetGuestManager() override;
  storage::SpecialStoragePolicy* GetSpecialStoragePolicy() override;
  content::PushMessagingService* GetPushMessagingService() override;
  content::SSLHostStateDelegate* GetSSLHostStateDelegate() override;
  content::BrowsingDataRemoverDelegate* GetBrowsingDataRemoverDelegate()
      override;
  content::PermissionControllerDelegate* GetPermissionControllerDelegate()
      override;
  content::BackgroundFetchDelegate* GetBackgroundFetchDelegate() override;
  content::BackgroundSyncController* GetBackgroundSyncController() override;
  net::URLRequestContextGetter* CreateRequestContext(
      content::ProtocolHandlerMap* protocol_handlers,
      content::URLRequestInterceptorScopedVector request_interceptors) override;
  net::URLRequestContextGetter* CreateRequestContextForStoragePartition(
      const base::FilePath& partition_path,
      bool in_memory,
      content::ProtocolHandlerMap* protocol_handlers,
      content::URLRequestInterceptorScopedVector request_interceptors) override;
  net::URLRequestContextGetter* CreateMediaRequestContext() override;
  net::URLRequestContextGetter* CreateMediaRequestContextForStoragePartition(
      const base::FilePath& partition_path,
      bool in_memory) override;
  void RegisterInProcessServices(StaticServiceMap* services) override;
  std::string GetMediaDeviceIDSalt() override;
  download::InProgressDownloadManager* RetriveInProgressDownloadManager()
      override;

  // Profile implementation:
  scoped_refptr<base::SequencedTaskRunner> GetIOTaskRunner() override;
  // Note that this implementation returns the Google-services username, if any,
  // not the Chrome user's display name.
  std::string GetProfileUserName() const override;
  ProfileType GetProfileType() const override;
  bool IsOffTheRecord() const override;
  Profile* GetOffTheRecordProfile() override;
  void DestroyOffTheRecordProfile() override;
  bool HasOffTheRecordProfile() override;
  Profile* GetOriginalProfile() override;
  const Profile* GetOriginalProfile() const override;
  bool IsSupervised() const override;
  bool IsChild() const override;
  bool IsLegacySupervised() const override;
  ExtensionSpecialStoragePolicy* GetExtensionSpecialStoragePolicy() override;
  PrefService* GetPrefs() override;
  const PrefService* GetPrefs() const override;
#if !defined(OS_ANDROID)
  ChromeZoomLevelPrefs* GetZoomLevelPrefs() override;
#endif
  PrefService* GetOffTheRecordPrefs() override;
  PrefService* GetReadOnlyOffTheRecordPrefs() override;
  net::URLRequestContextGetter* GetRequestContext() override;
  base::OnceCallback<net::CookieStore*()> GetExtensionsCookieStoreGetter()
      override;
  scoped_refptr<network::SharedURLLoaderFactory> GetURLLoaderFactory() override;
  bool IsSameProfile(Profile* profile) override;
  base::Time GetStartTime() const override;
  base::FilePath last_selected_directory() override;
  void set_last_selected_directory(const base::FilePath& path) override;
  GURL GetHomePage() override;
  bool WasCreatedByVersionOrLater(const std::string& version) override;
  void SetExitType(ExitType exit_type) override;
  ExitType GetLastSessionExitType() override;
  bool ShouldRestoreOldSessionCookies() override;
  bool ShouldPersistSessionCookies() override;

#if defined(OS_CHROMEOS)
  void ChangeAppLocale(const std::string& locale, AppLocaleChangedVia) override;
  void OnLogin() override;
  void InitChromeOSPreferences() override;
#endif  // defined(OS_CHROMEOS)

 private:
#if defined(OS_CHROMEOS)
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
              scoped_refptr<base::SequencedTaskRunner> io_task_runner);

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

  void GetMediaCacheParameters(base::FilePath* cache_path, int* max_size);

  std::unique_ptr<domain_reliability::DomainReliabilityMonitor>
  CreateDomainReliabilityMonitor(PrefService* local_state);

  // Creates an instance of the Identity Service for this Profile, populating it
  // with the appropriate instances of its dependencies.
  std::unique_ptr<service_manager::Service> CreateIdentityService();

#if defined(OS_CHROMEOS)
  std::unique_ptr<service_manager::Service> CreateDeviceSyncService();
  std::unique_ptr<service_manager::Service> CreateMultiDeviceSetupService();
#endif  // defined(OS_CHROMEOS)

  PrefChangeRegistrar pref_change_registrar_;

  base::FilePath path_;
  base::FilePath base_cache_path_;

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
  // |profile_policy_connector_| in turn depends on
  // |configuration_policy_provider_|, which depends on
  // |schema_registry_service_|.
  std::unique_ptr<policy::SchemaRegistryService> schema_registry_service_;
  std::unique_ptr<policy::ConfigurationPolicyProvider>
      configuration_policy_provider_;
  std::unique_ptr<policy::ProfilePolicyConnector> profile_policy_connector_;

  // Keep |prefs_| on top for destruction order because |extension_prefs_|,
  // |io_data_| and others store pointers to |prefs_| and shall be destructed
  // first.
  scoped_refptr<user_prefs::PrefRegistrySyncable> pref_registry_;
  std::unique_ptr<sync_preferences::PrefServiceSyncable> prefs_;
  // See comment in GetOffTheRecordPrefs. Field exists so something owns the
  // dummy.
  std::unique_ptr<sync_preferences::PrefServiceSyncable> dummy_otr_prefs_;
  ProfileImplIOData::Handle io_data_;
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

  std::unique_ptr<Profile> off_the_record_profile_;

  // See GetStartTime for details.
  base::Time start_time_;

#if defined(OS_CHROMEOS)
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

  ReportingPermissionsCheckerFactory reporting_permissions_checker_factory_;

  DISALLOW_COPY_AND_ASSIGN(ProfileImpl);
};

#endif  // CHROME_BROWSER_PROFILES_PROFILE_IMPL_H_
