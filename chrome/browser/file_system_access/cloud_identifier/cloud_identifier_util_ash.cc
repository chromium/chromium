// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/file_system_access/cloud_identifier/cloud_identifier_util_ash.h"

#include <memory>
#include <optional>
#include <string>

#include "base/files/file.h"
#include "base/strings/string_util.h"
#include "chrome/browser/ash/drive/drive_integration_service.h"
#include "chrome/browser/ash/drive/file_system_util.h"
#include "chrome/browser/ash/file_system_provider/mount_path_util.h"
#include "chrome/browser/ash/file_system_provider/provided_file_system_interface.h"
#include "chrome/browser/ash/profiles/profile_helper.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chromeos/ash/components/drivefs/drivefs_util.h"
#include "chromeos/ash/components/drivefs/mojom/drivefs.mojom.h"
#include "components/drive/file_errors.h"
#include "content/public/browser/browser_thread.h"
#include "storage/browser/file_system/file_system_url.h"
#include "third_party/blink/public/mojom/file_system_access/file_system_access_cloud_identifier.mojom.h"
#include "third_party/blink/public/mojom/file_system_access/file_system_access_error.mojom.h"

namespace {
constexpr char kDriveFsProviderName[] = "drive.google.com";

// Files/directories which have not yet been uploaded have not received a proper
// ID yet and will have a "local" ID instead. This prefix is defined upstream at
// http://google3/apps/drive/cello/common/item_util.h
constexpr char kDriveFsLocalPrefix[] = "local-";

blink::mojom::FileSystemAccessErrorPtr FileSystemAccessErrorOk() {
  return blink::mojom::FileSystemAccessError::New(
      blink::mojom::FileSystemAccessStatus::kOk, base::File::FILE_OK,
      std::string());
}

blink::mojom::FileSystemAccessErrorPtr FileSystemAccessErrorFailed() {
  return blink::mojom::FileSystemAccessError::New(
      blink::mojom::FileSystemAccessStatus::kOperationFailed,
      base::File::Error::FILE_ERROR_FAILED, "Unable to retrieve identifier");
}

bool MatchesExpectedDriveFsType(
    content::FileSystemAccessPermissionContext::HandleType expected_handle_type,
    drivefs::mojom::FileMetadata::Type type) {
  switch (expected_handle_type) {
    case content::FileSystemAccessPermissionContext::HandleType::kFile:
      return drivefs::IsAFile(type);
    case content::FileSystemAccessPermissionContext::HandleType::kDirectory:
      return drivefs::IsADirectory(type);
  }
}

bool MatchesExpectedProvidedFsType(
    content::FileSystemAccessPermissionContext::HandleType expected_handle_type,
    bool is_directory) {
  switch (expected_handle_type) {
    case content::FileSystemAccessPermissionContext::HandleType::kFile:
      return !is_directory;
    case content::FileSystemAccessPermissionContext::HandleType::kDirectory:
      return is_directory;
  }
}

void DidGetDriveFSMetadata(
    content::FileSystemAccessPermissionContext::HandleType expected_handle_type,
    content::ContentBrowserClient::GetCloudIdentifiersCallback callback,
    drive::FileError error,
    drivefs::mojom::FileMetadataPtr metadata) {
  if (error != drive::FILE_ERROR_OK || metadata.is_null() ||
      !metadata->item_id.has_value() ||
      base::StartsWith(metadata->item_id.value(), kDriveFsLocalPrefix) ||
      !MatchesExpectedDriveFsType(expected_handle_type, metadata->type)) {
    // TODO(crbug.com/434161032): Forward `error`.
    std::move(callback).Run(FileSystemAccessErrorFailed(), {});
    return;
  }

  std::vector<blink::mojom::FileSystemAccessCloudIdentifierPtr> handles;
  handles.push_back(blink::mojom::FileSystemAccessCloudIdentifier::New(
      kDriveFsProviderName, metadata->item_id.value()));
  std::move(callback).Run(FileSystemAccessErrorOk(), std::move(handles));
}

void DidGetProvidedFilesystemMetada(
    content::FileSystemAccessPermissionContext::HandleType expected_handle_type,
    content::ContentBrowserClient::GetCloudIdentifiersCallback callback,
    std::unique_ptr<ash::file_system_provider::EntryMetadata> metadata,
    base::File::Error result) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  if (result != base::File::FILE_OK || !metadata ||
      !metadata->cloud_identifier || !metadata->is_directory.get() ||
      !MatchesExpectedProvidedFsType(expected_handle_type,
                                     *metadata->is_directory)) {
    // TODO(crbug.com/434161032): Forward `result`.
    std::move(callback).Run(FileSystemAccessErrorFailed(), {});
    return;
  }

  std::vector<blink::mojom::FileSystemAccessCloudIdentifierPtr> handles;
  handles.push_back(blink::mojom::FileSystemAccessCloudIdentifier::New(
      metadata->cloud_identifier->provider_name,
      metadata->cloud_identifier->id));
  std::move(callback).Run(FileSystemAccessErrorOk(), std::move(handles));
}

}  // namespace

namespace cloud_identifier {

void GetCloudIdentifier(
    const storage::FileSystemURL& url,
    content::FileSystemAccessPermissionContext::HandleType handle_type,
    content::ContentBrowserClient::GetCloudIdentifiersCallback callback) {
  if (url.type() == storage::kFileSystemTypeDriveFs) {
    // TODO(crbug.com/434161032): Pass correct Profile via a param.
    Profile* profile =
        g_browser_process->profile_manager()->GetActiveUserProfile();
    CHECK(profile);

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
      std::move(callback).Run(FileSystemAccessErrorFailed(), {});
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

  // Only `kFileSystemTypeDriveFs` and `kFileSystemTypeProvided` can be cloud
  // handled on ChromeOS.
  std::move(callback).Run(FileSystemAccessErrorOk(), {});
}

}  // namespace cloud_identifier
