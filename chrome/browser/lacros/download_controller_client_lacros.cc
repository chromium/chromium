// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/lacros/download_controller_client_lacros.h"

#include "base/bind.h"
#include "chrome/browser/browser_process.h"
#include "chromeos/crosapi/mojom/download_controller.mojom.h"
#include "chromeos/lacros/lacros_chrome_service_impl.h"
#include "components/download/public/common/download_item.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/download_item_utils.h"
#include "content/public/browser/download_manager.h"
#include "mojo/public/cpp/bindings/pending_remote.h"

namespace {
crosapi::mojom::DownloadState ConvertDownloadState(
    download::DownloadItem::DownloadState value) {
  switch (value) {
    case download::DownloadItem::IN_PROGRESS:
      return crosapi::mojom::DownloadState::kInProgress;
    case download::DownloadItem::COMPLETE:
      return crosapi::mojom::DownloadState::kComplete;
    case download::DownloadItem::CANCELLED:
      return crosapi::mojom::DownloadState::kCancelled;
    case download::DownloadItem::INTERRUPTED:
      return crosapi::mojom::DownloadState::kInterrupted;
    case download::DownloadItem::MAX_DOWNLOAD_STATE:
      NOTREACHED();
      return crosapi::mojom::DownloadState::kUnknown;
  }
}

crosapi::mojom::DownloadEventPtr BuildDownloadEvent(
    download::DownloadItem* item) {
  auto* profile = Profile::FromBrowserContext(
      content::DownloadItemUtils::GetBrowserContext(item));

  crosapi::mojom::DownloadEventPtr dle = crosapi::mojom::DownloadEvent::New();
  dle->state = ConvertDownloadState(item->GetState());
  dle->target_file_path = item->GetTargetFilePath();
  dle->is_from_incognito_profile = profile->IsIncognitoProfile();
  return dle;
}
}  // namespace

// A wrapper for `base::ScopedObservation` and `DownloadManageObserver` that
// allows us to keep the manager associated with its `OnManagerInitialized()`
// event. This prevents us from having to check every manager and profile when
// a single manager is updated, since `OnManagerInitialized()` does not pass a
// pointer to the relevant `content::DownloadManager`.
class DownloadControllerClientLacros::ObservableDownloadManager
    : public content::DownloadManager::Observer,
      public download::DownloadItem::Observer {
 public:
  ObservableDownloadManager(DownloadControllerClientLacros* controller_client,
                            content::DownloadManager* manager)
      : controller_client_(controller_client), manager_(manager) {
    download_manager_observer_.Observe(manager);
    if (manager->IsManagerInitialized())
      OnManagerInitialized();
  }

  ~ObservableDownloadManager() override = default;

 private:
  // content::DownloadManager::Observer:
  void OnManagerInitialized() override {
    download::SimpleDownloadManager::DownloadVector downloads;
    manager_->GetAllDownloads(&downloads);

    for (auto* download : downloads) {
      download_item_observer_.AddObservation(download);
      controller_client_->OnDownloadCreated(download);
    }
  }

  void ManagerGoingDown(content::DownloadManager* manager) override {
    download_manager_observer_.Reset();
    // Manually call the destroyed event for each download, because this
    // `ObservableDownloadManager` will be destroyed before we receive them.
    download::SimpleDownloadManager::DownloadVector downloads;
    manager->GetAllDownloads(&downloads);

    for (auto* download : downloads)
      OnDownloadDestroyed(download);

    controller_client_->OnManagerGoingDown(this);
  }

  void OnDownloadCreated(content::DownloadManager* manager,
                         download::DownloadItem* item) override {
    if (!manager->IsManagerInitialized())
      return;
    download_item_observer_.AddObservation(item);
    controller_client_->OnDownloadCreated(item);
  }

  // download::DownloadItem::Observer:
  void OnDownloadUpdated(download::DownloadItem* item) override {
    controller_client_->OnDownloadUpdated(item);
  }

  void OnDownloadDestroyed(download::DownloadItem* item) override {
    if (download_item_observer_.IsObservingSource(item))
      download_item_observer_.RemoveObservation(item);
    controller_client_->OnDownloadDestroyed(item);
  }

  DownloadControllerClientLacros* const controller_client_;

  content::DownloadManager* const manager_;

  base::ScopedMultiSourceObservation<download::DownloadItem,
                                     download::DownloadItem::Observer>
      download_item_observer_{this};

  base::ScopedObservation<content::DownloadManager,
                          content::DownloadManager::Observer>
      download_manager_observer_{this};
};

DownloadControllerClientLacros::DownloadControllerClientLacros() {
  g_browser_process->profile_manager()->AddObserver(this);
  auto profiles = g_browser_process->profile_manager()->GetLoadedProfiles();
  for (auto* profile : profiles)
    OnProfileAdded(profile);
}

DownloadControllerClientLacros::~DownloadControllerClientLacros() {
  if (g_browser_process && g_browser_process->profile_manager())
    g_browser_process->profile_manager()->RemoveObserver(this);
}

void DownloadControllerClientLacros::OnProfileAdded(Profile* profile) {
  profile_observer_.AddObservation(profile);
  auto* manager = profile->GetDownloadManager();
  observable_download_managers_.emplace(
      std::make_unique<ObservableDownloadManager>(this, manager));
}

void DownloadControllerClientLacros::OnOffTheRecordProfileCreated(
    Profile* off_the_record) {
  OnProfileAdded(off_the_record);
}

void DownloadControllerClientLacros::OnProfileWillBeDestroyed(
    Profile* profile) {
  profile_observer_.RemoveObservation(profile);
}

void DownloadControllerClientLacros::OnManagerGoingDown(
    ObservableDownloadManager* observable_manager) {
  auto it = observable_download_managers_.find(observable_manager);
  DCHECK_NE(it->get(), observable_download_managers_.end()->get());
  observable_download_managers_.erase(it);
}

void DownloadControllerClientLacros::OnDownloadCreated(
    download::DownloadItem* item) {
  auto* service = chromeos::LacrosService::Get();
  if (!service->IsAvailable<crosapi::mojom::DownloadController>())
    return;

  service->GetRemote<crosapi::mojom::DownloadController>()->OnDownloadCreated(
      BuildDownloadEvent(item));
}

void DownloadControllerClientLacros::OnDownloadUpdated(
    download::DownloadItem* item) {
  auto* service = chromeos::LacrosService::Get();
  if (!service->IsAvailable<crosapi::mojom::DownloadController>())
    return;

  service->GetRemote<crosapi::mojom::DownloadController>()->OnDownloadUpdated(
      BuildDownloadEvent(item));
}

void DownloadControllerClientLacros::OnDownloadDestroyed(
    download::DownloadItem* item) {
  auto* service = chromeos::LacrosService::Get();
  if (!service->IsAvailable<crosapi::mojom::DownloadController>())
    return;

  service->GetRemote<crosapi::mojom::DownloadController>()->OnDownloadDestroyed(
      BuildDownloadEvent(item));
}
