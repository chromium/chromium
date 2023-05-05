// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/download/download_status_updater.h"

#include "base/containers/contains.h"
#include "chrome/browser/download/bubble/download_bubble_prefs.h"
#include "chrome/browser/download/download_commands.h"
#include "chrome/browser/download/download_item_model.h"
#include "chrome/browser/download/download_ui_model.h"
#include "chrome/browser/profiles/profile.h"
#include "chromeos/crosapi/mojom/download_controller.mojom.h"
#include "chromeos/crosapi/mojom/download_status_updater.mojom.h"
#include "chromeos/lacros/lacros_service.h"
#include "components/download/public/common/download_item_utils.h"
#include "content/public/browser/download_item_utils.h"

namespace {

// Helpers ---------------------------------------------------------------------

bool IsCommandEnabled(DownloadItemModel& model,
                      DownloadCommands::Command command) {
  // To support other commands, we may need to update checks below to also
  // inspect `BubbleUIInfo` subpage buttons.
  CHECK(command == DownloadCommands::CANCEL ||
        command == DownloadCommands::PAUSE ||
        command == DownloadCommands::RESUME);

  const bool is_download_bubble_v2_enabled =
      download::IsDownloadBubbleV2Enabled(Profile::FromBrowserContext(
          content::DownloadItemUtils::GetBrowserContext(
              model.GetDownloadItem())));

  // `BubbleUIInfo` contains at most one of either `CANCEL`, `PAUSE`, or
  // `RESUME` when download bubble v2 is disabled, despite the fact that a
  // download may be simultaneously cancellable and pausable/resumable. For
  // this reason, do not use `BubbleUIInfo`-based determination of command
  // enablement when download bubble v2 is disabled.
  if (!is_download_bubble_v2_enabled) {
    DownloadCommands commands(model.GetWeakPtr());
    return model.IsCommandEnabled(&commands, command);
  }

  const DownloadUIModel::BubbleUIInfo info =
      model.GetBubbleUIInfo(/*is_download_bubble_v2_enabled=*/true);

  // A command is enabled if `BubbleUIInfo` contains a quick action for it. This
  // is preferred over non-`BubbleUIInfo`-based determination of command
  // enablement as it takes more signals into account, e.g. if the download has
  // been marked dangerous.
  return base::Contains(info.quick_actions, command,
                        &DownloadUIModel::BubbleUIInfo::QuickAction::command);
}

crosapi::mojom::DownloadStatusPtr ConvertToMojoDownloadStatus(
    download::DownloadItem* download) {
  DownloadItemModel model(download);
  auto status = crosapi::mojom::DownloadStatus::New();
  status->guid = download->GetGuid();
  status->state = download::download_item_utils::ConvertToMojoDownloadState(
      download->GetState());
  status->received_bytes = download->GetReceivedBytes();
  status->total_bytes = download->GetTotalBytes();
  status->target_file_path = download->GetTargetFilePath();
  status->cancellable = IsCommandEnabled(model, DownloadCommands::CANCEL);
  status->pausable = IsCommandEnabled(model, DownloadCommands::PAUSE);
  status->resumable = IsCommandEnabled(model, DownloadCommands::RESUME);
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
