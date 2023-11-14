// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_CROSAPI_DRIVE_INTEGRATION_SERVICE_ASH_H_
#define CHROME_BROWSER_ASH_CROSAPI_DRIVE_INTEGRATION_SERVICE_ASH_H_

#include "chrome/browser/ash/drive/drive_integration_service.h"
#include "chromeos/crosapi/mojom/drive_integration_service.mojom.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/receiver_set.h"
#include "mojo/public/cpp/bindings/remote_set.h"

namespace crosapi {

// Implements the crosapi interface for DriveIntegrationService. Lives in
// Ash-Chrome on the UI thread.
class DriveIntegrationServiceAsh : public mojom::DriveIntegrationService,
                                   drive::DriveIntegrationService::Observer {
 public:
  DriveIntegrationServiceAsh();
  DriveIntegrationServiceAsh(const DriveIntegrationServiceAsh&) = delete;
  DriveIntegrationServiceAsh& operator=(const DriveIntegrationServiceAsh&) =
      delete;
  ~DriveIntegrationServiceAsh() override;

  void BindReceiver(
      mojo::PendingReceiver<mojom::DriveIntegrationService> receiver);

  // crosapi::mojom::DriveIntegrationService:
  void DeprecatedGetMountPointPath(
      DeprecatedGetMountPointPathCallback callback) override;
  void AddDriveIntegrationServiceObserver(
      mojo::PendingRemote<mojom::DriveIntegrationServiceObserver> observer)
      override;
  void CreateNativeHostSession(
      drivefs::mojom::ExtensionConnectionParamsPtr params,
      mojo::PendingReceiver<drivefs::mojom::NativeMessagingHost>
          drivefs_receiver,
      mojo::PendingRemote<drivefs::mojom::NativeMessagingPort> extension_remote)
      override;
  void RegisterDriveFsNativeMessageHostBridge(
      mojo::PendingRemote<crosapi::mojom::DriveFsNativeMessageHostBridge>
          bridge) override;

  // DriveIntegrationService::Observer implementation.
  void OnFileSystemMounted() override;
  void OnFileSystemBeingUnmounted() override;
  void OnFileSystemMountFailed() override;

 private:
  // This class supports any number of connections. This allows the client to
  // have multiple, potentially thread-affine, remotes.
  mojo::ReceiverSet<mojom::DriveIntegrationService> receivers_;
  // Support any number of observers.
  mojo::RemoteSet<mojom::DriveIntegrationServiceObserver> observers_;

  mojo::Remote<mojom::DriveFsNativeMessageHostBridge> native_message_bridge_;
};

}  // namespace crosapi

#endif  // CHROME_BROWSER_ASH_CROSAPI_DRIVE_INTEGRATION_SERVICE_ASH_H_
