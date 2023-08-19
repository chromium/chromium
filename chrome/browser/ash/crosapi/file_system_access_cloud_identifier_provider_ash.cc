// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/crosapi/file_system_access_cloud_identifier_provider_ash.h"

#include "base/files/file_path.h"
#include "chrome/browser/file_system_access/cloud_identifier/cloud_identifier_util_ash.h"

namespace crosapi {

FileSystemAccessCloudIdentifierProviderAsh::
    FileSystemAccessCloudIdentifierProviderAsh() = default;
FileSystemAccessCloudIdentifierProviderAsh::
    ~FileSystemAccessCloudIdentifierProviderAsh() = default;

void FileSystemAccessCloudIdentifierProviderAsh::BindReceiver(
    mojo::PendingReceiver<mojom::FileSystemAccessCloudIdentifierProvider>
        receiver) {
  receivers_.Add(this, std::move(receiver));
}

void FileSystemAccessCloudIdentifierProviderAsh::GetCloudIdentifier(
    const base::FilePath& virtual_path,
    mojom::HandleType handle_type,
    GetCloudIdentifierCallback callback) {
  cloud_identifier::GetCloudIdentifier(virtual_path, handle_type,
                                       std::move(callback));
}

}  // namespace crosapi
