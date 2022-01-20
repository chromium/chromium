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

namespace network {

class SimpleURLLoader;

}  // namespace network

namespace ash {
// FirmwareUpdateManager contains all logic that runs the firmware update SWA.
class COMPONENT_EXPORT(ASH_FIRMWARE_UPDATE_MANAGER) FirmwareUpdateManager
    : public chromeos::FwupdClient::Observer,
      public firmware_update::mojom::UpdateProvider,
      public firmware_update::mojom::InstallController {
 public:
  FirmwareUpdateManager();
  FirmwareUpdateManager(const FirmwareUpdateManager&) = delete;
  FirmwareUpdateManager& operator=(const FirmwareUpdateManager&) = delete;
  ~FirmwareUpdateManager() override;

  // firmware_update::mojom::UpdateProvider
  void ObservePeripheralUpdates(
      mojo::PendingRemote<firmware_update::mojom::UpdateObserver> observer)
      override;

  void PrepareForUpdate(const std::string& device_id,
                        PrepareForUpdateCallback callback) override;

  void FetchInProgressUpdate(FetchInProgressUpdateCallback callback) override;

  // firmware_update::mojom::InstallController
  void BeginUpdate(const std::string& device_id,
                   const base::FilePath& filepath) override;

  void AddObserver(
      mojo::PendingRemote<firmware_update::mojom::UpdateProgressObserver>
          observer) override;

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
      chromeos::FwupdProperties* properties) override;

  // Query all updates for all devices.
  void RequestAllUpdates();

  // TODO(jimmyxgong): This should override the mojo api interface.
  // Download and prepare the install file for a specific device.
  void StartInstall(const std::string& device_id,
                    const base::FilePath& filepath,
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

  void CreateLocalPatchFile(const base::FilePath& cache_path,
                            const std::string& device_id,
                            const base::FilePath& filepath,
                            base::OnceCallback<void()> callback);

  void DownloadFileToInternal(const base::FilePath& patch_path,
                              const std::string& device_id,
                              const base::FilePath& filepath,
                              base::OnceCallback<void()> callback,
                              bool write_file_success);

  void OnUrlDownloadedToFile(
      const std::string& device_id,
      std::unique_ptr<network::SimpleURLLoader> simple_loader,
      base::OnceCallback<void()> callback,
      base::FilePath download_path);

  // Notifies observers registered with ObservePeripheralUpdates() the current
  // list of devices with pending updates (if any).
  void NotifyUpdateListObservers();

  bool HasPendingUpdates();

  void SetFakeUrlForTesting(const std::string& fake_url) {
    fake_url_for_testing_ = fake_url;
  }

  int GetNumUpdatesForTesting() { return updates_.size(); }

  // Resets the mojo::Receiver |install_controller_receiver_|
  // and |update_progress_observer_|.
  void ResetInstallState();

  // Map of a device ID to `FwupdDevice` which is waiting for the list
  // of updates.
  base::flat_map<std::string, chromeos::FwupdDevice> devices_pending_update_;

  // List of all available updates. If `devices_pending_update_` is not
  // empty then this list is not yet complete.
  std::vector<firmware_update::mojom::FirmwareUpdatePtr> updates_;

  // Only used for testing if StartInstall() queries to a fake URL.
  std::string fake_url_for_testing_;

  // The device update that is currently inflight.
  firmware_update::mojom::FirmwareUpdatePtr inflight_update_;

  // Remotes for tracking observers that will be notified of changes to the
  // list of firmware updates.
  mojo::RemoteSet<firmware_update::mojom::UpdateObserver>
      update_list_observers_;

  // Remote for tracking observer that will be notified of changes to
  // the in-progress update.
  mojo::Remote<firmware_update::mojom::UpdateProgressObserver>
      update_progress_observer_;

  scoped_refptr<base::SequencedTaskRunner> task_runner_;

  mojo::Receiver<firmware_update::mojom::UpdateProvider> receiver_{this};

  mojo::Receiver<firmware_update::mojom::InstallController>
      install_controller_receiver_{this};

  base::WeakPtrFactory<FirmwareUpdateManager> weak_ptr_factory_{this};
};

}  // namespace ash

#endif  // ASH_COMPONENTS_FWUPD_FIRMWARE_UPDATE_MANAGER_H_
