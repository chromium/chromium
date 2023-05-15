// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>

#include "base/command_line.h"
#include "base/memory/ptr_util.h"
#include "base/memory/raw_ptr.h"
#include "chrome/browser/apps/platform_apps/app_browsertest_util.h"
#include "chrome/browser/ash/app_mode/kiosk_app_manager.h"
#include "chrome/browser/ash/login/users/fake_chrome_user_manager.h"
#include "chrome/browser/ash/ownership/fake_owner_settings_service.h"
#include "chrome/browser/ash/settings/scoped_cros_settings_test_helper.h"
#include "chrome/browser/extensions/extension_apitest.h"
#include "components/account_id/account_id.h"
#include "components/user_manager/scoped_user_manager.h"
#include "content/public/test/browser_test.h"
#include "extensions/common/switches.h"
#include "testing/gmock/include/gmock/gmock.h"

namespace {
const char kTestingAppId[] = "pjdjhejcdkeebjehnokfbfnjmgmgdjlc";
}  // namespace

namespace extensions {
namespace {

// This class contains chrome.bluetoothLowEnergy API tests.
class BluetoothLowEnergyApiTestChromeOs : public PlatformAppBrowserTest {
 public:
  BluetoothLowEnergyApiTestChromeOs()
      : fake_user_manager_(nullptr), settings_helper_(false) {}
  ~BluetoothLowEnergyApiTestChromeOs() override {}

  void SetUpOnMainThread() override {
    PlatformAppBrowserTest::SetUpOnMainThread();
    settings_helper_.ReplaceDeviceSettingsProviderWithStub();
    owner_settings_service_ =
        settings_helper_.CreateOwnerSettingsService(browser()->profile());
  }

  void TearDownOnMainThread() override {
    owner_settings_service_.reset();
    settings_helper_.RestoreRealDeviceSettingsProvider();
    PlatformAppBrowserTest::TearDownOnMainThread();
    user_manager_enabler_.reset();
    fake_user_manager_ = nullptr;
  }

 protected:
  void EnterKioskSession() {
    fake_user_manager_ = new ash::FakeChromeUserManager();
    user_manager_enabler_ = std::make_unique<user_manager::ScopedUserManager>(
        base::WrapUnique(fake_user_manager_.get()));

    const AccountId kiosk_account_id(
        AccountId::FromUserEmail("kiosk@foobar.com"));
    fake_user_manager_->AddKioskAppUser(kiosk_account_id);
    fake_user_manager_->LoginUser(kiosk_account_id);
  }

  void SetAutoLaunchApp() {
    manager()->AddApp(kTestingAppId, owner_settings_service_.get());
    manager()->SetAutoLaunchApp(kTestingAppId, owner_settings_service_.get());
    manager()->SetAppWasAutoLaunchedWithZeroDelay(kTestingAppId);
  }

  ash::KioskAppManager* manager() const { return ash::KioskAppManager::Get(); }

  raw_ptr<ash::FakeChromeUserManager, ExperimentalAsh> fake_user_manager_;
  std::unique_ptr<user_manager::ScopedUserManager> user_manager_enabler_;

  ash::ScopedCrosSettingsTestHelper settings_helper_;
  std::unique_ptr<ash::FakeOwnerSettingsService> owner_settings_service_;
};

IN_PROC_BROWSER_TEST_F(BluetoothLowEnergyApiTestChromeOs,
                       RegisterAdvertisement_Flag) {
  base::CommandLine::ForCurrentProcess()->AppendSwitch(
      switches::kEnableBLEAdvertising);
  ASSERT_TRUE(RunExtensionTest(
      "api_test/bluetooth_low_energy/register_advertisement_flag",
      {.launch_as_platform_app = true}))
      << message_;
}

IN_PROC_BROWSER_TEST_F(BluetoothLowEnergyApiTestChromeOs,
                       RegisterAdvertisement_NotKioskSession) {
  ASSERT_TRUE(RunExtensionTest(
      "api_test/bluetooth_low_energy/register_advertisement_no_kiosk_mode",
      {.launch_as_platform_app = true}))
      << message_;
}

IN_PROC_BROWSER_TEST_F(BluetoothLowEnergyApiTestChromeOs,
                       RegisterAdvertisement_KioskSessionOnly) {
  EnterKioskSession();
  ASSERT_TRUE(
      RunExtensionTest("api_test/bluetooth_low_energy/"
                       "register_advertisement_kiosk_session_only",
                       {.launch_as_platform_app = true}))
      << message_;
}

IN_PROC_BROWSER_TEST_F(BluetoothLowEnergyApiTestChromeOs,
                       RegisterAdvertisement) {
  EnterKioskSession();
  SetAutoLaunchApp();
  ASSERT_TRUE(
      RunExtensionTest("api_test/bluetooth_low_energy/register_advertisement",
                       {.launch_as_platform_app = true}))
      << message_;
}

IN_PROC_BROWSER_TEST_F(BluetoothLowEnergyApiTestChromeOs, ResetAdvertising) {
  EnterKioskSession();
  SetAutoLaunchApp();
  ASSERT_TRUE(
      RunExtensionTest("api_test/bluetooth_low_energy/reset_all_advertisements",
                       {.launch_as_platform_app = true}))
      << message_;
}

IN_PROC_BROWSER_TEST_F(BluetoothLowEnergyApiTestChromeOs,
                       SetAdvertisingInterval) {
  EnterKioskSession();
  SetAutoLaunchApp();
  ASSERT_TRUE(
      RunExtensionTest("api_test/bluetooth_low_energy/"
                       "set_advertising_interval",
                       {.launch_as_platform_app = true}))
      << message_;
}

IN_PROC_BROWSER_TEST_F(BluetoothLowEnergyApiTestChromeOs, CreateService) {
  EnterKioskSession();
  SetAutoLaunchApp();
  ASSERT_TRUE(
      RunExtensionTest("api_test/bluetooth_low_energy/"
                       "create_service",
                       {.launch_as_platform_app = true}))
      << message_;
}

IN_PROC_BROWSER_TEST_F(BluetoothLowEnergyApiTestChromeOs, CreateService_Flag) {
  base::CommandLine::ForCurrentProcess()->AppendSwitch(
      switches::kEnableBLEAdvertising);
  ASSERT_TRUE(
      RunExtensionTest("api_test/bluetooth_low_energy/create_service_flag",
                       {.launch_as_platform_app = true}))
      << message_;
}

IN_PROC_BROWSER_TEST_F(BluetoothLowEnergyApiTestChromeOs,
                       CreateService_NotKioskSession) {
  ASSERT_TRUE(RunExtensionTest(
      "api_test/bluetooth_low_energy/create_service_no_kiosk_mode",
      {.launch_as_platform_app = true}))
      << message_;
}

IN_PROC_BROWSER_TEST_F(BluetoothLowEnergyApiTestChromeOs,
                       CreateService_KioskSessionOnly) {
  EnterKioskSession();
  ASSERT_TRUE(
      RunExtensionTest("api_test/bluetooth_low_energy/"
                       "create_service_kiosk_session_only",
                       {.launch_as_platform_app = true}))
      << message_;
}

IN_PROC_BROWSER_TEST_F(BluetoothLowEnergyApiTestChromeOs,
                       CreateCharacteristic) {
  EnterKioskSession();
  SetAutoLaunchApp();
  ASSERT_TRUE(
      RunExtensionTest("api_test/bluetooth_low_energy/"
                       "create_characteristic",
                       {.launch_as_platform_app = true}))
      << message_;
}

IN_PROC_BROWSER_TEST_F(BluetoothLowEnergyApiTestChromeOs, CreateDescriptor) {
  EnterKioskSession();
  SetAutoLaunchApp();
  ASSERT_TRUE(
      RunExtensionTest("api_test/bluetooth_low_energy/"
                       "create_descriptor",
                       {.launch_as_platform_app = true}))
      << message_;
}

IN_PROC_BROWSER_TEST_F(BluetoothLowEnergyApiTestChromeOs, RegisterService) {
  EnterKioskSession();
  SetAutoLaunchApp();
  ASSERT_TRUE(
      RunExtensionTest("api_test/bluetooth_low_energy/"
                       "register_service",
                       {.launch_as_platform_app = true}))
      << message_;
}

IN_PROC_BROWSER_TEST_F(BluetoothLowEnergyApiTestChromeOs, UnregisterService) {
  EnterKioskSession();
  SetAutoLaunchApp();
  ASSERT_TRUE(
      RunExtensionTest("api_test/bluetooth_low_energy/"
                       "unregister_service",
                       {.launch_as_platform_app = true}))
      << message_;
}

IN_PROC_BROWSER_TEST_F(BluetoothLowEnergyApiTestChromeOs, RemoveService) {
  EnterKioskSession();
  SetAutoLaunchApp();
  ASSERT_TRUE(
      RunExtensionTest("api_test/bluetooth_low_energy/"
                       "remove_service",
                       {.launch_as_platform_app = true}))
      << message_;
}

IN_PROC_BROWSER_TEST_F(BluetoothLowEnergyApiTestChromeOs,
                       NotifyCharacteristicValueChanged) {
  EnterKioskSession();
  SetAutoLaunchApp();
  ASSERT_TRUE(
      RunExtensionTest("api_test/bluetooth_low_energy/"
                       "notify_characteristic_value_changed",
                       {.launch_as_platform_app = true}))
      << message_;
}

IN_PROC_BROWSER_TEST_F(BluetoothLowEnergyApiTestChromeOs,
                       NotifyCharacteristicValueChanged_ErrorConditions) {
  EnterKioskSession();
  SetAutoLaunchApp();
  ASSERT_TRUE(
      RunExtensionTest("api_test/bluetooth_low_energy/"
                       "notify_characteristic_value_changed_error_conditions",
                       {.launch_as_platform_app = true}))
      << message_;
}

// TODO(rkc): Figure out how to integrate with BluetoothTestBlueZ and write
// comprehensive tests for GATT server events. See http://crbug.com/607395 for
// details.

}  // namespace
}  // namespace extensions
