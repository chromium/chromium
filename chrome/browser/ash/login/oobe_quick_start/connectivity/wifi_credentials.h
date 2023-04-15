// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_LOGIN_OOBE_QUICK_START_CONNECTIVITY_WIFI_CREDENTIALS_H_
#define CHROME_BROWSER_ASH_LOGIN_OOBE_QUICK_START_CONNECTIVITY_WIFI_CREDENTIALS_H_

#include <string>

#include "chromeos/ash/services/nearby/public/mojom/quick_start_decoder_types.mojom-shared.h"
#include "chromeos/ash/services/nearby/public/mojom/quick_start_decoder_types.mojom.h"

namespace ash::quick_start {

// A `struct` to store the information related to a device's Wifi Credentials.
// It is constructed from the information returned by whatever device is
// helping setup this device, and is used in the Quick Start flow to configure
// the device's Wifi connection.
struct WifiCredentials {
  WifiCredentials();
  WifiCredentials(const WifiCredentials& other);
  WifiCredentials& operator=(const WifiCredentials& other);
  ~WifiCredentials();

  // The SSID of the Wifi Network.
  std::string ssid;

  // The password of the Wifi Network.
  std::string password;

  // Whether this is a hidden Wifi Network.
  bool is_hidden;

  // The Security Type of the Wifi Network.
  ash::quick_start::mojom::WifiSecurityType security_type;
};

}  // namespace ash::quick_start

#endif  // CHROME_BROWSER_ASH_LOGIN_OOBE_QUICK_START_CONNECTIVITY_WIFI_CREDENTIALS_H_
