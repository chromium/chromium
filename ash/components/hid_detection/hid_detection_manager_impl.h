// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_COMPONENTS_HID_DETECTION_HID_DETECTION_MANAGER_IMPL_H_
#define ASH_COMPONENTS_HID_DETECTION_HID_DETECTION_MANAGER_IMPL_H_

#include "ash/components/hid_detection/hid_detection_manager.h"

#include "base/callback.h"
#include "base/memory/weak_ptr.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "services/device/public/mojom/device_service.mojom.h"
#include "services/device/public/mojom/input_service.mojom.h"

namespace ash::hid_detection {

// Concrete HidDetectionManager implementation that uses InputDeviceManager and
// BluetoothHidDetectorImpl to detect and connect with devices.
class HidDetectionManagerImpl : public HidDetectionManager {
 public:
  using InputDeviceManagerBinder = base::RepeatingCallback<void(
      mojo::PendingReceiver<device::mojom::InputDeviceManager>)>;

  // Allows tests to override how this class binds InputDeviceManager receivers.
  static void SetInputDeviceManagerBinderForTest(
      InputDeviceManagerBinder binder);

  explicit HidDetectionManagerImpl(
      device::mojom::DeviceService* device_service);
  ~HidDetectionManagerImpl() override;

 private:
  // HidDetectionManager:
  void GetIsHidDetectionRequired(
      base::OnceCallback<void(bool)> callback) override;

  void BindToInputDeviceManagerIfNeeded();

  // Processes the list of input devices fetched by GetIsHidDetectionRequired().
  // Invokes |callback| with whether HID detection is required or not. The
  // returned device list is not saved.
  void OnGetDevicesForIsRequired(
      base::OnceCallback<void(bool)> callback,
      std::vector<device::mojom::InputDeviceInfoPtr> devices);

  device::mojom::DeviceService* device_service_ = nullptr;
  mojo::Remote<device::mojom::InputDeviceManager> input_device_manager_;

  base::WeakPtrFactory<HidDetectionManagerImpl> weak_ptr_factory_{this};
};

}  // namespace ash::hid_detection

#endif  // ASH_COMPONENTS_HID_DETECTION_HID_DETECTION_MANAGER_IMPL_H_
