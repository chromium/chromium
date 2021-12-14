// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_COMPONENTS_FWUPD_FIRMWARE_UPDATE_MANAGER_H_
#define ASH_COMPONENTS_FWUPD_FIRMWARE_UPDATE_MANAGER_H_

#include <string>

#include "ash/webui/firmware_update_ui/mojom/firmware_update.mojom.h"
#include "base/callback.h"
#include "base/component_export.h"
#include "base/containers/flat_map.h"
#include "base/files/file_path.h"
#include "base/files/scoped_file.h"
#include "base/memory/weak_ptr.h"
#include "base/task/sequenced_task_runner.h"
#include "chromeos/dbus/fwupd/fwupd_client.h"
#include "chromeos/dbus/fwupd/fwupd_device.h"
#include "chromeos/dbus/fwupd/fwupd_properties.h"
#include "chromeos/dbus/fwupd/fwupd_update.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "mojo/public/cpp/bindings/remote_set.h"

namespace ash {
// FirmwareUpdateManager contains all logic that runs the firmware update SWA.
class COMPONENT_EXPORT(ASH_FIRMWARE_UPDATE_MANAGER) FirmwareUpdateManager
    : public chromeos::FwupdClient::Observer,
      public firmware_update::mojom::UpdateProvider {
 public:
  FirmwareUpdateManager();
  FirmwareUpdateManager(const FirmwareUpdateManager&) = delete;
  FirmwareUpdateManager& operator=(const FirmwareUpdateManager&) = delete;
  ~FirmwareUpdateManager() override;

  // firmware_update::mojom::UpdateProvider
  void ObservePeripheralUpdates(
      mojo::PendingRemote<firmware_update::mojom::UpdateObserver> observer)
      override;

  // Gets the global instance pointer.
  static FirmwareUpdateManager* Get();

  // FwupdClient::Observer:
  // When the fwupd DBus client gets a response with devices from fwupd,
  // it calls this function and passes the response.
  void OnDeviceListResponse(chromeos::FwupdDeviceList* devices) override;

  // When the fwupd DBus client gets a response with updates from fwupd,
  // it calls this function and passes the response.
  void OnUpdateListResponse(const std::string& device_id,
                            chromeos::FwupdUpdateList* updates) override;
  void OnInstallResponse(bool success) override;
  // TODO(jimmyxgong): Implement this function to send property updates via
  // mojo.
  void OnPropertiesChangedResponse(
      chromeos::FwupdProperties* properties) override {}

  // Query all updates for all devices.
  void RequestAllUpdates();

  // TODO(jimmyxgong): This should override the mojo api interface.
  // Download and prepare the install file for a specific device.
  void StartInstall(const std::string& device_id,
                    int release,
                    base::OnceCallback<void()> callback);

  void BindInterface(
      mojo::PendingReceiver<firmware_update::mojom::UpdateProvider>
          pending_receiver);

 protected:
  friend class FirmwareUpdateManagerTest;
  // Temporary auxiliary variables for testing.
  // TODO(swifton): Replace with mock observers.
  int on_device_list_response_count_for_testing_ = 0;
  int on_update_list_response_count_for_testing_ = 0;
  int on_install_update_response_count_for_testing_ = 0;

 private:
  friend class FirmwareUpdateManagerTest;
  // Query the fwupd DBus client for currently connected devices.
  void RequestDevices();

  // Query the fwupd DBus client for updates for a certain device.
  void RequestUpdates(const std::string& device_id);

  // Query the fwupd DBus client to install an update for a certain device.
  void InstallUpdate(const std::string& device_id,
                     chromeos::FirmwareInstallOptions options,
                     base::OnceCallback<void()> callback,
                     base::ScopedFD file_descriptor);

  void OnCacheDirectoryCreated(const base::FilePath& root_path,
                               const std::string& device_id,
                               int release,
                               base::OnceCallback<void()> callback);

  // Notifies observers registered with ObservePeripheralUpdates() the current
  // list of devices with pending updates (if any).
  void NotifyUpdateListObservers();

  bool HasPendingUpdates();

  // Map of a device ID to `FwupdDevice` which is waiting for the list
  // of updates.
  base::flat_map<std::string, chromeos::FwupdDevice> devices_pending_update_;

  // List of all available updates. If `devices_pending_update_` is not
  // empty then this list is not yet complete.
  std::vector<firmware_update::mojom::FirmwareUpdatePtr> updates_;

  // Remotes for tracking observers that will be notified of changes to the
  // list of firmware updates.
  mojo::RemoteSet<firmware_update::mojom::UpdateObserver>
      update_list_observers_;

  scoped_refptr<base::SequencedTaskRunner> task_runner_;

  mojo::Receiver<firmware_update::mojom::UpdateProvider> receiver_{this};

  base::WeakPtrFactory<FirmwareUpdateManager> weak_ptr_factory_{this};
};

}  // namespace ash

#endif  // ASH_COMPONENTS_FWUPD_FIRMWARE_UPDATE_MANAGER_H_
