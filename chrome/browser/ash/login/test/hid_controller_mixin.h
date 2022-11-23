// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_LOGIN_TEST_HID_CONTROLLER_MIXIN_H_
#define CHROME_BROWSER_ASH_LOGIN_TEST_HID_CONTROLLER_MIXIN_H_

#include <memory>

#include "base/memory/ref_counted.h"
#include "base/memory/weak_ptr.h"
#include "chrome/test/base/mixin_based_in_process_browser_test.h"
#include "device/bluetooth/test/mock_bluetooth_adapter.h"
#include "services/device/public/mojom/input_service.mojom.h"
#include "testing/gmock/include/gmock/gmock.h"

namespace device {
class FakeInputServiceLinux;
}

namespace ash {
namespace test {

// This test mixin allows to control presence of Human Input Devices during
// OOBE. Just adding this mixin would result in "ho HID available" case,
// and it can be changed using AddUsbMouse / AddUsbKeyboard methods.
class HIDControllerMixin : public InProcessBrowserTestMixin {
 public:
  static const char kMouseId[];
  static const char kKeyboardId[];
  static const char kTouchscreenId[];

  explicit HIDControllerMixin(InProcessBrowserTestMixinHost* host);

  HIDControllerMixin(const HIDControllerMixin&) = delete;
  HIDControllerMixin& operator=(const HIDControllerMixin&) = delete;

  ~HIDControllerMixin() override;

  void AddMouse(device::mojom::InputDeviceType type);
  void AddKeyboard(device::mojom::InputDeviceType type);
  void AddTouchscreen();
  void RemoveMouse();
  void RemoveKeyboard();
  void ConnectUSBDevices();
  void ConnectBTDevices();
  void RemoveDevices();
  void SetUpInProcessBrowserTestFixture() override;
  void set_wait_until_idle_after_device_update(bool value) {
    wait_until_idle_after_device_update_ = value;
  }
  scoped_refptr<testing::NiceMock<device::MockBluetoothAdapter>>
  mock_bluetooth_adapter() {
    return mock_adapter_;
  }

 private:
  bool wait_until_idle_after_device_update_ = true;
  std::unique_ptr<device::FakeInputServiceLinux> fake_input_service_manager_;
  scoped_refptr<testing::NiceMock<device::MockBluetoothAdapter>> mock_adapter_;
  base::WeakPtrFactory<HIDControllerMixin> weak_ptr_factory_{this};
};

}  // namespace test
}  // namespace ash

#endif  // CHROME_BROWSER_ASH_LOGIN_TEST_HID_CONTROLLER_MIXIN_H_
