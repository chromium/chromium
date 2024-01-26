// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
#include "chrome/browser/chromeos/policy/dlp/dlp_files_controller_lacros.h"

#include "base/files/file_path.h"
#include "base/path_service.h"
#include "chrome/browser/chromeos/policy/dlp/dlp_file_destination.h"
#include "chrome/browser/chromeos/policy/dlp/dlp_rules_manager.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/common/chrome_paths.h"
#include "chrome/common/chrome_paths_lacros.h"
#include "chromeos/crosapi/mojom/dlp.mojom.h"
#include "chromeos/lacros/lacros_service.h"

namespace policy {
namespace {
crosapi::mojom::FileAction ConvertFileActionToMojo(dlp::FileAction action) {
  switch (action) {
    case dlp::FileAction::kUnknown:
      return crosapi::mojom::FileAction::kUnknown;
    case dlp::FileAction::kDownload:
      return crosapi::mojom::FileAction::kDownload;
    case dlp::FileAction::kTransfer:
      return crosapi::mojom::FileAction::kTransfer;
    case dlp::FileAction::kUpload:
      return crosapi::mojom::FileAction::kUpload;
    case dlp::FileAction::kCopy:
      return crosapi::mojom::FileAction::kCopy;
    case dlp::FileAction::kMove:
      return crosapi::mojom::FileAction::kMove;
    case dlp::FileAction::kOpen:
      return crosapi::mojom::FileAction::kOpen;
    case dlp::FileAction::kShare:
      return crosapi::mojom::FileAction::kShare;
  }
}
}  // namespace

DlpFilesControllerLacros::DlpFilesControllerLacros(
    const DlpRulesManager& rules_manager)
    : DlpFilesController(rules_manager) {}
DlpFilesControllerLacros::~DlpFilesControllerLacros() = default;

// TODO(b/283764626): Add OneDrive component
std::optional<data_controls::Component>
DlpFilesControllerLacros::MapFilePathToPolicyComponent(
    Profile* profile,
    const base::FilePath& file_path) {
  base::FilePath reference;

  if (chrome::GetAndroidFilesPath(&reference) &&
      reference.IsParent(file_path)) {
    return data_controls::Component::kArc;
  }

  if (chrome::GetRemovableMediaPath(&reference) &&
      reference.IsParent(file_path)) {
    return data_controls::Component::kUsb;
  }

  if (chrome::GetDriveFsMountPointPath(&reference) &&
      reference.IsParent(file_path)) {
    return data_controls::Component::kDrive;
  }

  if (chrome::GetLinuxFilesPath(&reference) &&
      (reference == file_path || reference.IsParent(file_path))) {
    return data_controls::Component::kCrostini;
  }

  return {};
}

bool DlpFilesControllerLacros::IsInLocalFileSystem(
    const base::FilePath& file_path) {
  base::FilePath my_files_folder;
  base::PathService::Get(chrome::DIR_USER_DOCUMENTS, &my_files_folder);
  if (my_files_folder == file_path || my_files_folder.IsParent(file_path)) {
    return true;
  }
  return false;
}

void DlpFilesControllerLacros::ShowDlpBlockedFiles(
    std::optional<uint64_t> task_id,
    std::vector<base::FilePath> blocked_files,
    dlp::FileAction action) {
  chromeos::LacrosService* lacros_service = chromeos::LacrosService::Get();
  if (!lacros_service->IsAvailable<crosapi::mojom::Dlp>()) {
    LOG(WARNING) << "DLP mojo service not available";
    return;
  }
  lacros_service->GetRemote<crosapi::mojom::Dlp>()->ShowBlockedFiles(
      task_id, std::move(blocked_files), ConvertFileActionToMojo(action));
}

}  // namespace policy
