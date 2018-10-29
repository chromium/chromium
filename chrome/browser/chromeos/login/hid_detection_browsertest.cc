// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>

#include "base/bind.h"
#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "base/memory/weak_ptr.h"
#include "chrome/browser/chromeos/login/oobe_screen.h"
#include "chrome/browser/chromeos/login/test/oobe_base_test.h"
#include "chrome/browser/chromeos/login/test/oobe_screen_waiter.h"
#include "content/public/browser/browser_thread.h"
#include "device/bluetooth/bluetooth_adapter_factory.h"
#include "device/bluetooth/test/mock_bluetooth_adapter.h"
#include "services/device/public/cpp/hid/fake_input_service_linux.h"
#include "services/device/public/mojom/constants.mojom.h"
#include "services/device/public/mojom/input_service.mojom.h"
#include "services/service_manager/public/cpp/service_context.h"
#include "testing/gmock/include/gmock/gmock.h"

using content::BrowserThread;
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

class HidDetectionTest : public OobeBaseTest {
 public:
  typedef device::mojom::InputDeviceInfoPtr InputDeviceInfoPtr;

  HidDetectionTest() : weak_ptr_factory_(this) {
    fake_input_service_manager_ =
        std::make_unique<device::FakeInputServiceLinux>();

    service_manager::ServiceContext::SetGlobalBinderForTesting(
        device::mojom::kServiceName, device::mojom::InputDeviceManager::Name_,
        base::Bind(&device::FakeInputServiceLinux::Bind,
                   base::Unretained(fake_input_service_manager_.get())));
  }

  ~HidDetectionTest() override {
    service_manager::ServiceContext::ClearGlobalBindersForTesting(
        device::mojom::kServiceName);
  }

  void SetUpInProcessBrowserTestFixture() override {
    OobeBaseTest::SetUpInProcessBrowserTestFixture();

    mock_adapter_ = new testing::NiceMock<device::MockBluetoothAdapter>();
    SetUpBluetoothMock(mock_adapter_, true);

    // Note: The SecureChannel service, which is never destroyed until the
    // browser process is killed, utilizes |mock_adapter_|.
    testing::Mock::AllowLeak(mock_adapter_.get());
  }

  void AddUsbMouse(const std::string& mouse_id) {
    auto mouse = device::mojom::InputDeviceInfo::New();
    mouse->id = mouse_id;
    mouse->subsystem = device::mojom::InputDeviceSubsystem::SUBSYSTEM_INPUT;
    mouse->type = device::mojom::InputDeviceType::TYPE_USB;
    mouse->is_mouse = true;
    fake_input_service_manager_->AddDevice(std::move(mouse));
  }

  void AddUsbKeyboard(const std::string& keyboard_id) {
    auto keyboard = device::mojom::InputDeviceInfo::New();
    keyboard->id = keyboard_id;
    keyboard->subsystem = device::mojom::InputDeviceSubsystem::SUBSYSTEM_INPUT;
    keyboard->type = device::mojom::InputDeviceType::TYPE_USB;
    keyboard->is_keyboard = true;
    fake_input_service_manager_->AddDevice(std::move(keyboard));
  }

 private:
  std::unique_ptr<device::FakeInputServiceLinux> fake_input_service_manager_;
  scoped_refptr<testing::NiceMock<device::MockBluetoothAdapter>> mock_adapter_;

  base::WeakPtrFactory<HidDetectionTest> weak_ptr_factory_;

  DISALLOW_COPY_AND_ASSIGN(HidDetectionTest);
};

class HidDetectionSkipTest : public HidDetectionTest {
 public:
  HidDetectionSkipTest() {
    AddUsbMouse("mouse");
    AddUsbKeyboard("keyboard");
  }

  void SetUpOnMainThread() override { HidDetectionTest::SetUpOnMainThread(); }
};

IN_PROC_BROWSER_TEST_F(HidDetectionTest, NoDevicesConnected) {
  OobeScreenWaiter(OobeScreen::SCREEN_OOBE_HID_DETECTION).Wait();
}

IN_PROC_BROWSER_TEST_F(HidDetectionSkipTest, BothDevicesPreConnected) {
  OobeScreenWaiter(OobeScreen::SCREEN_OOBE_WELCOME).Wait();
}

}  // namespace chromeos
