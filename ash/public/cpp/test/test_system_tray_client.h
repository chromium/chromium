// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_PUBLIC_CPP_TEST_TEST_SYSTEM_TRAY_CLIENT_H_
#define ASH_PUBLIC_CPP_TEST_TEST_SYSTEM_TRAY_CLIENT_H_

#include "ash/public/cpp/ash_public_export.h"
#include "ash/public/cpp/system_tray_client.h"
#include "base/macros.h"

namespace ash {

// A SystemTrayClient that does nothing. Used by AshTestBase.
class ASH_PUBLIC_EXPORT TestSystemTrayClient : public SystemTrayClient {
 public:
  TestSystemTrayClient();
  ~TestSystemTrayClient() override;

  // SystemTrayClient:
  void ShowSettings() override;
  void ShowBluetoothSettings() override;
  void ShowBluetoothPairingDialog(const std::string& address,
                                  const base::string16& name_for_display,
                                  bool paired,
                                  bool connected) override;
  void ShowDateSettings() override;
  void ShowSetTimeDialog() override;
  void ShowDisplaySettings() override;
  void ShowPowerSettings() override;
  void ShowChromeSlow() override;
  void ShowIMESettings() override;
  void ShowConnectedDevicesSettings() override;
  void ShowAboutChromeOS() override;
  void ShowHelp() override;
  void ShowAccessibilityHelp() override;
  void ShowAccessibilitySettings() override;
  void ShowPaletteHelp() override;
  void ShowPaletteSettings() override;
  void ShowPublicAccountInfo() override;
  void ShowEnterpriseInfo() override;
  void ShowNetworkConfigure(const std::string& network_id) override;
  void ShowNetworkCreate(const std::string& type) override;
  void ShowThirdPartyVpnCreate(const std::string& extension_id) override;
  void ShowArcVpnCreate(const std::string& app_id) override;
  void ShowNetworkSettings(const std::string& network_id) override;
  void ShowMultiDeviceSetup() override;
  void RequestRestartForUpdate() override;
  void SetLocaleAndExit(const std::string& locale_iso_code) override;

  int show_bluetooth_settings_count() const {
    return show_bluetooth_settings_count_;
  }
  int show_multi_device_setup_count() const {
    return show_multi_device_setup_count_;
  }
  int show_connected_devices_settings_count() const {
    return show_connected_devices_settings_count_;
  }

 private:
  int show_bluetooth_settings_count_ = 0;
  int show_multi_device_setup_count_ = 0;
  int show_connected_devices_settings_count_ = 0;

  DISALLOW_COPY_AND_ASSIGN(TestSystemTrayClient);
};

}  // namespace ash

#endif  // ASH_PUBLIC_CPP_TEST_TEST_SYSTEM_TRAY_CLIENT_H_
