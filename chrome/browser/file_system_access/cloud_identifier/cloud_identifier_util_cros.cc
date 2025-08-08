// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/file_system_access/cloud_identifier/cloud_identifier_util_cros.h"

#include "chrome/browser/file_system_access/cloud_identifier/cloud_identifier_util_ash.h"
#include "storage/browser/file_system/file_system_url.h"
#include "third_party/blink/public/mojom/file_system_access/file_system_access_cloud_identifier.mojom.h"
#include "third_party/blink/public/mojom/file_system_access/file_system_access_error.mojom.h"

namespace {

blink::mojom::FileSystemAccessErrorPtr FileSystemAccessErrorOk() {
  return blink::mojom::FileSystemAccessError::New(
      blink::mojom::FileSystemAccessStatus::kOk, base::File::FILE_OK,
      std::string());
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

  GetCloudIdentifier(url.virtual_path(), handle_type, std::move(callback));
}

}  // namespace cloud_identifier
