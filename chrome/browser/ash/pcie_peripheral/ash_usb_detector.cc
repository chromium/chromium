// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/pcie_peripheral/ash_usb_detector.h"

#include "ash/components/peripheral_notification/peripheral_notification_manager.h"
#include "base/memory/weak_ptr.h"
#include "content/public/browser/device_service.h"

namespace ash {

namespace {
static AshUsbDetector* g_ash_usb_detector = nullptr;
}  // namespace

AshUsbDetector::AshUsbDetector() {
  DCHECK(!g_ash_usb_detector);
  g_ash_usb_detector = this;
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
}

void AshUsbDetector::OnDeviceRemoved(
    device::mojom::UsbDeviceInfoPtr device_info) {}

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

void AshUsbDetector::SetDeviceManagerForTesting(
    mojo::PendingRemote<device::mojom::UsbDeviceManager> device_manager) {
  DCHECK(!device_manager_) << "device_manager_ was already initialized";
  device_manager_.Bind(std::move(device_manager));
  is_testing_ = true;
}

}  // namespace ash
