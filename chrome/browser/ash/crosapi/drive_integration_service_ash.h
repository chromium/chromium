// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_CROSAPI_DRIVE_INTEGRATION_SERVICE_ASH_H_
#define CHROME_BROWSER_ASH_CROSAPI_DRIVE_INTEGRATION_SERVICE_ASH_H_

#include "base/scoped_observation.h"
#include "chrome/browser/ash/drive/drive_integration_service.h"
#include "chromeos/crosapi/mojom/drive_integration_service.mojom.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/receiver_set.h"
#include "mojo/public/cpp/bindings/remote_set.h"

namespace crosapi {

// Implements the crosapi interface for DriveIntegrationService. Lives in
// Ash-Chrome on the UI thread.
class DriveIntegrationServiceAsh
    : public mojom::DriveIntegrationService,
      public drive::DriveIntegrationServiceObserver {
 public:
  DriveIntegrationServiceAsh();
  DriveIntegrationServiceAsh(const DriveIntegrationServiceAsh&) = delete;
  DriveIntegrationServiceAsh& operator=(const DriveIntegrationServiceAsh&) =
      delete;
  ~DriveIntegrationServiceAsh() override;

  void BindReceiver(
      mojo::PendingReceiver<mojom::DriveIntegrationService> receiver);

  // crosapi::mojom::DriveIntegrationService:
  void GetMountPointPath(GetMountPointPathCallback callback) override;
  void AddDriveIntegrationServiceObserver(
      mojo::PendingRemote<mojom::DriveIntegrationServiceObserver> observer)
      override;

  // drivefs::DriveIntegrationServiceObserver:
  void OnFileSystemMounted() override;
  void OnFileSystemBeingUnmounted() override;
  void OnFileSystemMountFailed() override;
  void OnDriveIntegrationServiceDestroyed() override;

 private:
  base::ScopedObservation<drive::DriveIntegrationService,
                          drive::DriveIntegrationServiceObserver>
      drive_service_observation_{this};
  // This class supports any number of connections. This allows the client to
  // have multiple, potentially thread-affine, remotes.
  mojo::ReceiverSet<mojom::DriveIntegrationService> receivers_;
  // Support any number of observers.
  mojo::RemoteSet<mojom::DriveIntegrationServiceObserver> observers_;
};

}  // namespace crosapi

#endif  // CHROME_BROWSER_ASH_CROSAPI_DRIVE_INTEGRATION_SERVICE_ASH_H_
