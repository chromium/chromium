// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/policy/dlp/dlp_files_controller.h"

#include <sys/types.h>
#include <string>

#include "base/bind.h"
#include "base/check.h"
#include "base/containers/contains.h"
#include "base/containers/flat_map.h"
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
#include "third_party/blink/public/mojom/choosers/file_chooser.mojom.h"
#include "url/gurl.h"

namespace policy {

namespace {

absl::optional<ino_t> GetInodeValue(const base::FilePath& path) {
  struct stat file_stats;
  if (stat(path.value().c_str(), &file_stats) != 0)
    return absl::nullopt;
  return file_stats.st_ino;
}

std::vector<absl::optional<ino_t>> GetFilesInodes(
    const std::vector<storage::FileSystemURL>& files) {
  std::vector<absl::optional<ino_t>> inodes;
  for (const auto& file : files) {
    inodes.push_back(GetInodeValue(file.path()));
  }
  return inodes;
}

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

DlpFilesController::DlpFileMetadata::DlpFileMetadata(
    const std::string& source_url,
    bool is_dlp_restricted)
    : source_url(source_url), is_dlp_restricted(is_dlp_restricted) {}

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
      base::BindOnce(&DlpFilesController::ReturnDisallowedTransfers,
                     weak_ptr_factory_.GetWeakPtr(), std::move(filtered_files),
                     std::move(result_callback)));
}

void DlpFilesController::GetDlpMetadata(
    std::vector<storage::FileSystemURL> files,
    GetDlpMetadataCallback result_callback) {
  if (!chromeos::DlpClient::Get() || !chromeos::DlpClient::Get()->IsAlive()) {
    std::move(result_callback).Run(std::vector<DlpFileMetadata>());
    return;
  }

  std::vector<absl::optional<ino_t>> inodes = GetFilesInodes(files);
  dlp::GetFilesSourcesRequest request;
  for (const auto& inode : inodes) {
    if (inode.has_value()) {
      request.add_files_inodes(inode.value());
    }
  }
  chromeos::DlpClient::Get()->GetFilesSources(
      request, base::BindOnce(&DlpFilesController::ReturnDlpMetadata,
                              weak_ptr_factory_.GetWeakPtr(), std::move(inodes),
                              std::move(result_callback)));
}

void DlpFilesController::FilterDisallowedUploads(
    std::vector<blink::mojom::FileChooserFileInfoPtr> uploaded_files,
    const GURL& destination,
    FilterDisallowedUploadsCallback result_callback) {
  if (uploaded_files.empty()) {
    std::move(result_callback).Run(std::move(uploaded_files));
    return;
  }

  if (!chromeos::DlpClient::Get() || !chromeos::DlpClient::Get()->IsAlive()) {
    std::move(result_callback).Run(std::move(uploaded_files));
    return;
  }

  dlp::CheckFilesTransferRequest request;
  for (const auto& file : uploaded_files) {
    if (file && file->is_native_file())
      request.add_files_paths(file->get_native_file()->file_path.value());
  }
  if (request.files_paths().empty()) {
    std::move(result_callback).Run(std::move(uploaded_files));
    return;
  }

  request.set_destination_url(destination.spec());

  chromeos::DlpClient::Get()->CheckFilesTransfer(
      request,
      base::BindOnce(&DlpFilesController::ReturnAllowedUploads,
                     weak_ptr_factory_.GetWeakPtr(), std::move(uploaded_files),
                     std::move(result_callback)));
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

void DlpFilesController::ReturnDisallowedTransfers(
    base::flat_map<std::string, storage::FileSystemURL> files_map,
    GetDisallowedTransfersCallback result_callback,
    dlp::CheckFilesTransferResponse response) {
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

void DlpFilesController::ReturnAllowedUploads(
    std::vector<blink::mojom::FileChooserFileInfoPtr> uploaded_files,
    FilterDisallowedUploadsCallback result_callback,
    dlp::CheckFilesTransferResponse response) {
  if (response.has_error_message()) {
    LOG(ERROR) << "Failed to get check files transfer, error: "
               << response.error_message();
  }
  std::set<std::string> restricted_files(response.files_paths().begin(),
                                         response.files_paths().end());
  std::vector<blink::mojom::FileChooserFileInfoPtr> filtered_files;
  for (auto& file : uploaded_files) {
    if (file && file->is_native_file() &&
        base::Contains(restricted_files,
                       file->get_native_file()->file_path.value())) {
      continue;
    }
    filtered_files.push_back(std::move(file));
  }
  std::move(result_callback).Run(std::move(filtered_files));
}

void DlpFilesController::ReturnDlpMetadata(
    std::vector<absl::optional<ino_t>> inodes,
    GetDlpMetadataCallback result_callback,
    const dlp::GetFilesSourcesResponse response) {
  if (response.has_error_message()) {
    LOG(ERROR) << "Failed to get files sources, error: "
               << response.error_message();
  }

  policy::DlpRulesManager* dlp_rules_manager =
      policy::DlpRulesManagerFactory::GetForPrimaryProfile();
  if (!dlp_rules_manager) {
    std::move(result_callback).Run(std::vector<DlpFileMetadata>());
    return;
  }

  base::flat_map<ino_t, DlpFileMetadata> metadata_map;
  for (const auto& metadata : response.files_metadata()) {
    DlpRulesManager::Level level = dlp_rules_manager->IsRestrictedByAnyRule(
        GURL(metadata.source_url()), DlpRulesManager::Restriction::kFiles);
    bool is_dlp_restricted = level != DlpRulesManager::Level::kNotSet &&
                             level != DlpRulesManager::Level::kAllow;
    metadata_map.emplace(
        metadata.inode(),
        DlpFileMetadata(metadata.source_url(), is_dlp_restricted));
  }

  std::vector<DlpFileMetadata> result;
  for (const auto& inode : inodes) {
    if (!inode.has_value()) {
      result.emplace_back("", false);
      continue;
    }
    auto metadata_itr = metadata_map.find(inode.value());
    if (metadata_itr == metadata_map.end()) {
      result.emplace_back("", false);
    } else {
      result.emplace_back(metadata_itr->second);
    }
  }

  std::move(result_callback).Run(std::move(result));
}

}  // namespace policy
