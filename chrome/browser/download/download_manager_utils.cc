// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/download/download_manager_utils.h"

#include <utility>

#include "base/functional/bind.h"
#include "base/no_destructor.h"
#include "build/build_config.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/download/download_offline_content_provider.h"
#include "chrome/browser/download/download_offline_content_provider_factory.h"
#include "chrome/browser/download/simple_download_manager_coordinator_factory.h"
#include "chrome/browser/net/system_network_context_manager.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_key.h"
#include "chrome/browser/transition_manager/full_browser_transition_manager.h"
#include "components/download/public/common/download_features.h"
#include "components/download/public/common/in_progress_download_manager.h"
#include "components/download/public/common/simple_download_manager_coordinator.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/device_service.h"
#include "content/public/browser/download_request_utils.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "services/device/public/mojom/wake_lock_provider.mojom.h"

#if BUILDFLAG(IS_ANDROID)
#include "base/android/path_utils.h"
#include "chrome/browser/download/android/download_controller.h"
#include "chrome/browser/download/android/download_manager_service.h"
#include "chrome/browser/download/download_target_determiner.h"
#endif

namespace {

// A map for owning InProgressDownloadManagers before DownloadManagerImpl gets
// created.
using InProgressManagerMap =
    std::map<ProfileKey*, std::unique_ptr<download::InProgressDownloadManager>>;

InProgressManagerMap& GetInProgressManagerMap() {
  static base::NoDestructor<InProgressManagerMap> map;
  return *map;
}

// Returns a callback to be invoked during `RetrieveInProgressDownloadManager()`
// to provide an opportunity to cache a pointer to the in progress download
// manager being released.
base::RepeatingCallback<void(download::InProgressDownloadManager*)>&
GetRetrieveInProgressDownloadManagerCallback() {
  static base::NoDestructor<
      base::RepeatingCallback<void(download::InProgressDownloadManager*)>>
      callback;
  return *callback;
}

// Ignores origin security check. DownloadManagerImpl will provide its own
// implementation when InProgressDownloadManager object is passed to it.
bool IgnoreOriginSecurityCheck(const GURL& url) {
  return true;
}

// Some ChromeOS browser tests doesn't initialize DownloadManager when profile
// is created, and cause the download request to fail. This method helps us
// ensure that the DownloadManager will be created after profile creation.
void GetDownloadManagerOnProfileCreation(Profile* profile) {
  content::DownloadManager* manager = profile->GetDownloadManager();
  DCHECK(manager);
}

void BindWakeLockProvider(
    mojo::PendingReceiver<device::mojom::WakeLockProvider> receiver) {
  content::GetDeviceService().BindWakeLockProvider(std::move(receiver));
}

}  // namespace

// static
std::unique_ptr<download::InProgressDownloadManager>
DownloadManagerUtils::RetrieveInProgressDownloadManager(Profile* profile) {
  ProfileKey* key = profile->GetProfileKey();
  GetInProgressDownloadManager(key);
  auto& map = GetInProgressManagerMap();
  if (GetRetrieveInProgressDownloadManagerCallback())
    GetRetrieveInProgressDownloadManagerCallback().Run(map[key].get());
  return std::move(map[key]);
}

// static
void DownloadManagerUtils::InitializeSimpleDownloadManager(ProfileKey* key) {
#if BUILDFLAG(IS_ANDROID)
  if (!g_browser_process) {
    GetInProgressDownloadManager(key);
    return;
  }
#endif
  if (base::FeatureList::IsEnabled(
          download::features::
              kUseInProgressDownloadManagerForDownloadService)) {
    GetInProgressDownloadManager(key);
  } else {
    FullBrowserTransitionManager::Get()->RegisterCallbackOnProfileCreation(
        key, base::BindOnce(&GetDownloadManagerOnProfileCreation));
  }
}

// static
download::InProgressDownloadManager*
DownloadManagerUtils::GetInProgressDownloadManager(ProfileKey* key) {
  auto& map = GetInProgressManagerMap();
  auto it = map.find(key);
  // Create the InProgressDownloadManager if it hasn't been created yet.
  if (it == map.end()) {
    auto in_progress_manager =
        std::make_unique<download::InProgressDownloadManager>(
            nullptr, key->IsOffTheRecord() ? base::FilePath() : key->GetPath(),
            key->IsOffTheRecord() ? nullptr : key->GetProtoDatabaseProvider(),
            base::BindRepeating(&IgnoreOriginSecurityCheck),
            base::BindRepeating(&content::DownloadRequestUtils::IsURLSafe),
            base::BindRepeating(&BindWakeLockProvider));
    download::SimpleDownloadManagerCoordinator* coordinator =
        SimpleDownloadManagerCoordinatorFactory::GetForKey(key);
    coordinator->SetSimpleDownloadManager(
        in_progress_manager.get(), false /* manages_all_history_downloads */);
    scoped_refptr<network::SharedURLLoaderFactory> factory =
        SystemNetworkContextManager::GetInstance()->GetSharedURLLoaderFactory();
    in_progress_manager->set_url_loader_factory(std::move(factory));
#if BUILDFLAG(IS_ANDROID)
    in_progress_manager->set_download_start_observer(
        DownloadControllerBase::Get());
    in_progress_manager->set_intermediate_path_cb(
        base::BindRepeating(&DownloadTargetDeterminer::GetCrDownloadPath));
    base::FilePath download_dir;
    base::android::GetDownloadsDirectory(&download_dir);
    in_progress_manager->set_default_download_dir(download_dir);
#endif  // BUILDFLAG(IS_ANDROID)
    auto* download_provider =
        DownloadOfflineContentProviderFactory::GetForKey(key);
    download_provider->SetSimpleDownloadManagerCoordinator(coordinator);
    map[key] = std::move(in_progress_manager);
  }
  return map[key].get();
}

// static
void DownloadManagerUtils::
    SetRetrieveInProgressDownloadManagerCallbackForTesting(
        base::RepeatingCallback<void(download::InProgressDownloadManager*)>
            callback) {
  GetRetrieveInProgressDownloadManagerCallback() = callback;
}
