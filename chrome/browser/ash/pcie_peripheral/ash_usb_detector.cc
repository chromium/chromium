// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/pcie_peripheral/ash_usb_detector.h"

#include <memory>

#include "base/memory/weak_ptr.h"
#include "base/time/time.h"
#include "base/timer/timer.h"
#include "chromeos/ash/components/dbus/fwupd/fwupd_client.h"
#include "chromeos/ash/components/fwupd/firmware_update_manager.h"
#include "chromeos/ash/components/peripheral_notification/peripheral_notification_manager.h"
#include "content/public/browser/device_service.h"

namespace ash {

namespace {

static AshUsbDetector* g_ash_usb_detector = nullptr;

constexpr int kRequestUpdatesIntervalInSeconds = 5;
constexpr int kMaxNumRequestUpdatesRetries = 3;

}  // namespace

AshUsbDetector::AshUsbDetector() {
  DCHECK(!g_ash_usb_detector);
  g_ash_usb_detector = this;
  fetch_updates_repeating_timer_ = std::make_unique<base::RepeatingTimer>();
}

AshUsbDetector::~AshUsbDetector() {
  DCHECK_EQ(this, g_ash_usb_detector);
  g_ash_usb_detector = nullptr;
}

// static
AshUsbDetector* AshUsbDetector::Get() {
  return g_ash_usb_detector;
}

void AshUsbDetector::ConnectToDeviceManager() {
  if (!device_manager_) {
    content::GetDeviceService().BindUsbDeviceManager(
        device_manager_.BindNewPipeAndPassReceiver());
  }
  DCHECK(device_manager_);
  device_manager_.set_disconnect_handler(
      base::BindOnce(&AshUsbDetector::OnDeviceManagerConnectionError,
                     weak_ptr_factory_.GetWeakPtr()));

  // Listen for added/removed device events.
  DCHECK(!client_receiver_.is_bound());
  device_manager_->EnumerateDevicesAndSetClient(
      client_receiver_.BindNewEndpointAndPassRemote(),
      base::BindOnce(&AshUsbDetector::OnListAttachedDevices,
                     weak_ptr_factory_.GetWeakPtr()));
}

int32_t AshUsbDetector::GetOnDeviceCheckedCountForTesting() {
  return on_device_checked_counter_for_testing_;
}

void AshUsbDetector::OnDeviceAdded(
    device::mojom::UsbDeviceInfoPtr device_info) {
  std::string guid = device_info->guid;
  device_manager_->CheckAccess(
      guid,
      base::BindOnce(&AshUsbDetector::OnDeviceChecked,
                     weak_ptr_factory_.GetWeakPtr(), std::move(device_info)));

  RequestAllUpdatesWithRepeatDelay();
}

void AshUsbDetector::OnDeviceRemoved(
    device::mojom::UsbDeviceInfoPtr device_info) {
  RequestAllUpdatesWithRepeatDelay();
}

void AshUsbDetector::RequestAllUpdatesWithRepeatDelay() {
  if (!fetch_updates_repeating_timer_->IsRunning()) {
    num_request_updates_repeats_ = kMaxNumRequestUpdatesRetries;
    fetch_updates_repeating_timer_->Start(
        FROM_HERE, base::Seconds(kRequestUpdatesIntervalInSeconds),
        base::BindRepeating(&AshUsbDetector::RequestUpdates,
                            base::Unretained(this)));
  } else {
    // Request to fetch all updates was a called during a current repeat cycle.
    // Reset the number of requests back to the default.
    num_request_updates_repeats_ = kMaxNumRequestUpdatesRetries;
  }
}

void AshUsbDetector::OnListAttachedDevices(
    std::vector<device::mojom::UsbDeviceInfoPtr> devices) {
  for (device::mojom::UsbDeviceInfoPtr& device_info : devices)
    AshUsbDetector::OnDeviceAdded(std::move(device_info));
}

void AshUsbDetector::OnDeviceChecked(
    device::mojom::UsbDeviceInfoPtr device_info,
    bool allowed) {
  if (!allowed)
    return;

  ash::PeripheralNotificationManager::Get()->OnDeviceConnected(
      device_info.get());

  if (is_testing_)
    ++on_device_checked_counter_for_testing_;
}

void AshUsbDetector::OnDeviceManagerConnectionError() {
  device_manager_.reset();
  client_receiver_.reset();
  ConnectToDeviceManager();
}

void AshUsbDetector::RequestUpdates() {
  if (is_testing_) {
    ++num_request_for_fetch_updates_for_testing_;
  } else {
    if (FirmwareUpdateManager::IsInitialized()) {
      FirmwareUpdateManager::Get()->RequestAllUpdates(
          FirmwareUpdateManager::Source::kUSBChange);
    }
  }

  --num_request_updates_repeats_;

  if (num_request_updates_repeats_ == 0) {
    fetch_updates_repeating_timer_->Stop();
  }
}

void AshUsbDetector::SetDeviceManagerForTesting(
    mojo::PendingRemote<device::mojom::UsbDeviceManager> device_manager) {
  DCHECK(!device_manager_) << "device_manager_ was already initialized";
  device_manager_.Bind(std::move(device_manager));
  is_testing_ = true;
}

void AshUsbDetector::SetFetchUpdatesTimerForTesting(
    std::unique_ptr<base::RepeatingTimer> timer) {
  fetch_updates_repeating_timer_ = std::move(timer);
}

}  // namespace ash
