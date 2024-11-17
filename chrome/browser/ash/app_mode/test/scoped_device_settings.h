// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_APP_MODE_TEST_SCOPED_DEVICE_SETTINGS_H_
#define CHROME_BROWSER_ASH_APP_MODE_TEST_SCOPED_DEVICE_SETTINGS_H_

#include <memory>

#include "chrome/browser/ash/settings/scoped_cros_settings_test_helper.h"

namespace ash {

class FakeOwnerSettingsService;

// Initializes a fake OwnerSettingsService owned by the primary user. Useful for
// browser tests to fake device policies.
class ScopedDeviceSettings {
 public:
  ScopedDeviceSettings();
  ~ScopedDeviceSettings();

  FakeOwnerSettingsService* owner_settings_service() {
    return owner_settings_service_.get();
  }

 private:
  ScopedCrosSettingsTestHelper settings_helper_;
  std::unique_ptr<FakeOwnerSettingsService> owner_settings_service_;
};

}  // namespace ash

#endif  // CHROME_BROWSER_ASH_APP_MODE_TEST_SCOPED_DEVICE_SETTINGS_H_
