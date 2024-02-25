// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/file_manager/copy_or_move_encrypted_hook_delegate.h"

#include "chrome/browser/ash/drive/drive_integration_service.h"
#include "chrome/browser/ash/drive/file_system_util.h"
#include "components/drive/drive_api_util.h"
#include "content/public/browser/browser_thread.h"
#include "google_apis/common/task_util.h"

namespace file_manager::io_task {

namespace {

void CheckFileCompleted(
    storage::FileSystemURL source_url,
    base::RepeatingCallback<void(storage::FileSystemURL source_url)>
        skip_callback,
    CopyOrMoveEncryptedHookDelegate::ErrorCallback finish_callback,
    drive::FileError error,
    drivefs::mojom::FileMetadataPtr metadata) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  if (metadata &&
      drive::util::IsEncryptedMimeType(metadata->content_mime_type)) {
    skip_callback.Run(std::move(source_url));
    std::move(finish_callback)
        .Run(CopyOrMoveEncryptedHookDelegate::ErrorAction::kSkip);
  } else {
    std::move(finish_callback)
        .Run(CopyOrMoveEncryptedHookDelegate::ErrorAction::kDefault);
  }
}

void CheckFile(Profile* profile,
               storage::FileSystemURL source_url,
               base::RepeatingCallback<void(storage::FileSystemURL source_url)>
                   skip_callback,
               CopyOrMoveEncryptedHookDelegate::ErrorCallback finish_callback) {
  // We can only access drive integration service from the UI thread.
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  drive::DriveIntegrationService* drive_integration_service =
      drive::util::GetIntegrationServiceByProfile(profile);
  if (!drive_integration_service) {
    std::move(finish_callback)
        .Run(CopyOrMoveEncryptedHookDelegate::ErrorAction::kDefault);
    return;
  }

  base::FilePath source_path = source_url.path();
  drive_integration_service->GetMetadata(
      source_path,
      base::BindOnce(&CheckFileCompleted, std::move(source_url),
                     std::move(skip_callback), std::move(finish_callback)));
}

}  // anonymous namespace

CopyOrMoveEncryptedHookDelegate::CopyOrMoveEncryptedHookDelegate(
    Profile* profile,
    base::RepeatingCallback<void(storage::FileSystemURL source_url)>
        on_file_skipped)
    : on_file_skipped_(std::move(on_file_skipped)),
      check_file_(google_apis::CreateRelayCallback(
          base::BindRepeating(&CheckFile, profile))) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
}

CopyOrMoveEncryptedHookDelegate::~CopyOrMoveEncryptedHookDelegate() = default;

void CopyOrMoveEncryptedHookDelegate::OnError(
    const storage::FileSystemURL& source_url,
    const storage::FileSystemURL& destination_url __attribute__((unused)),
    base::File::Error error,
    ErrorCallback callback) {
  if (source_url.type() != storage::kFileSystemTypeDriveFs) {
    std::move(callback).Run(ErrorAction::kDefault);
    return;
  }

  if (error != base::File::FILE_ERROR_FAILED) {
    std::move(callback).Run(ErrorAction::kDefault);
    return;
  }

  check_file_.Run(source_url, on_file_skipped_,
                  google_apis::CreateRelayCallback(std::move(callback)));
}

}  // namespace file_manager::io_task
