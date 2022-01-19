// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/policy/dlp/dlp_files_controller.h"

#include "base/bind.h"
#include "base/check.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/logging.h"
#include "base/task/task_traits.h"
#include "base/task/thread_pool.h"
#include "chrome/browser/ash/drive/drive_integration_service.h"
#include "chrome/browser/ash/file_manager/path_util.h"
#include "chrome/browser/chromeos/policy/dlp/dlp_rules_manager.h"
#include "chromeos/dbus/dlp/dlp_client.h"
#include "storage/browser/file_system/file_system_url.h"
#include "third_party/abseil-cpp/absl/types/optional.h"
#include "url/gurl.h"

namespace policy {

namespace {

absl::optional<ino_t> GetInodeValue(const base::FilePath& path) {
  struct stat file_stats;
  if (stat(path.value().c_str(), &file_stats) != 0)
    return absl::nullopt;
  return file_stats.st_ino;
}

base::flat_map<ino_t, storage::FileSystemURL> GetFilesInodes(
    std::vector<storage::FileSystemURL> transferred_files) {
  base::flat_map<ino_t, storage::FileSystemURL> files_map;
  dlp::GetFilesSourcesRequest request;
  for (const auto& file : transferred_files) {
    absl::optional<ino_t> inode = GetInodeValue(file.path());
    if (inode.has_value())
      files_map[inode.value()] = file;
  }
  return files_map;
}

// Maps |file_path| to DlpRulesManager::Component if possible.
absl::optional<DlpRulesManager::Component> MapFilePathtoPolicyComponent(
    Profile* profile,
    const storage::FileSystemURL& file_url) {
  auto file_path = file_url.path();

  if (base::FilePath(file_manager::util::kAndroidFilesPath)
          .IsParent(file_path)) {
    return DlpRulesManager::Component::kArc;
  }

  if (base::FilePath(file_manager::util::kRemovableMediaPath)
          .IsParent(file_path)) {
    return DlpRulesManager::Component::kUsb;
  }

  drive::DriveIntegrationService* integration_service =
      drive::DriveIntegrationServiceFactory::FindForProfile(profile);
  if (integration_service && integration_service->is_enabled() &&
      integration_service->GetMountPointPath().IsParent(file_path)) {
    return DlpRulesManager::Component::kDrive;
  }

  base::FilePath linux_files =
      file_manager::util::GetCrostiniMountDirectory(profile);
  if (linux_files == file_path || linux_files.IsParent(file_path)) {
    return DlpRulesManager::Component::kCrostini;
  }

  return {};
}

}  // namespace

DlpFilesController::DlpFilesController(Profile* profile,
                                       DlpRulesManager* dlp_rules_manager)
    : profile_(profile), dlp_rules_manager_(dlp_rules_manager) {
  DCHECK(dlp_rules_manager);
  DCHECK(profile);
}

DlpFilesController::~DlpFilesController() = default;

void DlpFilesController::GetDisallowedTransfers(
    std::vector<storage::FileSystemURL> transferred_files,
    storage::FileSystemURL destination,
    GetDisallowedTransfersCallback result_callback) {
  if (!chromeos::DlpClient::Get() || !chromeos::DlpClient::Get()->IsAlive()) {
    std::move(result_callback).Run(std::vector<storage::FileSystemURL>());
    return;
  }

  std::vector<storage::FileSystemURL> filtered_files;
  for (const auto& file : transferred_files) {
    // If the file is in the same file system as the destination, no
    // restrictions should be applied.
    if (!file.IsInSameFileSystem(destination))
      filtered_files.push_back(file);
  }

  base::ThreadPool::PostTaskAndReplyWithResult(
      FROM_HERE, {base::MayBlock(), base::TaskPriority::USER_VISIBLE},
      base::BindOnce(&GetFilesInodes, std::move(filtered_files)),
      base::BindOnce(&DlpFilesController::GetFilesSources,
                     weak_ptr_factory_.GetWeakPtr(), std::move(destination),
                     std::move(result_callback)));
}

void DlpFilesController::GetFilesSources(
    storage::FileSystemURL destination,
    GetDisallowedTransfersCallback result_callback,
    base::flat_map<ino_t, storage::FileSystemURL> files_map) {
  dlp::GetFilesSourcesRequest request;
  for (const auto& file_inode_url : files_map) {
    request.add_files_inodes(file_inode_url.first);
  }

  chromeos::DlpClient::Get()->GetFilesSources(
      request,
      base::BindOnce(&DlpFilesController::OnGetFilesSourcesReply,
                     weak_ptr_factory_.GetWeakPtr(), std::move(files_map),
                     std::move(destination), std::move(result_callback)));
}

void DlpFilesController::OnGetFilesSourcesReply(
    base::flat_map<ino_t, storage::FileSystemURL> files_map,
    storage::FileSystemURL destination,
    GetDisallowedTransfersCallback result_callback,
    const dlp::GetFilesSourcesResponse response) {
  if (response.has_error_message()) {
    LOG(ERROR) << "Failed to get files sources, error: "
               << response.error_message();
  }

  auto dst_component = MapFilePathtoPolicyComponent(profile_, destination);

  std::vector<storage::FileSystemURL> restricted_files;
  for (const auto& file : response.files_metadata()) {
    DlpRulesManager::Level level;
    if (dst_component.has_value()) {
      level = dlp_rules_manager_->IsRestrictedComponent(
          GURL(file.source_url()), dst_component.value(),
          DlpRulesManager::Restriction::kFiles, nullptr);
    } else {
      // TODO(crbug.com/1286366): Revisit whether passing files paths here make
      // sense.
      level = dlp_rules_manager_->IsRestrictedDestination(
          GURL(file.source_url()), destination.ToGURL(),
          DlpRulesManager::Restriction::kFiles, nullptr, nullptr);
    }

    if (level == DlpRulesManager::Level::kBlock) {
      auto blocked_file_itr = files_map.find(file.inode());
      DCHECK(blocked_file_itr != files_map.end());
      restricted_files.push_back(blocked_file_itr->second);
    }
  }
  std::move(result_callback).Run(std::move(restricted_files));
}

}  // namespace policy
