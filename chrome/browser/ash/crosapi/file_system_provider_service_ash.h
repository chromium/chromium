// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_CROSAPI_FILE_SYSTEM_PROVIDER_SERVICE_ASH_H_
#define CHROME_BROWSER_ASH_CROSAPI_FILE_SYSTEM_PROVIDER_SERVICE_ASH_H_

#include "base/values.h"
#include "chromeos/crosapi/mojom/file_system_provider.mojom.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/receiver_set.h"
#include "mojo/public/cpp/bindings/remote_set.h"

class Profile;

namespace crosapi {

// Implements the ash side of the file_system_provider interface. This interface
// allows extensions to implement file systems. Extensions call into this class
// either directly via c++ (ash) or via crosapi (lacros) to fulfill information
// requests about the file system.
class FileSystemProviderServiceAsh : public mojom::FileSystemProviderService {
 public:
  FileSystemProviderServiceAsh();
  FileSystemProviderServiceAsh(const FileSystemProviderServiceAsh&) = delete;
  FileSystemProviderServiceAsh& operator=(const FileSystemProviderServiceAsh&) =
      delete;
  ~FileSystemProviderServiceAsh() override;

  void BindReceiver(
      mojo::PendingReceiver<mojom::FileSystemProviderService> receiver);

  // crosapi::mojom::FileSystemProviderService:
  void RegisterFileSystemProvider(
      mojo::PendingRemote<mojom::FileSystemProvider> provider) override;
  void Mount(mojom::FileSystemMetadataPtr metadata,
             bool persistent,
             MountCallback callback) override;
  void Unmount(mojom::FileSystemIdPtr file_system_id,
               UnmountCallback callback) override;
  void GetAll(const std::string& provider, GetAllCallback callback) override;
  void Get(mojom::FileSystemIdPtr file_system_id,
           GetCallback callback) override;
  void Notify(mojom::FileSystemIdPtr file_system_id,
              mojom::FSPWatcherPtr watcher,
              mojom::FSPChangeType type,
              std::vector<mojom::FSPChangePtr> changes,
              NotifyCallback callback) override;
  void DeprecatedOperationFinished(mojom::FSPOperationResponse response,
                                   mojom::FileSystemIdPtr file_system_id,
                                   int64_t request_id,
                                   std::vector<base::Value> args,
                                   OperationFinishedCallback callback) override;
  void OperationFinished(mojom::FSPOperationResponse response,
                         mojom::FileSystemIdPtr file_system_id,
                         int64_t request_id,
                         base::Value::List args,
                         OperationFinishedCallback callback) override;
  void OpenFileFinishedSuccessfully(
      mojom::FileSystemIdPtr file_system_id,
      int64_t request_id,
      base::Value::List args,
      OperationFinishedCallback callback) override;
  void MountFinished(const std::string& extension_id,
                     int64_t request_id,
                     base::Value::List args,
                     MountFinishedCallback callback) override;
  void ExtensionLoadedDeprecated(bool configurable,
                                 bool watchable,
                                 bool multiple_mounts,
                                 mojom::FileSystemSource source,
                                 const std::string& name,
                                 const std::string& id) override;
  void ExtensionLoaded(bool configurable,
                       bool watchable,
                       bool multiple_mounts,
                       mojom::FileSystemSource source,
                       const std::string& name,
                       const std::string& id,
                       const gfx::ImageSkia& icon16x16,
                       const gfx::ImageSkia& icon32x32) override;
  void ExtensionUnloaded(const std::string& id, bool due_to_shutdown) override;

  // In order to support multi-login in ash, a legacy feature that is going
  // away in Lacros, all methods above are redirected to a variation that
  // supports directly passing in a Profile*.
  void MountWithProfile(mojom::FileSystemMetadataPtr metadata,
                        bool persistent,
                        MountCallback callback,
                        Profile* profile);
  void UnmountWithProfile(mojom::FileSystemIdPtr file_system_id,
                          UnmountCallback callback,
                          Profile* profile);
  void GetAllWithProfile(const std::string& provider,
                         GetAllCallback callback,
                         Profile* profile);
  void GetWithProfile(mojom::FileSystemIdPtr file_system_id,
                      GetCallback callback,
                      Profile* profile);
  void NotifyWithProfile(mojom::FileSystemIdPtr file_system_id,
                         mojom::FSPWatcherPtr watcher,
                         mojom::FSPChangeType type,
                         std::vector<mojom::FSPChangePtr> changes,
                         NotifyCallback callback,
                         Profile* profile);
  void OperationFinishedWithProfile(mojom::FSPOperationResponse response,
                                    mojom::FileSystemIdPtr file_system_id,
                                    int64_t request_id,
                                    base::Value::List args,
                                    OperationFinishedCallback callback,
                                    Profile* profile);
  void OpenFileFinishedSuccessfullyWithProfile(
      mojom::FileSystemIdPtr file_system_id,
      int64_t request_id,
      base::Value::List args,
      OperationFinishedCallback callback,
      Profile* profile);
  void MountFinishedWithProfile(const std::string& extension_id,
                                int64_t request_id,
                                base::Value::List args,
                                MountFinishedCallback callback,
                                Profile* profile);

  // Exposed so that ash clients can work with Lacros file system providers.
  mojo::RemoteSet<mojom::FileSystemProvider>& remotes() { return remotes_; }

 private:
  // Each separate Lacros process owns its own remote.
  mojo::RemoteSet<mojom::FileSystemProvider> remotes_;
  // Receives events from Lacros file system provider extensions.
  mojo::ReceiverSet<mojom::FileSystemProviderService> receivers_;
};

}  // namespace crosapi

#endif  // CHROME_BROWSER_ASH_CROSAPI_FILE_SYSTEM_PROVIDER_SERVICE_ASH_H_
