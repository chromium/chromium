// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/login/test/hid_controller_mixin.h"

#include <utility>

#include "base/bind.h"
#include "base/callback_helpers.h"
#include "chrome/browser/ash/login/screens/hid_detection_screen.h"
#include "device/bluetooth/bluetooth_adapter_factory.h"
#include "services/device/public/cpp/hid/fake_input_service_linux.h"

namespace ash {
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

namespace test {

using InputDeviceInfoPtr = device::mojom::InputDeviceInfoPtr;

// static
const char HIDControllerMixin::kMouseId[] = "mouse";
const char HIDControllerMixin::kKeyboardId[] = "keyboard";
const char HIDControllerMixin::kTouchscreenId[] = "touchscreen";

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

void HIDControllerMixin::AddMouse(device::mojom::InputDeviceType type) {
  auto mouse = device::mojom::InputDeviceInfo::New();
  mouse->id = kMouseId;
  mouse->subsystem = device::mojom::InputDeviceSubsystem::SUBSYSTEM_INPUT;
  mouse->type = type;
  mouse->is_mouse = true;
  mouse->name = "pointer";
  fake_input_service_manager_->AddDevice(std::move(mouse));
  if (wait_until_idle_after_device_update_)
    base::RunLoop().RunUntilIdle();
}

void HIDControllerMixin::AddKeyboard(device::mojom::InputDeviceType type) {
  auto keyboard = device::mojom::InputDeviceInfo::New();
  keyboard->id = kKeyboardId;
  keyboard->subsystem = device::mojom::InputDeviceSubsystem::SUBSYSTEM_INPUT;
  keyboard->type = type;
  keyboard->is_keyboard = true;
  keyboard->name = "keyboard";
  fake_input_service_manager_->AddDevice(std::move(keyboard));
  if (wait_until_idle_after_device_update_)
    base::RunLoop().RunUntilIdle();
}

void HIDControllerMixin::AddTouchscreen() {
  auto touchscreen = device::mojom::InputDeviceInfo::New();
  touchscreen->id = kTouchscreenId;
  touchscreen->subsystem = device::mojom::InputDeviceSubsystem::SUBSYSTEM_INPUT;
  touchscreen->type = device::mojom::InputDeviceType::TYPE_UNKNOWN;
  touchscreen->is_touchscreen = true;
  fake_input_service_manager_->AddDevice(std::move(touchscreen));
  if (wait_until_idle_after_device_update_)
    base::RunLoop().RunUntilIdle();
}

void HIDControllerMixin::ConnectUSBDevices() {
  AddMouse(device::mojom::InputDeviceType::TYPE_USB);
  AddKeyboard(device::mojom::InputDeviceType::TYPE_USB);
}

void HIDControllerMixin::ConnectBTDevices() {
  AddMouse(device::mojom::InputDeviceType::TYPE_BLUETOOTH);
  AddKeyboard(device::mojom::InputDeviceType::TYPE_BLUETOOTH);
}

void HIDControllerMixin::RemoveMouse() {
  fake_input_service_manager_->RemoveDevice(kMouseId);
  if (wait_until_idle_after_device_update_)
    base::RunLoop().RunUntilIdle();
}

void HIDControllerMixin::RemoveKeyboard() {
  fake_input_service_manager_->RemoveDevice(kKeyboardId);
  if (wait_until_idle_after_device_update_)
    base::RunLoop().RunUntilIdle();
}

void HIDControllerMixin::RemoveDevices() {
  RemoveMouse();
  RemoveKeyboard();
}

}  // namespace test
}  // namespace ash
