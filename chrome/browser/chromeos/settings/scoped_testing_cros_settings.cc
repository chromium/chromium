// Copyright (c) 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/settings/scoped_testing_cros_settings.h"

#include "chrome/browser/chromeos/settings/cros_settings.h"
#include "chrome/browser/chromeos/settings/stub_cros_settings_provider.h"
#include "chromeos/settings/system_settings_provider.h"

#include "chrome/browser/chromeos/ownership/owner_settings_service_chromeos_factory.h"

namespace chromeos {

ScopedTestingCrosSettings::ScopedTestingCrosSettings() {
  test_instance_ = std::make_unique<CrosSettings>();

  std::unique_ptr<StubCrosSettingsProvider> device_settings =
      std::make_unique<StubCrosSettingsProvider>();
  OwnerSettingsServiceChromeOSFactory::SetStubCrosSettingsProviderForTesting(
      device_settings.get());
  device_settings_ptr_ = device_settings.get();
  test_instance_->AddSettingsProvider(std::move(device_settings));

  std::unique_ptr<SystemSettingsProvider> system_settings =
      std::make_unique<SystemSettingsProvider>();
  system_settings_ptr_ = system_settings.get();
  test_instance_->AddSettingsProvider(std::move(system_settings));

  CrosSettings::SetForTesting(test_instance_.get());
}

ScopedTestingCrosSettings::~ScopedTestingCrosSettings() {
  OwnerSettingsServiceChromeOSFactory::SetStubCrosSettingsProviderForTesting(
      nullptr);
  device_settings_ptr_ = nullptr;
  system_settings_ptr_ = nullptr;
  CrosSettings::ShutdownForTesting();
}

}  // namespace chromeos
