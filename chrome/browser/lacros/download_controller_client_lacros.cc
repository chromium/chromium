// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/lacros/download_controller_client_lacros.h"

#include "base/bind.h"
#include "chrome/browser/browser_process.h"
#include "chromeos/crosapi/mojom/download_controller.mojom.h"
#include "chromeos/lacros/lacros_service.h"
#include "components/download/public/common/download_item.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/download_item_utils.h"
#include "content/public/browser/download_manager.h"
#include "mojo/public/cpp/bindings/pending_remote.h"

namespace {

crosapi::mojom::DownloadState ConvertMojoDownloadState(
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

crosapi::mojom::DownloadItemPtr ConvertToMojoDownloadItem(
    download::DownloadItem* item) {
  auto* profile = Profile::FromBrowserContext(
      content::DownloadItemUtils::GetBrowserContext(item));

  auto download = crosapi::mojom::DownloadItem::New();
  download->guid = item->GetGuid();
  download->state = ConvertMojoDownloadState(item->GetState());
  download->full_path = item->GetFullPath();
  download->target_file_path = item->GetTargetFilePath();
  download->is_from_incognito_profile = profile->IsIncognitoProfile();
  download->is_paused = item->IsPaused();
  download->has_is_paused = true;
  download->open_when_complete = item->GetOpenWhenComplete();
  download->has_open_when_complete = true;
  download->received_bytes = item->GetReceivedBytes();
  download->has_received_bytes = true;
  download->total_bytes = item->GetTotalBytes();
  download->has_total_bytes = true;
  return download;
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

  // Pauses the download associated with the specified `download_guid`.
  void Pause(const std::string& download_guid) {
    download::DownloadItem* download = GetDownloadByGuid(download_guid);
    if (download)
      download->Pause();
  }

  // Resumes the download associated with the specified `download_guid`. If
  // `user_resume` is `true`, it signifies that this invocation was triggered by
  // an explicit user action.
  void Resume(const std::string& download_guid, bool user_resume) {
    download::DownloadItem* download = GetDownloadByGuid(download_guid);
    if (download)
      download->Resume(user_resume);
  }

  // Cancels the download associated with the specified `download_guid`. If
  // `user_cancel` is `true`, it signifies that this invocation was triggered by
  // an explicit user action.
  void Cancel(const std::string& download_guid, bool user_cancel) {
    download::DownloadItem* download = GetDownloadByGuid(download_guid);
    if (download)
      download->Cancel(user_cancel);
  }

  // Marks the download associated with the specified `download_guid` to be
  // `open_when_complete`.
  void SetOpenWhenComplete(const std::string& download_guid,
                           bool open_when_complete) {
    download::DownloadItem* download = GetDownloadByGuid(download_guid);
    if (download)
      download->SetOpenWhenComplete(open_when_complete);
  }

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

  download::DownloadItem* GetDownloadByGuid(const std::string& guid) {
    download::SimpleDownloadManager::DownloadVector downloads;
    manager_->GetAllDownloads(&downloads);
    for (auto* download : downloads) {
      if (download->GetGuid() == guid)
        return download;
    }
    return nullptr;
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

  auto* service = chromeos::LacrosService::Get();
  if (!service->IsAvailable<crosapi::mojom::DownloadController>())
    return;

  int remote_version =
      service->GetInterfaceVersion(crosapi::mojom::DownloadController::Uuid_);
  if (remote_version < 0 ||
      static_cast<uint32_t>(remote_version) <
          crosapi::mojom::DownloadController::kBindClientMinVersion) {
    return;
  }

  service->GetRemote<crosapi::mojom::DownloadController>()->BindClient(
      client_receiver_.BindNewPipeAndPassRemoteWithVersion());
}

DownloadControllerClientLacros::~DownloadControllerClientLacros() {
  if (g_browser_process && g_browser_process->profile_manager())
    g_browser_process->profile_manager()->RemoveObserver(this);
}

void DownloadControllerClientLacros::Pause(const std::string& download_guid) {
  for (auto& observable_download_manager : observable_download_managers_)
    observable_download_manager->Pause(download_guid);
}

void DownloadControllerClientLacros::Resume(const std::string& download_guid,
                                            bool user_resume) {
  for (auto& observable_download_manager : observable_download_managers_)
    observable_download_manager->Resume(download_guid, user_resume);
}

void DownloadControllerClientLacros::Cancel(const std::string& download_guid,
                                            bool user_cancel) {
  for (auto& observable_download_manager : observable_download_managers_)
    observable_download_manager->Cancel(download_guid, user_cancel);
}

void DownloadControllerClientLacros::SetOpenWhenComplete(
    const std::string& download_guid,
    bool open_when_complete) {
  for (auto& observable_download_manager : observable_download_managers_) {
    observable_download_manager->SetOpenWhenComplete(download_guid,
                                                     open_when_complete);
  }
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
      ConvertToMojoDownloadItem(item));
}

void DownloadControllerClientLacros::OnDownloadUpdated(
    download::DownloadItem* item) {
  auto* service = chromeos::LacrosService::Get();
  if (!service->IsAvailable<crosapi::mojom::DownloadController>())
    return;

  service->GetRemote<crosapi::mojom::DownloadController>()->OnDownloadUpdated(
      ConvertToMojoDownloadItem(item));
}

void DownloadControllerClientLacros::OnDownloadDestroyed(
    download::DownloadItem* item) {
  auto* service = chromeos::LacrosService::Get();
  if (!service->IsAvailable<crosapi::mojom::DownloadController>())
    return;

  service->GetRemote<crosapi::mojom::DownloadController>()->OnDownloadDestroyed(
      ConvertToMojoDownloadItem(item));
}
