// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/settings/scoped_testing_cros_settings.h"

#include "chrome/browser/ash/settings/stub_cros_settings_provider.h"
#include "chromeos/ash/components/settings/cros_settings.h"
#include "chromeos/ash/components/settings/system_settings_provider.h"
#include "chromeos/ash/components/settings/user_login_permission_tracker.h"

namespace ash {

ScopedTestingCrosSettings::ScopedTestingCrosSettings()
    : test_instance_(std::make_unique<CrosSettings>()) {
  std::unique_ptr<StubCrosSettingsProvider> device_settings =
      std::make_unique<StubCrosSettingsProvider>();
  device_settings_ptr_ = device_settings.get();
  test_instance_->AddSettingsProvider(std::move(device_settings));

  std::unique_ptr<SystemSettingsProvider> system_settings =
      std::make_unique<SystemSettingsProvider>();
  system_settings_ptr_ = system_settings.get();
  test_instance_->AddSettingsProvider(std::move(system_settings));

  CHECK(!CrosSettings::IsInitialized());
  CrosSettings::SetInstance(test_instance_.get());
  user_login_permission_tracker_ =
      std::make_unique<UserLoginPermissionTracker>(test_instance_.get());
}

ScopedTestingCrosSettings::~ScopedTestingCrosSettings() {
  user_login_permission_tracker_.reset();
  CHECK_EQ(CrosSettings::Get(), test_instance_.get());
  CrosSettings::SetInstance(nullptr);
  device_settings_ptr_ = nullptr;
  system_settings_ptr_ = nullptr;
}

}  // namespace ash
