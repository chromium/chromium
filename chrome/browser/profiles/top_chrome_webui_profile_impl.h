// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_PROFILES_TOP_CHROME_WEBUI_PROFILE_IMPL_H_
#define CHROME_BROWSER_PROFILES_TOP_CHROME_WEBUI_PROFILE_IMPL_H_

#include <memory>
#include <string>
#include <vector>

#include "base/files/file_path.h"
#include "base/task/sequenced_task_runner.h"
#include "base/time/time.h"
#include "build/build_config.h"
#include "chrome/browser/profiles/profile.h"

namespace sync_preferences {
class PrefServiceSyncable;
}

// A lightweight, off-the-record Profile dedicated to hosting Top Chrome WebUI
// renderer processes. It has no path and keeps no data on disk, has no
// KeyedServices unless their factory explicitly allows it, and is not tied to
// any user browsing session or general web navigation. Unlike other
// off-the-record profiles, it has no original profile. It is not managed by
// ProfileManager; see chrome/browser/ui/webui/top_chrome/
// top_chrome_webui_profile.h for its owner.
class TopChromeWebUIProfileImpl : public Profile {
 public:
  TopChromeWebUIProfileImpl();
  TopChromeWebUIProfileImpl(const TopChromeWebUIProfileImpl&) = delete;
  TopChromeWebUIProfileImpl& operator=(const TopChromeWebUIProfileImpl&) =
      delete;
  ~TopChromeWebUIProfileImpl() override;

  // Profile implementation:
  std::string GetProfileUserName() const override;
  Profile* GetOffTheRecordProfile(const OTRProfileID& otr_profile_id,
                                  bool create_if_needed) override;
  std::vector<Profile*> GetAllOffTheRecordProfiles() override;
  void DestroyOffTheRecordProfile(Profile* otr_profile) override;
  bool HasOffTheRecordProfile(const OTRProfileID& otr_profile_id) override;
  bool HasAnyOffTheRecordProfile() override;
  Profile* GetOriginalProfile() override;
  const Profile* GetOriginalProfile() const override;
  bool IsChild() const override;
  ExtensionSpecialStoragePolicy* GetExtensionSpecialStoragePolicy() override;
  PrefService* GetPrefs() override;
  const PrefService* GetPrefs() const override;
  policy::SchemaRegistryService* GetPolicySchemaRegistryService() override;
#if BUILDFLAG(IS_CHROMEOS)
  policy::UserCloudPolicyManagerAsh* GetUserCloudPolicyManagerAsh() override;
#else
  policy::UserCloudPolicyManager* GetUserCloudPolicyManager() override;
  policy::ProfileCloudPolicyManager* GetProfileCloudPolicyManager() override;
#endif  // BUILDFLAG(IS_CHROMEOS)
  policy::CloudPolicyManager* GetCloudPolicyManager() override;
  bool IsSameOrParent(Profile* profile) override;
  base::Time GetStartTime() const override;
  ProfileKey* GetProfileKey() const override;
  policy::ProfilePolicyConnector* GetProfilePolicyConnector() override;
  const policy::ProfilePolicyConnector* GetProfilePolicyConnector()
      const override;
  base::FilePath last_selected_directory() override;
  void set_last_selected_directory(const base::FilePath& path) override;
  bool WasCreatedByVersionOrLater(const std::string& version) override;

#if BUILDFLAG(IS_CHROMEOS)
  void ChangeAppLocale(const std::string& locale, AppLocaleChangedVia) override;
  void OnLogin() override;
  void InitChromeOSPreferences() override;
#endif  // BUILDFLAG(IS_CHROMEOS)

  bool IsNewProfile() const override;
  GURL GetHomePage() override;
  void SetCreationTimeForTesting(base::Time creation_time) override;

  // content::BrowserContext implementation:
  base::FilePath GetPath() const override;
  base::Time GetCreationTime() const override;
  std::unique_ptr<content::ZoomLevelDelegate> CreateZoomLevelDelegate(
      const base::FilePath& partition_path) override;
  scoped_refptr<base::SequencedTaskRunner> GetIOTaskRunner() override;
  content::DownloadManagerDelegate* GetDownloadManagerDelegate() override;
  content::BrowserPluginGuestManager* GetGuestManager() override;
  storage::SpecialStoragePolicy* GetSpecialStoragePolicy() override;
  content::PlatformNotificationService* GetPlatformNotificationService()
      override;
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
  content::ReduceAcceptLanguageControllerDelegate*
  GetReduceAcceptLanguageControllerDelegate() override;
  std::unique_ptr<media::VideoDecodePerfHistory> CreateVideoDecodePerfHistory()
      override;
  content::FileSystemAccessPermissionContext*
  GetFileSystemAccessPermissionContext() override;
  void RecordPrimaryMainFrameNavigation() override;
  content::FederatedIdentityPermissionContextDelegate*
  GetFederatedIdentityPermissionContext() override;
  content::FederatedIdentityApiPermissionContextDelegate*
  GetFederatedIdentityApiPermissionContext() override;
  content::FederatedIdentityAutoReauthnPermissionContextDelegate*
  GetFederatedIdentityAutoReauthnPermissionContext() override;

#if BUILDFLAG(IS_WIN)
  void AckCrashForTracking() override;
#endif

 protected:
  bool IsSignedIn() override;

 private:
  explicit TopChromeWebUIProfileImpl(const OTRProfileID& otr_profile_id);

  base::Time start_time_;
  std::unique_ptr<sync_preferences::PrefServiceSyncable> prefs_;
};

#endif  // CHROME_BROWSER_PROFILES_TOP_CHROME_WEBUI_PROFILE_IMPL_H_
