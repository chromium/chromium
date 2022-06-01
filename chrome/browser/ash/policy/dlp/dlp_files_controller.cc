// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/policy/dlp/dlp_files_controller.h"

#include <string>

#include "base/bind.h"
#include "base/check.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/logging.h"
#include "base/notreached.h"
#include "base/task/task_traits.h"
#include "base/task/thread_pool.h"
#include "chrome/browser/ash/drive/drive_integration_service.h"
#include "chrome/browser/ash/file_manager/path_util.h"
#include "chrome/browser/chromeos/policy/dlp/dlp_rules_manager.h"
#include "chrome/browser/chromeos/policy/dlp/dlp_rules_manager_factory.h"
#include "chromeos/dbus/dlp/dlp_client.h"
#include "chromeos/dbus/dlp/dlp_service.pb.h"
#include "storage/browser/file_system/file_system_url.h"
#include "third_party/abseil-cpp/absl/types/optional.h"
#include "url/gurl.h"

namespace policy {

namespace {

// Maps |file_path| to DlpRulesManager::Component if possible.
absl::optional<DlpRulesManager::Component> MapFilePathtoPolicyComponent(
    Profile* profile,
    const base::FilePath file_path) {
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

DlpFilesController::DlpFilesController() = default;

DlpFilesController::~DlpFilesController() = default;

void DlpFilesController::GetDisallowedTransfers(
    std::vector<storage::FileSystemURL> transferred_files,
    storage::FileSystemURL destination,
    GetDisallowedTransfersCallback result_callback) {
  if (!chromeos::DlpClient::Get() || !chromeos::DlpClient::Get()->IsAlive()) {
    std::move(result_callback).Run(std::vector<storage::FileSystemURL>());
    return;
  }

  dlp::CheckFilesTransferRequest request;
  base::flat_map<std::string, storage::FileSystemURL> filtered_files;
  for (const auto& file : transferred_files) {
    // If the file is in the same file system as the destination, no
    // restrictions should be applied.
    if (!file.IsInSameFileSystem(destination)) {
      auto file_path = file.path().value();
      filtered_files[file_path] = file;
      request.add_files_paths(file_path);
    }
  }
  if (filtered_files.empty()) {
    std::move(result_callback).Run(std::vector<storage::FileSystemURL>());
    return;
  }

  request.set_destination_url(destination.path().value());

  chromeos::DlpClient::Get()->CheckFilesTransfer(
      request,
      base::BindOnce(&DlpFilesController::OnCheckFilesTransferReply,
                     weak_ptr_factory_.GetWeakPtr(), std::move(filtered_files),
                     std::move(result_callback)));
}

void DlpFilesController::GetFilesRestrictedByAnyRule(
    std::vector<storage::FileSystemURL> files,
    GetFilesRestrictedByAnyRuleCallback result_callback) {
  if (!chromeos::DlpClient::Get() || !chromeos::DlpClient::Get()->IsAlive()) {
    std::move(result_callback).Run(std::vector<storage::FileSystemURL>());
    return;
  }
  // TODO(aidazolic): Implement getting the restricted files by calling DLP
  // daemon to check restrictions.
  NOTIMPLEMENTED();
}

// static
std::vector<GURL> DlpFilesController::IsFilesTransferRestricted(
    Profile* profile,
    std::vector<GURL> files_sources,
    std::string destination) {
  DCHECK(profile);
  policy::DlpRulesManager* dlp_rules_manager =
      policy::DlpRulesManagerFactory::GetForPrimaryProfile();
  if (!dlp_rules_manager)
    return std::vector<GURL>();

  auto dst_component =
      MapFilePathtoPolicyComponent(profile, base::FilePath(destination));
  std::vector<GURL> restricted_files_sources;
  for (const auto& src : files_sources) {
    DlpRulesManager::Level level;
    if (dst_component.has_value()) {
      level = dlp_rules_manager->IsRestrictedComponent(
          src, dst_component.value(), DlpRulesManager::Restriction::kFiles,
          nullptr);
    } else {
      // TODO(crbug.com/1286366): Revisit whether passing files paths here make
      // sense.
      level = dlp_rules_manager->IsRestrictedDestination(
          src, GURL(destination), DlpRulesManager::Restriction::kFiles, nullptr,
          nullptr);
    }

    if (level == DlpRulesManager::Level::kBlock)
      restricted_files_sources.push_back(src);
  }
  return restricted_files_sources;
}

void DlpFilesController::OnCheckFilesTransferReply(
    base::flat_map<std::string, storage::FileSystemURL> files_map,
    GetDisallowedTransfersCallback result_callback,
    const dlp::CheckFilesTransferResponse response) {
  if (response.has_error_message()) {
    LOG(ERROR) << "Failed to get check files transfer, error: "
               << response.error_message();
  }

  std::vector<storage::FileSystemURL> restricted_files;
  for (const auto& file : response.files_paths()) {
    DCHECK(files_map.find(file) != files_map.end());
    restricted_files.push_back(files_map.at(file));
  }
  std::move(result_callback).Run(std::move(restricted_files));
}

}  // namespace policy
