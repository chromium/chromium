// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_PROFILES_OFF_THE_RECORD_PROFILE_IMPL_H_
#define CHROME_BROWSER_PROFILES_OFF_THE_RECORD_PROFILE_IMPL_H_

#include <string>

#include "base/macros.h"
#include "base/sequenced_task_runner.h"
#include "build/build_config.h"
#include "chrome/browser/profiles/off_the_record_profile_io_data.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser_list.h"
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
  explicit OffTheRecordProfileImpl(Profile* real_profile);
  ~OffTheRecordProfileImpl() override;
  void Init();

  // Profile implementation.
  std::string GetProfileUserName() const override;
  ProfileType GetProfileType() const override;
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
  PrefService* GetOffTheRecordPrefs() override;
  net::URLRequestContextGetter* GetRequestContext() override;
  base::OnceCallback<net::CookieStore*()> GetExtensionsCookieStoreGetter()
      override;
  scoped_refptr<network::SharedURLLoaderFactory> GetURLLoaderFactory() override;
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
  bool IsSameProfile(Profile* profile) override;
  base::Time GetStartTime() const override;
  base::FilePath last_selected_directory() override;
  void set_last_selected_directory(const base::FilePath& path) override;
  bool WasCreatedByVersionOrLater(const std::string& version) override;
  void SetExitType(ExitType exit_type) override;
  ExitType GetLastSessionExitType() override;

#if defined(OS_CHROMEOS)
  void ChangeAppLocale(const std::string& locale, AppLocaleChangedVia) override;
  void OnLogin() override;
  void InitChromeOSPreferences() override;
#endif  // defined(OS_CHROMEOS)

  GURL GetHomePage() override;

  // content::BrowserContext implementation:
  base::FilePath GetPath() const override;
  base::FilePath GetCachePath() const override;
#if !defined(OS_ANDROID)
  std::unique_ptr<content::ZoomLevelDelegate> CreateZoomLevelDelegate(
      const base::FilePath& partition_path) override;
#endif  // !defined(OS_ANDROID)
  scoped_refptr<base::SequencedTaskRunner> GetIOTaskRunner() override;
  bool IsOffTheRecord() const override;
  content::DownloadManagerDelegate* GetDownloadManagerDelegate() override;
  content::ResourceContext* GetResourceContext() override;
  content::BrowserPluginGuestManager* GetGuestManager() override;
  storage::SpecialStoragePolicy* GetSpecialStoragePolicy() override;
  content::PushMessagingService* GetPushMessagingService() override;
  content::SSLHostStateDelegate* GetSSLHostStateDelegate() override;
  content::PermissionControllerDelegate* GetPermissionControllerDelegate()
      override;
  content::BackgroundFetchDelegate* GetBackgroundFetchDelegate() override;
  content::BackgroundSyncController* GetBackgroundSyncController() override;
  content::BrowsingDataRemoverDelegate* GetBrowsingDataRemoverDelegate()
      override;
  media::VideoDecodePerfHistory* GetVideoDecodePerfHistory() override;

 private:
  void InitIoData();

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

  std::unique_ptr<sync_preferences::PrefServiceSyncable> prefs_;

#if !defined(OS_ANDROID)
  std::unique_ptr<content::HostZoomMap::Subscription> track_zoom_subscription_;
  std::unique_ptr<ChromeZoomLevelPrefs::DefaultZoomLevelSubscription>
      parent_default_zoom_level_subscription_;
#endif  // !defined(OS_ANDROID)
  std::unique_ptr<OffTheRecordProfileIOData::Handle> io_data_;

  // Time we were started.
  base::Time start_time_;

  base::FilePath last_selected_directory_;

  DISALLOW_COPY_AND_ASSIGN(OffTheRecordProfileImpl);
};

#endif  // CHROME_BROWSER_PROFILES_OFF_THE_RECORD_PROFILE_IMPL_H_
