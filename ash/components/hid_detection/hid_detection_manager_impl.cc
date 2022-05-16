// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/components/hid_detection/hid_detection_manager_impl.h"

#include "ash/components/hid_detection/hid_detection_utils.h"
#include "base/no_destructor.h"
#include "components/device_event_log/device_event_log.h"

namespace ash::hid_detection {
namespace {
// Global InputDeviceManagerBinder instance that can be overriden in tests.
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

}  // namespace ash::hid_detection
