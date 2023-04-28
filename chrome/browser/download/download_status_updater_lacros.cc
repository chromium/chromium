// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/download/download_status_updater.h"

#include "chromeos/crosapi/mojom/download_controller.mojom.h"
#include "chromeos/crosapi/mojom/download_status_updater.mojom.h"
#include "chromeos/lacros/lacros_service.h"
#include "components/download/public/common/download_item_utils.h"

namespace {

// Helpers ---------------------------------------------------------------------

// TODO(http://b/279831939): Add more renderable metadata.
crosapi::mojom::DownloadStatusPtr ConvertToMojoDownloadStatus(
    const download::DownloadItem* download) {
  auto status = crosapi::mojom::DownloadStatus::New();
  status->guid = download->GetGuid();
  status->state = download::download_item_utils::ConvertToMojoDownloadState(
      download->GetState());
  return status;
}

}  // namespace

// DownloadStatusUpdater -------------------------------------------------------

void DownloadStatusUpdater::UpdateAppIconDownloadProgress(
    download::DownloadItem* download) {
  using DownloadStatusUpdater = crosapi::mojom::DownloadStatusUpdater;
  if (auto* service = chromeos::LacrosService::Get();
      service && service->IsAvailable<DownloadStatusUpdater>()) {
    service->GetRemote<DownloadStatusUpdater>()->Update(
        ConvertToMojoDownloadStatus(download));
  }
}
