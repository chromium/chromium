// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/policy/handlers/device_dock_mac_address_source_handler.h"

#include <memory>
#include <string>
#include <tuple>

#include "chrome/browser/ash/settings/scoped_cros_settings_test_helper.h"
#include "chromeos/ash/components/network/mock_network_device_handler.h"
#include "chromeos/ash/components/network/network_device_handler.h"
#include "chromeos/ash/components/settings/cros_settings.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace policy {

class DeviceDockMacAddressHandlerBaseTest : public testing::Test {
 public:
  DeviceDockMacAddressHandlerBaseTest() {
    scoped_cros_settings_test_helper_.ReplaceDeviceSettingsProviderWithStub();
    scoped_cros_settings_test_helper_.SetTrustedStatus(
        ash::CrosSettingsProvider::TEMPORARILY_UNTRUSTED);

    device_dock_mac_address_handler_ =
        std::make_unique<DeviceDockMacAddressHandler>(
            ash::CrosSettings::Get(), &network_device_handler_mock_);
  }

  DeviceDockMacAddressHandlerBaseTest(
      const DeviceDockMacAddressHandlerBaseTest&) = delete;
  DeviceDockMacAddressHandlerBaseTest& operator=(
      const DeviceDockMacAddressHandlerBaseTest&) = delete;

 protected:
  void MakeCrosSettingsTrusted() {
    scoped_cros_settings_test_helper_.SetTrustedStatus(
        ash::CrosSettingsProvider::TRUSTED);
  }

  ash::ScopedCrosSettingsTestHelper scoped_cros_settings_test_helper_;

  testing::StrictMock<ash::MockNetworkDeviceHandler>
      network_device_handler_mock_;

  std::unique_ptr<DeviceDockMacAddressHandler> device_dock_mac_address_handler_;
};

// Tests that DockMacAddressHandler changes device handler property to
// "usb_adapter_mac" if received policy is invalid.
TEST_F(DeviceDockMacAddressHandlerBaseTest, InvalidPolicyValue) {
  MakeCrosSettingsTrusted();
  EXPECT_CALL(network_device_handler_mock_,
              SetUsbEthernetMacAddressSource("usb_adapter_mac"));
  scoped_cros_settings_test_helper_.SetInteger(ash::kDeviceDockMacAddressSource,
                                               4);
}

class DeviceDockMacAddressHandlerTest
    : public DeviceDockMacAddressHandlerBaseTest,
      public testing::WithParamInterface<std::tuple<int, std::string>> {
 public:
  DeviceDockMacAddressHandlerTest() = default;

  DeviceDockMacAddressHandlerTest(const DeviceDockMacAddressHandlerTest&) =
      delete;
  DeviceDockMacAddressHandlerTest& operator=(
      const DeviceDockMacAddressHandlerTest&) = delete;
};

// Tests that DockMacAddressHandler does not change device handler property if
// CrosSettings are untrusted.
TEST_P(DeviceDockMacAddressHandlerTest, OnPolicyChangeUntrusted) {
  scoped_cros_settings_test_helper_.SetInteger(ash::kDeviceDockMacAddressSource,
                                               std::get<0>(GetParam()));
}

// Tests that DockMacAddressHandler changes device handler property if
// CrosSetting are trusted.
TEST_P(DeviceDockMacAddressHandlerTest, OnPolicyChange) {
  MakeCrosSettingsTrusted();
  EXPECT_CALL(network_device_handler_mock_,
              SetUsbEthernetMacAddressSource(std::get<1>(GetParam())));
  scoped_cros_settings_test_helper_.SetInteger(ash::kDeviceDockMacAddressSource,
                                               std::get<0>(GetParam()));
}

INSTANTIATE_TEST_SUITE_P(
    All,
    DeviceDockMacAddressHandlerTest,
    testing::Values(std::make_tuple(1, "designated_dock_mac"),
                    std::make_tuple(2, "builtin_adapter_mac"),
                    std::make_tuple(3, "usb_adapter_mac")));

}  // namespace policy
