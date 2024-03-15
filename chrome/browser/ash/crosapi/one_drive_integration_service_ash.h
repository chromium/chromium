// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_CROSAPI_ONE_DRIVE_INTEGRATION_SERVICE_ASH_H_
#define CHROME_BROWSER_ASH_CROSAPI_ONE_DRIVE_INTEGRATION_SERVICE_ASH_H_

#include "chromeos/crosapi/mojom/one_drive_integration_service.mojom.h"

#include "chrome/browser/ash/file_system_provider/observer.h"
#include "chrome/browser/ash/file_system_provider/service.h"
#include "mojo/public/cpp/bindings/receiver_set.h"
#include "mojo/public/cpp/bindings/remote_set.h"

namespace crosapi {

// The ash-chrome implementation of the OneDriveIntegrationService crosapi
// interface.
class OneDriveIntegrationServiceAsh
    : public mojom::OneDriveIntegrationService,
      public ash::file_system_provider::Observer {
 public:
  OneDriveIntegrationServiceAsh();
  ~OneDriveIntegrationServiceAsh() override;

  void BindReceiver(
      mojo::PendingReceiver<mojom::OneDriveIntegrationService> receiver);

  // mojom::OneDriveIntegrationService:
  void AddOneDriveMountObserver(
      mojo::PendingRemote<mojom::OneDriveMountObserver> observer) override;

 protected:
  // ash::file_system_provider::Observer:
  void OnProvidedFileSystemMount(
      const ash::file_system_provider::ProvidedFileSystemInfo& file_system_info,
      ash::file_system_provider::MountContext context,
      base::File::Error error) override;
  void OnProvidedFileSystemUnmount(
      const ash::file_system_provider::ProvidedFileSystemInfo& file_system_info,
      base::File::Error error) override;
  void OnShutDown() override;

 private:
  // This class supports any number of connections. This allows the client to
  // have multiple, potentially thread-affine, remotes.
  mojo::ReceiverSet<mojom::OneDriveIntegrationService> one_drive_service_set_;
  // Support any number of observers.
  mojo::RemoteSet<mojom::OneDriveMountObserver> observers_;

  base::ScopedObservation<ash::file_system_provider::Service,
                          ash::file_system_provider::Observer>
      file_system_provider_observation_{this};
};

}  // namespace crosapi

#endif  // CHROME_BROWSER_ASH_CROSAPI_ONE_DRIVE_INTEGRATION_SERVICE_ASH_H_
