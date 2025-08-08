// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/file_system_access/cloud_identifier/cloud_identifier_util_cros.h"

#include "base/check_is_test.h"
#include "chrome/browser/file_system_access/cloud_identifier/cloud_identifier_util_ash.h"
#include "chromeos/crosapi/mojom/file_system_access_cloud_identifier.mojom.h"
#include "storage/browser/file_system/file_system_url.h"
#include "third_party/blink/public/mojom/file_system_access/file_system_access_cloud_identifier.mojom.h"
#include "third_party/blink/public/mojom/file_system_access/file_system_access_error.mojom.h"

namespace {

blink::mojom::FileSystemAccessErrorPtr FileSystemAccessErrorOk() {
  return blink::mojom::FileSystemAccessError::New(
      blink::mojom::FileSystemAccessStatus::kOk, base::File::FILE_OK, "");
}

void OnCrosApiResult(
    content::ContentBrowserClient::GetCloudIdentifiersCallback callback,
    crosapi::mojom::FileSystemAccessCloudIdentifierPtr result) {
  if (result.is_null()) {
    std::move(callback).Run(
        blink::mojom::FileSystemAccessError::New(
            blink::mojom::FileSystemAccessStatus::kOperationFailed,
            base::File::Error::FILE_ERROR_FAILED,
            "Unable to retrieve identifier"),
        {});
    return;
  }

  std::vector<blink::mojom::FileSystemAccessCloudIdentifierPtr> handles;
  blink::mojom::FileSystemAccessCloudIdentifierPtr handle =
      blink::mojom::FileSystemAccessCloudIdentifier::New(result->provider_name,
                                                         result->id);
  handles.push_back(std::move(handle));
  std::move(callback).Run(FileSystemAccessErrorOk(), std::move(handles));
}

}  // namespace

namespace cloud_identifier {

void GetCloudIdentifierFromAsh(
    const storage::FileSystemURL& url,
    content::FileSystemAccessPermissionContext::HandleType handle_type,
    content::ContentBrowserClient::GetCloudIdentifiersCallback callback) {
  // Only `kFileSystemTypeDriveFs` and `kFileSystemTypeProvided` can be cloud
  // handled on ChromeOS.
  if (url.type() != storage::kFileSystemTypeDriveFs &&
      url.type() != storage::kFileSystemTypeProvided) {
    std::move(callback).Run(FileSystemAccessErrorOk(), {});
    return;
  }

  GetCloudIdentifier(url.virtual_path(), handle_type,
                     base::BindOnce(&OnCrosApiResult, std::move(callback)));
}

}  // namespace cloud_identifier
