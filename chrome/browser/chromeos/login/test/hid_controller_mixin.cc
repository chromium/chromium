// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/login/test/hid_controller_mixin.h"

#include <utility>

#include "base/bind.h"
#include "base/callback_helpers.h"
#include "chrome/browser/chromeos/login/screens/hid_detection_screen.h"
#include "device/bluetooth/bluetooth_adapter_factory.h"
#include "services/device/public/cpp/hid/fake_input_service_linux.h"
#include "services/device/public/mojom/input_service.mojom.h"

using testing::_;

namespace {

void SetUpBluetoothMock(
    scoped_refptr<testing::NiceMock<device::MockBluetoothAdapter>> mock_adapter,
    bool is_present) {
  device::BluetoothAdapterFactory::SetAdapterForTesting(mock_adapter);

  EXPECT_CALL(*mock_adapter, IsPresent())
      .WillRepeatedly(testing::Return(is_present));

  EXPECT_CALL(*mock_adapter, IsPowered()).WillRepeatedly(testing::Return(true));
  EXPECT_CALL(*mock_adapter, GetDevices())
      .WillRepeatedly(
          testing::Return(device::BluetoothAdapter::ConstDeviceList()));
}

}  // namespace

namespace chromeos {
namespace test {

using InputDeviceInfoPtr = device::mojom::InputDeviceInfoPtr;

// static
const char HIDControllerMixin::kMouseId[] = "mouse";
const char HIDControllerMixin::kKeyboardId[] = "keyboard";

HIDControllerMixin::HIDControllerMixin(InProcessBrowserTestMixinHost* host)
    : InProcessBrowserTestMixin(host) {
  fake_input_service_manager_ =
      std::make_unique<device::FakeInputServiceLinux>();

  HIDDetectionScreen::OverrideInputDeviceManagerBinderForTesting(
      base::BindRepeating(&device::FakeInputServiceLinux::Bind,
                          base::Unretained(fake_input_service_manager_.get())));
}

HIDControllerMixin::~HIDControllerMixin() {
  HIDDetectionScreen::OverrideInputDeviceManagerBinderForTesting(
      base::NullCallback());
}

void HIDControllerMixin::SetUpInProcessBrowserTestFixture() {
  mock_adapter_ = new testing::NiceMock<device::MockBluetoothAdapter>();
  SetUpBluetoothMock(mock_adapter_, true);

  // Note: The SecureChannel service, which is never destroyed until the
  // browser process is killed, utilizes `mock_adapter_`.
  testing::Mock::AllowLeak(mock_adapter_.get());
}

void HIDControllerMixin::AddUsbMouse(const std::string& mouse_id) {
  auto mouse = device::mojom::InputDeviceInfo::New();
  mouse->id = mouse_id;
  mouse->subsystem = device::mojom::InputDeviceSubsystem::SUBSYSTEM_INPUT;
  mouse->type = device::mojom::InputDeviceType::TYPE_USB;
  mouse->is_mouse = true;
  fake_input_service_manager_->AddDevice(std::move(mouse));
}

void HIDControllerMixin::AddUsbKeyboard(const std::string& keyboard_id) {
  auto keyboard = device::mojom::InputDeviceInfo::New();
  keyboard->id = keyboard_id;
  keyboard->subsystem = device::mojom::InputDeviceSubsystem::SUBSYSTEM_INPUT;
  keyboard->type = device::mojom::InputDeviceType::TYPE_USB;
  keyboard->is_keyboard = true;
  fake_input_service_manager_->AddDevice(std::move(keyboard));
}

}  // namespace test
}  // namespace chromeos
