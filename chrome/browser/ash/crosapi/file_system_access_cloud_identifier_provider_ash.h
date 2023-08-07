// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_CROSAPI_FILE_SYSTEM_ACCESS_CLOUD_IDENTIFIER_PROVIDER_ASH_H_
#define CHROME_BROWSER_ASH_CROSAPI_FILE_SYSTEM_ACCESS_CLOUD_IDENTIFIER_PROVIDER_ASH_H_

#include "chromeos/crosapi/mojom/file_system_access_cloud_identifier.mojom.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/receiver_set.h"

namespace base {
class FilePath;
}

namespace crosapi {

// The ash-chrome implementation of the
// `FileSystemAccessCloudIdentifierProvider` crosapi interface.
class FileSystemAccessCloudIdentifierProviderAsh
    : public mojom::FileSystemAccessCloudIdentifierProvider {
 public:
  FileSystemAccessCloudIdentifierProviderAsh();
  FileSystemAccessCloudIdentifierProviderAsh(
      const FileSystemAccessCloudIdentifierProviderAsh&) = delete;
  FileSystemAccessCloudIdentifierProviderAsh& operator=(
      const FileSystemAccessCloudIdentifierProviderAsh&) = delete;
  ~FileSystemAccessCloudIdentifierProviderAsh() override;

  void BindReceiver(
      mojo::PendingReceiver<mojom::FileSystemAccessCloudIdentifierProvider>
          receiver);

  // crosapi::mojom::FileSystemAccessCloudIdentifierProvider:
  void GetCloudIdentifier(const base::FilePath& virtual_path,
                          mojom::HandleType handle_type,
                          GetCloudIdentifierCallback callback) override;

 private:
  // This class supports any number of connections.
  mojo::ReceiverSet<mojom::FileSystemAccessCloudIdentifierProvider> receivers_;
};

}  // namespace crosapi

#endif  // CHROME_BROWSER_ASH_CROSAPI_FILE_SYSTEM_ACCESS_CLOUD_IDENTIFIER_PROVIDER_ASH_H_
