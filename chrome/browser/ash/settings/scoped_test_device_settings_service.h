// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_SETTINGS_SCOPED_TEST_DEVICE_SETTINGS_SERVICE_H_
#define CHROME_BROWSER_ASH_SETTINGS_SCOPED_TEST_DEVICE_SETTINGS_SERVICE_H_

namespace ash {

// Helper class for tests. Initializes the DeviceSettingsService singleton on
// construction and tears it down again on destruction.
class ScopedTestDeviceSettingsService {
 public:
  ScopedTestDeviceSettingsService();

  ScopedTestDeviceSettingsService(const ScopedTestDeviceSettingsService&) =
      delete;
  ScopedTestDeviceSettingsService& operator=(
      const ScopedTestDeviceSettingsService&) = delete;

  ~ScopedTestDeviceSettingsService();
};

}  // namespace ash

#endif  // CHROME_BROWSER_ASH_SETTINGS_SCOPED_TEST_DEVICE_SETTINGS_SERVICE_H_
