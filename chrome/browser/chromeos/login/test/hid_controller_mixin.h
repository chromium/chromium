// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_LOGIN_TEST_HID_CONTROLLER_MIXIN_H_
#define CHROME_BROWSER_CHROMEOS_LOGIN_TEST_HID_CONTROLLER_MIXIN_H_

#include <memory>
#include <string>

#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "base/memory/weak_ptr.h"
#include "chrome/test/base/mixin_based_in_process_browser_test.h"
#include "device/bluetooth/test/mock_bluetooth_adapter.h"
#include "testing/gmock/include/gmock/gmock.h"

namespace device {
class FakeInputServiceLinux;
}

namespace chromeos {
namespace test {

// This test mixin allows to control presence of Human Input Devices during
// OOBE. Just adding this mixin would result in "ho HID available" case,
// and it can be changed using AddUsbMouse / AddUsbKeyboard methods.
class HIDControllerMixin : public InProcessBrowserTestMixin {
 public:
  static const char kMouseId[];
  static const char kKeyboardId[];

  explicit HIDControllerMixin(InProcessBrowserTestMixinHost* host);
  ~HIDControllerMixin() override;

  void AddUsbMouse(const std::string& mouse_id);
  void AddUsbKeyboard(const std::string& keyboard_id);

  void SetUpInProcessBrowserTestFixture() override;

 private:
  std::unique_ptr<device::FakeInputServiceLinux> fake_input_service_manager_;
  scoped_refptr<testing::NiceMock<device::MockBluetoothAdapter>> mock_adapter_;

  base::WeakPtrFactory<HIDControllerMixin> weak_ptr_factory_{this};

  DISALLOW_COPY_AND_ASSIGN(HIDControllerMixin);
};

}  // namespace test
}  // namespace chromeos

#endif  // CHROME_BROWSER_CHROMEOS_LOGIN_TEST_HID_CONTROLLER_MIXIN_H_
