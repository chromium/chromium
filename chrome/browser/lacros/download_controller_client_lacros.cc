// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/lacros/download_controller_client_lacros.h"

#include "base/bind.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chromeos/crosapi/mojom/download_controller.mojom.h"
#include "chromeos/lacros/lacros_service.h"
#include "components/download/public/common/download_item.h"
#include "components/download/public/common/simple_download_manager.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/download_item_utils.h"
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
  download->start_time = item->GetStartTime();
  download->is_dangerous = item->IsDangerous();
  download->has_is_dangerous = true;
  download->is_mixed_content = item->IsMixedContent();
  download->has_is_mixed_content = true;

  return download;
}

}  // namespace

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

void DownloadControllerClientLacros::GetAllDownloads(
    crosapi::mojom::DownloadControllerClient::GetAllDownloadsCallback
        callback) {
  std::vector<crosapi::mojom::DownloadItemPtr> downloads;

  // Aggregate all downloads.
  for (auto* download : download_notifier_.GetAllDownloads())
    downloads.push_back(ConvertToMojoDownloadItem(download));

  // Sort chronologically by start time.
  std::sort(downloads.begin(), downloads.end(),
            [](const auto& a, const auto& b) {
              return a->start_time.value_or(base::Time()) <
                     b->start_time.value_or(base::Time());
            });

  std::move(callback).Run(std::move(downloads));
}

void DownloadControllerClientLacros::Pause(const std::string& download_guid) {
  auto* download = download_notifier_.GetDownloadByGuid(download_guid);
  if (download)
    download->Pause();
}

void DownloadControllerClientLacros::Resume(const std::string& download_guid,
                                            bool user_resume) {
  auto* download = download_notifier_.GetDownloadByGuid(download_guid);
  if (download)
    download->Resume(user_resume);
}

void DownloadControllerClientLacros::Cancel(const std::string& download_guid,
                                            bool user_cancel) {
  auto* download = download_notifier_.GetDownloadByGuid(download_guid);
  if (download)
    download->Cancel(user_cancel);
}

void DownloadControllerClientLacros::SetOpenWhenComplete(
    const std::string& download_guid,
    bool open_when_complete) {
  auto* download = download_notifier_.GetDownloadByGuid(download_guid);
  if (download)
    download->SetOpenWhenComplete(open_when_complete);
}

void DownloadControllerClientLacros::OnProfileAdded(Profile* profile) {
  download_notifier_.AddProfile(profile);
}

void DownloadControllerClientLacros::OnManagerInitialized(
    content::DownloadManager* manager) {
  download::SimpleDownloadManager::DownloadVector downloads;
  manager->GetAllDownloads(&downloads);
  for (auto* download : downloads)
    OnDownloadCreated(manager, download);
}

void DownloadControllerClientLacros::OnManagerGoingDown(
    content::DownloadManager* manager) {
  download::SimpleDownloadManager::DownloadVector downloads;
  manager->GetAllDownloads(&downloads);
  for (auto* download : downloads)
    OnDownloadDestroyed(manager, download);
}

void DownloadControllerClientLacros::OnDownloadCreated(
    content::DownloadManager* manager,
    download::DownloadItem* item) {
  auto* service = chromeos::LacrosService::Get();
  if (!service->IsAvailable<crosapi::mojom::DownloadController>())
    return;

  service->GetRemote<crosapi::mojom::DownloadController>()->OnDownloadCreated(
      ConvertToMojoDownloadItem(item));
}

void DownloadControllerClientLacros::OnDownloadUpdated(
    content::DownloadManager* manager,
    download::DownloadItem* item) {
  auto* service = chromeos::LacrosService::Get();
  if (!service->IsAvailable<crosapi::mojom::DownloadController>())
    return;

  service->GetRemote<crosapi::mojom::DownloadController>()->OnDownloadUpdated(
      ConvertToMojoDownloadItem(item));
}

void DownloadControllerClientLacros::OnDownloadDestroyed(
    content::DownloadManager* manager,
    download::DownloadItem* item) {
  auto* service = chromeos::LacrosService::Get();
  if (!service->IsAvailable<crosapi::mojom::DownloadController>())
    return;

  service->GetRemote<crosapi::mojom::DownloadController>()->OnDownloadDestroyed(
      ConvertToMojoDownloadItem(item));
}
