// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/download/download_manager_utils.h"

#include "base/bind.h"
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
#include "content/public/browser/download_request_utils.h"
#include "content/public/browser/system_connector.h"

#if defined(OS_ANDROID)
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

// Ignores origin security check. DownloadManagerImpl will provide its own
// implementation when InProgressDownloadManager object is passed to it.
bool IgnoreOriginSecurityCheck(const GURL& url) {
  return true;
}

// Some ChromeOS browser tests doesn't initialize DownloadManager when profile
// is created, and cause the download request to fail. This method helps us
// ensure that the DownloadManager will be created after profile creation.
void GetDownloadManagerOnProfileCreation(Profile* profile) {
  content::DownloadManager* manager =
      content::BrowserContext::GetDownloadManager(profile);
  DCHECK(manager);
}

}  // namespace

// static
download::InProgressDownloadManager*
DownloadManagerUtils::RetrieveInProgressDownloadManager(Profile* profile) {
  ProfileKey* key = profile->GetProfileKey();
  GetInProgressDownloadManager(key);
  auto& map = GetInProgressManagerMap();
  return map[key].release();
}

// static
void DownloadManagerUtils::InitializeSimpleDownloadManager(ProfileKey* key) {
#if defined(OS_ANDROID)
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
    service_manager::Connector* connector = content::GetSystemConnector();
    auto in_progress_manager =
        std::make_unique<download::InProgressDownloadManager>(
            nullptr, key->IsOffTheRecord() ? base::FilePath() : key->GetPath(),
            key->IsOffTheRecord() ? nullptr : key->GetProtoDatabaseProvider(),
            base::BindRepeating(&IgnoreOriginSecurityCheck),
            base::BindRepeating(&content::DownloadRequestUtils::IsURLSafe),
            connector);
    download::SimpleDownloadManagerCoordinator* coordinator =
        SimpleDownloadManagerCoordinatorFactory::GetForKey(key);
    coordinator->SetSimpleDownloadManager(
        in_progress_manager.get(), false /* manages_all_history_downloads */);
    scoped_refptr<network::SharedURLLoaderFactory> factory =
        SystemNetworkContextManager::GetInstance()->GetSharedURLLoaderFactory();
    in_progress_manager->set_url_loader_factory(std::move(factory));
#if defined(OS_ANDROID)
    in_progress_manager->set_download_start_observer(
        DownloadControllerBase::Get());
    in_progress_manager->set_intermediate_path_cb(
        base::BindRepeating(&DownloadTargetDeterminer::GetCrDownloadPath));
    base::FilePath download_dir;
    base::android::GetDownloadsDirectory(&download_dir);
    in_progress_manager->set_default_download_dir(download_dir);
#endif  // defined(OS_ANDROID)
    auto* download_provider =
        DownloadOfflineContentProviderFactory::GetForKey(key);
    download_provider->SetSimpleDownloadManagerCoordinator(coordinator);
    map[key] = std::move(in_progress_manager);
  }
  return map[key].get();
}
