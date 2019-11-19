// Copyright (c) 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/bind.h"
#include "base/macros.h"
#include "base/run_loop.h"
#include "base/system/sys_info.h"
#include "chrome/browser/chromeos/login/login_wizard.h"
#include "chrome/browser/chromeos/login/screens/base_screen.h"
#include "chrome/browser/chromeos/login/screens/hid_detection_screen.h"
#include "chrome/browser/chromeos/login/test/oobe_screen_waiter.h"
#include "chrome/browser/chromeos/login/wizard_controller.h"
#include "chrome/browser/ui/webui/chromeos/login/hid_detection_screen_handler.h"
#include "chrome/browser/ui/webui/chromeos/login/welcome_screen_handler.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chromeos/constants/chromeos_switches.h"
#include "services/device/public/cpp/hid/fake_input_service_linux.h"
#include "services/device/public/mojom/constants.mojom.h"
#include "services/device/public/mojom/input_service.mojom.h"
#include "services/service_manager/public/cpp/service_binding.h"

namespace chromeos {

class HIDDetectionScreenTest : public InProcessBrowserTest {
 public:
  HIDDetectionScreenTest() {
    // HID detection screen only appears for Chromebases, Chromebits, and
    // Chromeboxes.
    base::SysInfo::SetChromeOSVersionInfoForTest("DEVICETYPE=CHROMEBOX",
                                                 base::Time::Now());

    fake_input_service_manager_ =
        std::make_unique<device::FakeInputServiceLinux>();

    service_manager::ServiceBinding::OverrideInterfaceBinderForTesting(
        device::mojom::kServiceName,
        base::Bind(&device::FakeInputServiceLinux::Bind,
                   base::Unretained(fake_input_service_manager_.get())));
  }

  ~HIDDetectionScreenTest() override {
    service_manager::ServiceBinding::ClearInterfaceBinderOverrideForTesting<
        device::mojom::InputDeviceManager>(device::mojom::kServiceName);
  }

  // InProcessBrowserTest:
  void SetUpCommandLine(base::CommandLine* command_line) override {
    InProcessBrowserTest::SetUpCommandLine(command_line);
    command_line->AppendArg(switches::kLoginManager);
  }

  void SetUpOnMainThread() override {
    ShowLoginWizard(HIDDetectionView::kScreenId);
    ASSERT_TRUE(WizardController::default_controller());

    hid_detection_screen_ = static_cast<HIDDetectionScreen*>(
        WizardController::default_controller()->GetScreen(
            HIDDetectionView::kScreenId));
    ASSERT_TRUE(hid_detection_screen_);
    ASSERT_EQ(WizardController::default_controller()->current_screen(),
              hid_detection_screen_);
    ASSERT_TRUE(hid_detection_screen_->view_);

    hid_detection_screen()->SetAdapterInitialPoweredForTesting(false);
  }

  HIDDetectionScreen* hid_detection_screen() { return hid_detection_screen_; }
  HIDDetectionScreenHandler* handler() {
    return static_cast<HIDDetectionScreenHandler*>(
        hid_detection_screen()->view_);
  }

  scoped_refptr<device::BluetoothAdapter> adapter() {
    return hid_detection_screen_->GetAdapterForTesting();
  }

  void AddDeviceToService(bool is_mouse, device::mojom::InputDeviceType type) {
    auto device = device::mojom::InputDeviceInfo::New();
    device->id = is_mouse ? "mouse" : "keyboard";
    device->subsystem = device::mojom::InputDeviceSubsystem::SUBSYSTEM_INPUT;
    device->type = type;
    device->is_mouse = is_mouse;
    device->is_keyboard = !is_mouse;
    fake_input_service_manager_->AddDevice(std::move(device));
    base::RunLoop().RunUntilIdle();
  }

  void RemoveDeviceFromService(bool is_mouse) {
    std::string id = is_mouse ? "mouse" : "keyboard";
    fake_input_service_manager_->RemoveDevice(std::move(id));
    base::RunLoop().RunUntilIdle();
  }

 private:
  HIDDetectionScreen* hid_detection_screen_;
  std::unique_ptr<device::FakeInputServiceLinux> fake_input_service_manager_;

  DISALLOW_COPY_AND_ASSIGN(HIDDetectionScreenTest);
};

IN_PROC_BROWSER_TEST_F(HIDDetectionScreenTest, HIDDetectionScreenNotAllowed) {
  // Set device type to one that should not invoke HIDDetectionScreen logic.
  base::SysInfo::SetChromeOSVersionInfoForTest("DEVICETYPE=CHROMEBOOK",
                                               base::Time::Now());

  ShowLoginWizard(WelcomeView::kScreenId);
  ASSERT_TRUE(WizardController::default_controller());

  EXPECT_FALSE(WizardController::default_controller()->HasScreen(
      HIDDetectionView::kScreenId));
}

IN_PROC_BROWSER_TEST_F(HIDDetectionScreenTest, MouseKeyboardStates) {
  // NOTE: State strings match those in hid_detection_screen.cc.
  // No devices added yet
  EXPECT_EQ("searching", handler()->mouse_state_for_test());
  EXPECT_EQ("searching", handler()->keyboard_state_for_test());
  EXPECT_FALSE(handler()->continue_button_enabled_for_test());

  // Generic connection types. Unlike the pointing device, which may be a tablet
  // or touchscreen, the keyboard only reports usb and bluetooth states.
  AddDeviceToService(true, device::mojom::InputDeviceType::TYPE_SERIO);
  EXPECT_TRUE(handler()->continue_button_enabled_for_test());

  AddDeviceToService(false, device::mojom::InputDeviceType::TYPE_SERIO);
  EXPECT_EQ("connected", handler()->mouse_state_for_test());
  EXPECT_EQ("usb", handler()->keyboard_state_for_test());
  EXPECT_TRUE(handler()->continue_button_enabled_for_test());

  // Remove generic devices, add usb devices.
  RemoveDeviceFromService(true);
  RemoveDeviceFromService(false);
  EXPECT_FALSE(handler()->continue_button_enabled_for_test());

  AddDeviceToService(true, device::mojom::InputDeviceType::TYPE_USB);
  AddDeviceToService(false, device::mojom::InputDeviceType::TYPE_USB);
  EXPECT_EQ("usb", handler()->mouse_state_for_test());
  EXPECT_EQ("usb", handler()->keyboard_state_for_test());
  EXPECT_TRUE(handler()->continue_button_enabled_for_test());

  // Remove usb devices, add bluetooth devices.
  RemoveDeviceFromService(true);
  RemoveDeviceFromService(false);
  EXPECT_FALSE(handler()->continue_button_enabled_for_test());

  AddDeviceToService(true, device::mojom::InputDeviceType::TYPE_BLUETOOTH);
  AddDeviceToService(false, device::mojom::InputDeviceType::TYPE_BLUETOOTH);
  EXPECT_EQ("paired", handler()->mouse_state_for_test());
  EXPECT_EQ("paired", handler()->keyboard_state_for_test());
  EXPECT_TRUE(handler()->continue_button_enabled_for_test());
}

// Test that if there is any Bluetooth device connected on HID screen, the
// Bluetooth adapter should not be disabled after advancing to the next screen.
IN_PROC_BROWSER_TEST_F(HIDDetectionScreenTest, BluetoothDeviceConnected) {
  OobeScreenWaiter(HIDDetectionView::kScreenId).Wait();
  EXPECT_TRUE(adapter()->IsPowered());

  // Add a pair of USB mouse/keyboard so that |pointing_device_connect_type_|
  // and |keyboard_device_connect_type_| are
  // device::mojom::InputDeviceType::TYPE_USB.
  AddDeviceToService(true, device::mojom::InputDeviceType::TYPE_USB);
  AddDeviceToService(false, device::mojom::InputDeviceType::TYPE_USB);

  // Add another pair of Bluetooth mouse/keyboard.
  AddDeviceToService(true, device::mojom::InputDeviceType::TYPE_BLUETOOTH);
  AddDeviceToService(false, device::mojom::InputDeviceType::TYPE_BLUETOOTH);

  // Simulate the user's click on "Continue" button.
  hid_detection_screen()->OnContinueButtonClicked();
  OobeScreenWaiter(WelcomeView::kScreenId).Wait();

  // The adapter should not be powered off at this moment.
  EXPECT_TRUE(adapter()->IsPowered());
}

// Test that if there is no Bluetooth device connected on HID screen, the
// Bluetooth adapter should be disabled after advancing to the next screen.
IN_PROC_BROWSER_TEST_F(HIDDetectionScreenTest, NoBluetoothDeviceConnected) {
  OobeScreenWaiter(HIDDetectionView::kScreenId).Wait();
  EXPECT_TRUE(adapter()->IsPowered());

  AddDeviceToService(true, device::mojom::InputDeviceType::TYPE_USB);
  AddDeviceToService(false, device::mojom::InputDeviceType::TYPE_USB);

  // Simulate the user's click on "Continue" button.
  hid_detection_screen()->OnContinueButtonClicked();
  OobeScreenWaiter(WelcomeView::kScreenId).Wait();

  // The adapter should be powered off at this moment.
  EXPECT_FALSE(adapter()->IsPowered());
}

}  // namespace chromeos
