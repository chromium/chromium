// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/settings/scoped_testing_cros_settings.h"

#include "chrome/browser/ash/ownership/owner_settings_service_ash_factory.h"
#include "chrome/browser/ash/settings/stub_cros_settings_provider.h"
#include "chromeos/ash/components/settings/cros_settings.h"
#include "chromeos/ash/components/settings/system_settings_provider.h"

namespace ash {

ScopedTestingCrosSettings::ScopedTestingCrosSettings()
    : test_instance_(std::make_unique<CrosSettings>()) {
  std::unique_ptr<StubCrosSettingsProvider> device_settings =
      std::make_unique<StubCrosSettingsProvider>();
  OwnerSettingsServiceAshFactory::SetStubCrosSettingsProviderForTesting(
      device_settings.get());
  device_settings_ptr_ = device_settings.get();
  test_instance_->AddSettingsProvider(std::move(device_settings));

  std::unique_ptr<SystemSettingsProvider> system_settings =
      std::make_unique<SystemSettingsProvider>();
  system_settings_ptr_ = system_settings.get();
  test_instance_->AddSettingsProvider(std::move(system_settings));

  CHECK(!CrosSettings::IsInitialized());
  CrosSettings::SetInstance(test_instance_.get());
}

ScopedTestingCrosSettings::~ScopedTestingCrosSettings() {
  CHECK_EQ(CrosSettings::Get(), test_instance_.get());
  CrosSettings::SetInstance(nullptr);
  OwnerSettingsServiceAshFactory::SetStubCrosSettingsProviderForTesting(
      nullptr);
  device_settings_ptr_ = nullptr;
  system_settings_ptr_ = nullptr;
}

}  // namespace ash
