// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_SETTINGS_CROS_SETTINGS_HOLDER_H_
#define CHROME_BROWSER_ASH_SETTINGS_CROS_SETTINGS_HOLDER_H_

#include <memory>

class PrefService;

namespace ash {

class CrosSettings;
class DeviceSettingsService;

// Holds the instance of CrosSettings.
class CrosSettingsHolder {
 public:
  CrosSettingsHolder(DeviceSettingsService* device_settings_service,
                     PrefService* local_state);
  CrosSettingsHolder(const CrosSettingsHolder&) = delete;
  CrosSettingsHolder& operator=(const CrosSettingsHolder&) = delete;
  ~CrosSettingsHolder();

 private:
  std::unique_ptr<CrosSettings> cros_settings_;
};

}  // namespace ash

#endif  // CHROME_BROWSER_ASH_SETTINGS_CROS_SETTINGS_HOLDER_H_
