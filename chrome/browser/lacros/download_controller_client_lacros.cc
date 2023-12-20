// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/lacros/download_controller_client_lacros.h"

#include "base/functional/bind.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chromeos/crosapi/mojom/download_controller.mojom.h"
#include "chromeos/lacros/lacros_service.h"
#include "components/download/public/common/download_item.h"
#include "components/download/public/common/download_item_utils.h"
#include "components/download/public/common/simple_download_manager.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/download_item_utils.h"
#include "mojo/public/cpp/bindings/pending_remote.h"

namespace {

crosapi::mojom::DownloadItemPtr ConvertToMojoDownloadItem(
    download::DownloadItem* item) {
  return download::download_item_utils::ConvertToMojoDownloadItem(
      item, /*is_from_incognito_profile=*/Profile::FromBrowserContext(
                content::DownloadItemUtils::GetBrowserContext(item))
                ->IsIncognitoProfile());
}

}  // namespace

DownloadControllerClientLacros::DownloadControllerClientLacros() {
  profile_manager_observation_.Observe(g_browser_process->profile_manager());
  auto profiles = g_browser_process->profile_manager()->GetLoadedProfiles();
  for (auto* profile : profiles)
    OnProfileAdded(profile);

  auto* service = chromeos::LacrosService::Get();
  if (!service->IsAvailable<crosapi::mojom::DownloadController>())
    return;

  int remote_version =
      service->GetInterfaceVersion<crosapi::mojom::DownloadController>();
  if (remote_version < 0 ||
      static_cast<uint32_t>(remote_version) <
          crosapi::mojom::DownloadController::kBindClientMinVersion) {
    return;
  }

  service->GetRemote<crosapi::mojom::DownloadController>()->BindClient(
      client_receiver_.BindNewPipeAndPassRemoteWithVersion());
}

DownloadControllerClientLacros::~DownloadControllerClientLacros() = default;

void DownloadControllerClientLacros::GetAllDownloads(
    crosapi::mojom::DownloadControllerClient::GetAllDownloadsCallback
        callback) {
  std::vector<crosapi::mojom::DownloadItemPtr> downloads;

  // Aggregate all downloads.
  for (download::DownloadItem* download :
       download_notifier_.GetAllDownloads()) {
    downloads.push_back(ConvertToMojoDownloadItem(download));
  }

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

void DownloadControllerClientLacros::OnProfileManagerDestroying() {
  profile_manager_observation_.Reset();
}

void DownloadControllerClientLacros::OnManagerInitialized(
    content::DownloadManager* manager) {
  download::SimpleDownloadManager::DownloadVector downloads;
  manager->GetAllDownloads(&downloads);
  for (download::DownloadItem* download : downloads) {
    OnDownloadCreated(manager, download);
  }
}

void DownloadControllerClientLacros::OnManagerGoingDown(
    content::DownloadManager* manager) {
  download::SimpleDownloadManager::DownloadVector downloads;
  manager->GetAllDownloads(&downloads);
  for (download::DownloadItem* download : downloads) {
    OnDownloadDestroyed(manager, download);
  }
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
