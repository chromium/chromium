// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_CROSAPI_FILE_SYSTEM_PROVIDER_SERVICE_ASH_H_
#define CHROME_BROWSER_ASH_CROSAPI_FILE_SYSTEM_PROVIDER_SERVICE_ASH_H_

#include <variant>

#include "base/files/file.h"
#include "base/values.h"
#include "chromeos/crosapi/mojom/file_system_provider.mojom.h"

class Profile;

namespace ash::file_system_provider {
class RequestManager;
class RequestValue;
}  // namespace ash::file_system_provider

namespace crosapi {

// Implements the ash side of the file_system_provider interface. This interface
// allows extensions to implement file systems. Extensions call into this class
// directly via c++ (ash) to fulfill information requests about the file system.
class FileSystemProviderServiceAsh : public mojom::FileSystemProviderService {
 public:
  FileSystemProviderServiceAsh();
  FileSystemProviderServiceAsh(const FileSystemProviderServiceAsh&) = delete;
  FileSystemProviderServiceAsh& operator=(const FileSystemProviderServiceAsh&) =
      delete;
  ~FileSystemProviderServiceAsh() override;

  void Notify(mojom::FileSystemIdPtr file_system_id,
              mojom::FSPWatcherPtr watcher,
              mojom::FSPChangeType type,
              std::vector<mojom::FSPChangePtr> changes,
              NotifyCallback callback) override;
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

  // In order to support multi-login in ash, all methods above are redirected to
  // a variation that supports directly passing in a Profile*.
  void NotifyWithProfile(mojom::FileSystemIdPtr file_system_id,
                         mojom::FSPWatcherPtr watcher,
                         mojom::FSPChangeType type,
                         std::vector<mojom::FSPChangePtr> changes,
                         NotifyCallback callback,
                         Profile* profile);
  void MountFinishedWithProfile(const std::string& extension_id,
                                int64_t request_id,
                                base::Value::List args,
                                MountFinishedCallback callback,
                                Profile* profile);

  // Forwards an operation response from an extension to the request manager and
  // then returns the error message. Empty string means success.
  std::string ForwardOperationResponse(
      ash::file_system_provider::RequestManager& manager,
      int64_t request_id,
      const ash::file_system_provider::RequestValue& value,
      std::variant<bool /*has_more*/, base::File::Error /*error*/> arg);
};

}  // namespace crosapi

#endif  // CHROME_BROWSER_ASH_CROSAPI_FILE_SYSTEM_PROVIDER_SERVICE_ASH_H_
