// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_PCIE_PERIPHERAL_ASH_USB_DETECTOR_H_
#define CHROME_BROWSER_ASH_PCIE_PERIPHERAL_ASH_USB_DETECTOR_H_

#include <memory>

#include "ash/public/cpp/ash_public_export.h"
#include "base/timer/timer.h"
#include "mojo/public/cpp/bindings/associated_receiver.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "services/device/public/mojom/usb_device.mojom.h"
#include "services/device/public/mojom/usb_manager.mojom.h"
#include "services/device/public/mojom/usb_manager_client.mojom.h"

namespace ash {

// Detects usb devices when they are connected and notifies ash. Similar to
// CrosUsbDetector and WebUsbDetector.
class ASH_PUBLIC_EXPORT AshUsbDetector
    : public device::mojom::UsbDeviceManagerClient {
 public:
  AshUsbDetector();
  ~AshUsbDetector() override;

  static AshUsbDetector* Get();

  // Connect to the device manager to be notified of connection/removal.
  // Used during browser startup, after connection errors and to setup a fake
  // device manager during testing.
  void ConnectToDeviceManager();

 private:
  friend class AshUsbDetectorTest;

  int32_t GetOnDeviceCheckedCountForTesting();

  // device::mojom::UsbDeviceManagerClient
  void OnDeviceAdded(device::mojom::UsbDeviceInfoPtr device_info) override;
  void OnDeviceRemoved(device::mojom::UsbDeviceInfoPtr device) override;

  void OnListAttachedDevices(
      std::vector<device::mojom::UsbDeviceInfoPtr> devices);
  void OnDeviceChecked(device::mojom::UsbDeviceInfoPtr device_info,
                       bool allowed);
  void OnDeviceManagerConnectionError();

  void RequestAllUpdatesWithRepeatDelay();
  void RequestUpdates();

  void SetDeviceManagerForTesting(
      mojo::PendingRemote<device::mojom::UsbDeviceManager> device_manager);

  void SetFetchUpdatesTimerForTesting(
      std::unique_ptr<base::RepeatingTimer> timer);

  void SetIsTesting(bool is_testing) { is_testing_ = is_testing; }

  int num_request_for_fetch_updates_for_testing() {
    return num_request_for_fetch_updates_for_testing_;
  }

  mojo::Remote<device::mojom::UsbDeviceManager> device_manager_;
  mojo::AssociatedReceiver<device::mojom::UsbDeviceManagerClient>
      client_receiver_{this};

  int32_t on_device_checked_counter_for_testing_ = 0;
  int32_t num_request_for_fetch_updates_for_testing_ = 0;
  bool is_testing_ = false;

  int num_request_updates_repeats_;
  std::unique_ptr<base::RepeatingTimer> fetch_updates_repeating_timer_;

  // WeakPtrFactory to use for callbacks.
  base::WeakPtrFactory<AshUsbDetector> weak_ptr_factory_{this};
};

}  // namespace ash

#endif  // CHROME_BROWSER_ASH_PCIE_PERIPHERAL_ASH_USB_DETECTOR_H_
