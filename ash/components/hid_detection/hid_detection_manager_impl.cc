// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/components/hid_detection/hid_detection_manager_impl.h"

#include "ash/components/hid_detection/hid_detection_utils.h"
#include "base/no_destructor.h"
#include "components/device_event_log/device_event_log.h"

namespace ash::hid_detection {
namespace {
// Global InputDeviceManagerBinder instance that can be overridden in tests.
base::NoDestructor<HidDetectionManagerImpl::InputDeviceManagerBinder>
    g_input_device_manager_binder;
}  // namespace

// static
void HidDetectionManagerImpl::SetInputDeviceManagerBinderForTest(
    InputDeviceManagerBinder binder) {
  *g_input_device_manager_binder = std::move(binder);
}

HidDetectionManagerImpl::HidDetectionManagerImpl(
    device::mojom::DeviceService* device_service)
    : device_service_{device_service} {}

HidDetectionManagerImpl::~HidDetectionManagerImpl() = default;

void HidDetectionManagerImpl::GetIsHidDetectionRequired(
    base::OnceCallback<void(bool)> callback) {
  BindToInputDeviceManagerIfNeeded();

  HID_LOG(EVENT) << "Fetching input devices for GetIsHidDetectionRequired().";
  input_device_manager_->GetDevices(
      base::BindOnce(&HidDetectionManagerImpl::OnGetDevicesForIsRequired,
                     weak_ptr_factory_.GetWeakPtr(), std::move(callback)));
}

void HidDetectionManagerImpl::PerformStartHidDetection() {
  BindToInputDeviceManagerIfNeeded();

  HID_LOG(EVENT) << "Starting HID detection by fetching input devices.";
  input_device_manager_->GetDevicesAndSetClient(
      input_device_manager_receiver_.BindNewEndpointAndPassRemote(),
      base::BindOnce(&HidDetectionManagerImpl::OnGetDevicesAndSetClient,
                     weak_ptr_factory_.GetWeakPtr()));
}

void HidDetectionManagerImpl::PerformStopHidDetection() {
  HID_LOG(EVENT) << "Stopping HID detection.";
  input_device_manager_receiver_.reset();
}

HidDetectionManager::HidDetectionStatus
HidDetectionManagerImpl::ComputeHidDetectionStatus() const {
  return HidDetectionManager::HidDetectionStatus{
      /*touchscreen_detected=*/connected_touchscreen_id_.has_value()};
}

void HidDetectionManagerImpl::InputDeviceAdded(
    device::mojom::InputDeviceInfoPtr info) {
  HID_LOG(EVENT) << "Input device added, id: " << info->id
                 << ", name: " << info->name;
  const std::string& device_id = info->id;
  device_id_to_device_map_[device_id] = std::move(info);

  if (AttemptSetDeviceAsConnectedHid(*device_id_to_device_map_[device_id]))
    NotifyHidDetectionStatusChanged();
}

void HidDetectionManagerImpl::InputDeviceRemoved(const std::string& id) {
  DCHECK(device_id_to_device_map_[id])
      << " Input device removed was not found in "
         "|device_id_to_device_map_|.";
  HID_LOG(EVENT) << "Input device removed, id: " << id
                 << ", name: " << device_id_to_device_map_[id]->name;
  device_id_to_device_map_.erase(id);
  bool was_connected_hid_disconnected_ = false;

  if (id == connected_touchscreen_id_) {
    HID_LOG(EVENT) << "Removing touchscreen: " << id;
    connected_touchscreen_id_.reset();
    was_connected_hid_disconnected_ = true;
  }

  if (was_connected_hid_disconnected_) {
    SetConnectedHids();
    NotifyHidDetectionStatusChanged();
  }
}

void HidDetectionManagerImpl::BindToInputDeviceManagerIfNeeded() {
  if (input_device_manager_.is_bound())
    return;

  mojo::PendingReceiver<device::mojom::InputDeviceManager> receiver =
      input_device_manager_.BindNewPipeAndPassReceiver();
  if (*g_input_device_manager_binder) {
    g_input_device_manager_binder->Run(std::move(receiver));
    return;
  }

  DCHECK(device_service_);
  device_service_->BindInputDeviceManager(std::move(receiver));
}

void HidDetectionManagerImpl::OnGetDevicesForIsRequired(
    base::OnceCallback<void(bool)> callback,
    std::vector<device::mojom::InputDeviceInfoPtr> devices) {
  bool has_pointer = false;
  bool has_keyboard = false;
  for (const auto& device : devices) {
    if (hid_detection::IsDevicePointer(*device))
      has_pointer = true;

    if (device->is_keyboard)
      has_keyboard = true;

    if (has_pointer && has_keyboard)
      break;
  }

  HID_LOG(EVENT)
      << "Fetched " << devices.size()
      << " input devices for GetIsHIdDetectionRequired(). Pointer detected: "
      << has_pointer << ", keyboard detected: " << has_keyboard;

  // HID detection is not required if both devices are present.
  std::move(callback).Run(!(has_pointer && has_keyboard));
}

void HidDetectionManagerImpl::OnGetDevicesAndSetClient(
    std::vector<device::mojom::InputDeviceInfoPtr> devices) {
  DCHECK(device_id_to_device_map_.empty())
      << " |devices_| should be empty when fetching initial devices.";
  for (auto& device : devices) {
    device_id_to_device_map_[device->id] = std::move(device);
  }
  SetConnectedHids();
  NotifyHidDetectionStatusChanged();
}

bool HidDetectionManagerImpl::SetConnectedHids() {
  bool is_any_device_newly_connected_hid = false;
  for (const auto& [device_id, device] : device_id_to_device_map_) {
    is_any_device_newly_connected_hid |=
        AttemptSetDeviceAsConnectedHid(*device);
  }
  return is_any_device_newly_connected_hid;
}

bool HidDetectionManagerImpl::AttemptSetDeviceAsConnectedHid(
    const device::mojom::InputDeviceInfo& device) {
  bool is_device_newly_connected_hid = false;
  if (!connected_touchscreen_id_.has_value() &&
      hid_detection::IsDeviceTouchscreen(device)) {
    HID_LOG(EVENT) << "Touchscreen detected: " << device.id;
    connected_touchscreen_id_ = device.id;
    is_device_newly_connected_hid = true;
  }

  // TODO(gordonseto): Handle keyboards/pointers.
  return is_device_newly_connected_hid;
}

}  // namespace ash::hid_detection
