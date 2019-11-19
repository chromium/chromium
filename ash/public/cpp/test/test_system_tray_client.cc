// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/public/cpp/test/test_system_tray_client.h"

namespace ash {

TestSystemTrayClient::TestSystemTrayClient() = default;

TestSystemTrayClient::~TestSystemTrayClient() = default;

void TestSystemTrayClient::ShowSettings() {}

void TestSystemTrayClient::ShowBluetoothSettings() {
  show_bluetooth_settings_count_++;
}

void TestSystemTrayClient::ShowBluetoothPairingDialog(
    const std::string& address,
    const base::string16& name_for_display,
    bool paired,
    bool connected) {}

void TestSystemTrayClient::ShowDateSettings() {}

void TestSystemTrayClient::ShowSetTimeDialog() {}

void TestSystemTrayClient::ShowDisplaySettings() {}

void TestSystemTrayClient::ShowPowerSettings() {}

void TestSystemTrayClient::ShowChromeSlow() {}

void TestSystemTrayClient::ShowIMESettings() {}

void TestSystemTrayClient::ShowConnectedDevicesSettings() {
  show_connected_devices_settings_count_++;
}

void TestSystemTrayClient::ShowAboutChromeOS() {}

void TestSystemTrayClient::ShowHelp() {}

void TestSystemTrayClient::ShowAccessibilityHelp() {}

void TestSystemTrayClient::ShowAccessibilitySettings() {}

void TestSystemTrayClient::ShowPaletteHelp() {}

void TestSystemTrayClient::ShowPaletteSettings() {}

void TestSystemTrayClient::ShowPublicAccountInfo() {}

void TestSystemTrayClient::ShowEnterpriseInfo() {}

void TestSystemTrayClient::ShowNetworkConfigure(const std::string& network_id) {
}

void TestSystemTrayClient::ShowNetworkCreate(const std::string& type) {}

void TestSystemTrayClient::ShowThirdPartyVpnCreate(
    const std::string& extension_id) {}

void TestSystemTrayClient::ShowArcVpnCreate(const std::string& app_id) {}

void TestSystemTrayClient::ShowNetworkSettings(const std::string& network_id) {}

void TestSystemTrayClient::ShowMultiDeviceSetup() {
  show_multi_device_setup_count_++;
}

void TestSystemTrayClient::RequestRestartForUpdate() {}

void TestSystemTrayClient::SetLocaleAndExit(
    const std::string& locale_iso_code) {}

}  // namespace ash
