// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/file_system_access/cloud_identifier/cloud_identifier_util_ash.h"

#include <memory>
#include <optional>

#include "base/files/file.h"
#include "base/files/file_path.h"
#include "base/strings/string_util.h"
#include "chrome/browser/ash/crosapi/crosapi_util.h"
#include "chrome/browser/ash/drive/drive_integration_service.h"
#include "chrome/browser/ash/drive/file_system_util.h"
#include "chrome/browser/ash/file_manager/fileapi_util.h"
#include "chrome/browser/ash/file_system_provider/mount_path_util.h"
#include "chrome/browser/ash/file_system_provider/provided_file_system_interface.h"
#include "chrome/browser/ash/profiles/profile_helper.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chromeos/ash/components/drivefs/drivefs_util.h"
#include "chromeos/ash/components/drivefs/mojom/drivefs.mojom.h"
#include "chromeos/crosapi/mojom/file_system_access_cloud_identifier.mojom.h"
#include "components/drive/file_errors.h"
#include "content/public/browser/browser_thread.h"
#include "storage/browser/file_system/file_system_context.h"

namespace {
constexpr char kDriveFsProviderName[] = "drive.google.com";

// Files/directories which have not yet been uploaded have not received a proper
// ID yet and will have a "local" ID instead. This prefix is defined upstream at
// http://google3/apps/drive/cello/common/item_util.h
constexpr char kDriveFsLocalPrefix[] = "local-";

bool MatchesExpectedDriveFsType(crosapi::mojom::HandleType expected_handle_type,
                                drivefs::mojom::FileMetadata::Type type) {
  switch (expected_handle_type) {
    case crosapi::mojom::HandleType::kUnknown:
      return false;
    case crosapi::mojom::HandleType::kFile:
      return drivefs::IsAFile(type);
    case crosapi::mojom::HandleType::kDirectory:
      return drivefs::IsADirectory(type);
  }
}

bool MatchesExpectedProvidedFsType(
    crosapi::mojom::HandleType expected_handle_type,
    bool is_directory) {
  switch (expected_handle_type) {
    case crosapi::mojom::HandleType::kUnknown:
      return false;
    case crosapi::mojom::HandleType::kFile:
      return !is_directory;
    case crosapi::mojom::HandleType::kDirectory:
      return is_directory;
  }
}

void DidGetDriveFSMetadata(crosapi::mojom::HandleType expected_handle_type,
                           crosapi::FileSystemAccessCloudIdentifierProviderAsh::
                               GetCloudIdentifierCallback callback,
                           drive::FileError error,
                           drivefs::mojom::FileMetadataPtr metadata) {
  if (error != drive::FILE_ERROR_OK || metadata.is_null() ||
      !metadata->item_id.has_value() ||
      base::StartsWith(metadata->item_id.value(), kDriveFsLocalPrefix) ||
      !MatchesExpectedDriveFsType(expected_handle_type, metadata->type)) {
    std::move(callback).Run(nullptr);
    return;
  }

  const std::string& item_id = metadata->item_id.value();
  crosapi::mojom::FileSystemAccessCloudIdentifierPtr cloud_identifier =
      crosapi::mojom::FileSystemAccessCloudIdentifier::New(kDriveFsProviderName,
                                                           item_id);
  std::move(callback).Run(std::move(cloud_identifier));
}

void DidGetProvidedFilesystemMetada(
    crosapi::mojom::HandleType expected_handle_type,
    crosapi::FileSystemAccessCloudIdentifierProviderAsh::
        GetCloudIdentifierCallback callback,
    std::unique_ptr<ash::file_system_provider::EntryMetadata> metadata,
    base::File::Error result) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  if (result != base::File::FILE_OK || !metadata ||
      !metadata->cloud_identifier || !metadata->is_directory.get() ||
      !MatchesExpectedProvidedFsType(expected_handle_type,
                                     *metadata->is_directory)) {
    std::move(callback).Run(nullptr);
    return;
  }

  crosapi::mojom::FileSystemAccessCloudIdentifierPtr cloud_identifier =
      crosapi::mojom::FileSystemAccessCloudIdentifier::New(
          metadata->cloud_identifier->provider_name,
          metadata->cloud_identifier->id);
  std::move(callback).Run(std::move(cloud_identifier));
}

}  // namespace

namespace cloud_identifier {

void GetCloudIdentifier(const base::FilePath& virtual_path,
                        crosapi::mojom::HandleType handle_type,
                        crosapi::FileSystemAccessCloudIdentifierProviderAsh::
                            GetCloudIdentifierCallback callback) {
  Profile* profile =
      g_browser_process->profile_manager()->GetActiveUserProfile();
  CHECK(profile);
  storage::FileSystemContext* file_system_context =
      file_manager::util::GetFileManagerFileSystemContext(profile);
  CHECK(file_system_context);

  const storage::FileSystemURL url =
      file_system_context->CreateCrackedFileSystemURL(
          blink::StorageKey(), storage::kFileSystemTypeExternal, virtual_path);
  CHECK(url.is_valid());

  if (url.type() == storage::kFileSystemTypeDriveFs) {
    auto* drive_integration_service =
        drive::util::GetIntegrationServiceByProfile(profile);
    drive_integration_service->GetMetadata(
        url.path(), base::BindOnce(&DidGetDriveFSMetadata, handle_type,
                                   std::move(callback)));
    return;
  }

  if (url.type() == storage::FileSystemType::kFileSystemTypeProvided) {
    ash::file_system_provider::util::FileSystemURLParser parser(url);
    if (!parser.Parse()) {
      std::move(callback).Run(nullptr);
      return;
    }

    ash::file_system_provider::ProvidedFileSystemInterface::MetadataFieldMask
        fields = ash::file_system_provider::ProvidedFileSystemInterface::
                     METADATA_FIELD_CLOUD_IDENTIFIER |
                 ash::file_system_provider::ProvidedFileSystemInterface::
                     METADATA_FIELD_IS_DIRECTORY;
    parser.file_system()->GetMetadata(
        parser.file_path(), fields,
        base::BindOnce(&DidGetProvidedFilesystemMetada, handle_type,
                       std::move(callback)));

    return;
  }

  // Unsupported file system type.
  std::move(callback).Run(nullptr);
}

}  // namespace cloud_identifier
