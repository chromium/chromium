// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_PROFILES_OFF_THE_RECORD_PROFILE_IMPL_H_
#define CHROME_BROWSER_PROFILES_OFF_THE_RECORD_PROFILE_IMPL_H_

#include <memory>
#include <string>
#include <vector>

#include "base/sequenced_task_runner.h"
#include "build/build_config.h"
#include "build/chromeos_buildflags.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/scoped_profile_keep_alive.h"
#include "components/domain_reliability/clear_mode.h"
#include "content/public/browser/content_browser_client.h"

#if !defined(OS_ANDROID)
#include "chrome/browser/ui/zoom/chrome_zoom_level_prefs.h"
#include "content/public/browser/host_zoom_map.h"
#endif

namespace sync_preferences {
class PrefServiceSyncable;
}

////////////////////////////////////////////////////////////////////////////////
//
// OffTheRecordProfileImpl is a profile subclass that wraps an existing profile
// to make it suitable for the incognito mode.
//
// Note: This class is a leaf class and is not intended for subclassing.
// Providing this header file is for unit testing.
//
////////////////////////////////////////////////////////////////////////////////
class OffTheRecordProfileImpl : public Profile {
 public:
  OffTheRecordProfileImpl(Profile* real_profile,
                          const OTRProfileID& otr_profile_id);
  OffTheRecordProfileImpl(const OffTheRecordProfileImpl&) = delete;
  OffTheRecordProfileImpl& operator=(const OffTheRecordProfileImpl&) = delete;
  ~OffTheRecordProfileImpl() override;
  void Init();

  // Profile implementation.
  std::string GetProfileUserName() const override;
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
  PrefService* GetOffTheRecordPrefs() override;
  policy::SchemaRegistryService* GetPolicySchemaRegistryService() override;
#if BUILDFLAG(IS_CHROMEOS_ASH)
  policy::UserCloudPolicyManagerChromeOS* GetUserCloudPolicyManagerChromeOS()
      override;
  policy::ActiveDirectoryPolicyManager* GetActiveDirectoryPolicyManager()
      override;
#else
  policy::UserCloudPolicyManager* GetUserCloudPolicyManager() override;
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)
  scoped_refptr<network::SharedURLLoaderFactory> GetURLLoaderFactory() override;
  bool IsSameOrParent(Profile* profile) override;
  base::Time GetStartTime() const override;
  ProfileKey* GetProfileKey() const override;
  policy::ProfilePolicyConnector* GetProfilePolicyConnector() override;
  const policy::ProfilePolicyConnector* GetProfilePolicyConnector()
      const override;
  base::FilePath last_selected_directory() override;
  void set_last_selected_directory(const base::FilePath& path) override;
  bool WasCreatedByVersionOrLater(const std::string& version) override;
  void SetExitType(ExitType exit_type) override;
  ExitType GetLastSessionExitType() const override;

#if BUILDFLAG(IS_CHROMEOS_ASH)
  void ChangeAppLocale(const std::string& locale, AppLocaleChangedVia) override;
  void OnLogin() override;
  void InitChromeOSPreferences() override;
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)

  // Returns whether the wrapped underlying profile is new.
  bool IsNewProfile() const override;

  GURL GetHomePage() override;
  void SetCreationTimeForTesting(base::Time creation_time) override;

  // content::BrowserContext implementation:
  base::FilePath GetPath() override;
  base::FilePath GetPath() const override;
  base::Time GetCreationTime() const override;
#if !defined(OS_ANDROID)
  std::unique_ptr<content::ZoomLevelDelegate> CreateZoomLevelDelegate(
      const base::FilePath& partition_path) override;
#endif  // !defined(OS_ANDROID)
  scoped_refptr<base::SequencedTaskRunner> GetIOTaskRunner() override;
  bool IsOffTheRecord() override;
  bool IsOffTheRecord() const override;
#if BUILDFLAG(IS_CHROMEOS_LACROS)
  bool IsMainProfile() const override;
#endif  // BUILDFLAG(IS_CHROMEOS_LACROS)
  const OTRProfileID& GetOTRProfileID() const override;
  content::DownloadManagerDelegate* GetDownloadManagerDelegate() override;
  content::BrowserPluginGuestManager* GetGuestManager() override;
  storage::SpecialStoragePolicy* GetSpecialStoragePolicy() override;
  content::PushMessagingService* GetPushMessagingService() override;
  content::StorageNotificationService* GetStorageNotificationService() override;
  content::SSLHostStateDelegate* GetSSLHostStateDelegate() override;
  content::PermissionControllerDelegate* GetPermissionControllerDelegate()
      override;
  content::ClientHintsControllerDelegate* GetClientHintsControllerDelegate()
      override;
  content::BackgroundFetchDelegate* GetBackgroundFetchDelegate() override;
  content::BackgroundSyncController* GetBackgroundSyncController() override;
  content::BrowsingDataRemoverDelegate* GetBrowsingDataRemoverDelegate()
      override;
  std::unique_ptr<media::VideoDecodePerfHistory> CreateVideoDecodePerfHistory()
      override;
  content::FileSystemAccessPermissionContext*
  GetFileSystemAccessPermissionContext() override;
  void RecordMainFrameNavigation() override;

 protected:
  // Profile implementation.
  bool IsSignedIn() override;

 private:
#if !defined(OS_ANDROID)
  // Allows a profile to track changes in zoom levels in its parent profile.
  void TrackZoomLevelsFromParent();
#endif  // !defined(OS_ANDROID)

#if !defined(OS_ANDROID)
  // Callback function for tracking parent's zoom level changes.
  void OnParentZoomLevelChanged(
      const content::HostZoomMap::ZoomLevelChange& change);
  void UpdateDefaultZoomLevel();
#endif  // !defined(OS_ANDROID)

  // The real underlying profile.
  Profile* profile_;
  // Prevent |profile_| from being destroyed first.
  ScopedProfileKeepAlive profile_keep_alive_;

  const OTRProfileID otr_profile_id_;

  std::unique_ptr<sync_preferences::PrefServiceSyncable> prefs_;

#if !defined(OS_ANDROID)
  base::CallbackListSubscription track_zoom_subscription_;
  base::CallbackListSubscription parent_default_zoom_level_subscription_;
#endif  // !defined(OS_ANDROID)

  // Time we were started.
  base::Time start_time_;

  // The key to index KeyedService instances created by
  // SimpleKeyedServiceFactory.
  std::unique_ptr<ProfileKey> key_;

  base::FilePath last_selected_directory_;

  // Number of main frame navigations done by this profile.
  unsigned int main_frame_navigations_ = 0;
};

#endif  // CHROME_BROWSER_PROFILES_OFF_THE_RECORD_PROFILE_IMPL_H_
