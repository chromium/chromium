// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_PUBLIC_CPP_SYSTEM_TRAY_CLIENT_H_
#define ASH_PUBLIC_CPP_SYSTEM_TRAY_CLIENT_H_

#include <string>

#include "ash/public/cpp/ash_public_export.h"
#include "base/strings/string16.h"

namespace ash {

// Handles method calls delegated back to chrome from ash.
class ASH_PUBLIC_EXPORT SystemTrayClient {
 public:
  virtual ~SystemTrayClient() {}

  // Shows general settings UI.
  virtual void ShowSettings() = 0;

  // Shows settings related to Bluetooth devices (e.g. to add a device).
  virtual void ShowBluetoothSettings() = 0;

  // Shows the web UI dialog to pair a Bluetooth device.
  // |address| is the unique device address in the form "XX:XX:XX:XX:XX:XX"
  // with hex digits X. |name_for_display| is a human-readable name, not
  // necessarily the device name.
  virtual void ShowBluetoothPairingDialog(
      const std::string& address,
      const base::string16& name_for_display,
      bool paired,
      bool connected) = 0;

  // Shows the settings related to date, timezone etc.
  virtual void ShowDateSettings() = 0;

  // Shows the dialog to set system time, date, and timezone.
  virtual void ShowSetTimeDialog() = 0;

  // Shows settings related to multiple displays.
  virtual void ShowDisplaySettings() = 0;

  // Shows settings related to power.
  virtual void ShowPowerSettings() = 0;

  // Shows the page that lets you disable performance tracing.
  virtual void ShowChromeSlow() = 0;

  // Shows settings related to input methods.
  virtual void ShowIMESettings() = 0;

  // Shows settings related to MultiDevice features.
  virtual void ShowConnectedDevicesSettings() = 0;

  // Shows the about chrome OS page and checks for updates after the page is
  // loaded.
  virtual void ShowAboutChromeOS() = 0;

  // Shows the Chromebook help app.
  virtual void ShowHelp() = 0;

  // Shows accessibility help.
  virtual void ShowAccessibilityHelp() = 0;

  // Shows the settings related to accessibility.
  virtual void ShowAccessibilitySettings() = 0;

  // Shows the help center article for the stylus tool palette.
  virtual void ShowPaletteHelp() = 0;

  // Shows the settings related to the stylus tool palette.
  virtual void ShowPaletteSettings() = 0;

  // Shows information about public account mode.
  virtual void ShowPublicAccountInfo() = 0;

  // Shows information about enterprise enrolled devices.
  virtual void ShowEnterpriseInfo() = 0;

  // Shows UI to configure or activate the network specified by |network_id|,
  // which may include showing payment or captive portal UI when appropriate.
  virtual void ShowNetworkConfigure(const std::string& network_id) = 0;

  // Shows UI to create a new network connection. |type| is the ONC network type
  // (see onc_spec.md). TODO(stevenjb): Use NetworkType from onc.mojo (TBD).
  virtual void ShowNetworkCreate(const std::string& type) = 0;

  // Shows the "add network" UI to create a third-party extension-backed VPN
  // connection (e.g. Cisco AnyConnect).
  virtual void ShowThirdPartyVpnCreate(const std::string& extension_id) = 0;

  // Launches Arc VPN provider.
  virtual void ShowArcVpnCreate(const std::string& app_id) = 0;

  // Shows settings related to networking. If |network_id| is empty, shows
  // general settings. Otherwise shows settings for the individual network.
  // On devices |network_id| is a GUID, but on Linux desktop and in tests it can
  // be any string.
  virtual void ShowNetworkSettings(const std::string& network_id) = 0;

  // Shows the MultiDevice setup flow dialog.
  virtual void ShowMultiDeviceSetup() = 0;

  // Attempts to restart the system for update.
  virtual void RequestRestartForUpdate() = 0;

  // Sets the UI locale to |locale_iso_code| and exit the session to take
  // effect.
  virtual void SetLocaleAndExit(const std::string& locale_iso_code) = 0;

 protected:
  SystemTrayClient() {}
};

}  // namespace ash

#endif  // ASH_PUBLIC_CPP_SYSTEM_TRAY_CLIENT_H_
