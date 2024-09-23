// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/settings/cros_settings_holder.h"

#include <memory>

#include "base/check_is_test.h"
#include "base/check_op.h"
#include "base/functional/bind.h"
#include "chrome/browser/ash/settings/device_settings_provider.h"
#include "chrome/browser/ash/settings/device_settings_service.h"
#include "chrome/browser/ash/settings/supervised_user_cros_settings_provider.h"
#include "chromeos/ash/components/settings/cros_settings.h"
#include "chromeos/ash/components/settings/system_settings_provider.h"

namespace ash {

CrosSettingsHolder::CrosSettingsHolder(
    DeviceSettingsService* device_settings_service,
    PrefService* local_state)
    : cros_settings_(std::make_unique<CrosSettings>()) {
  // Using Unretained here is safe, because `cros_settings_` outlives
  // than each provider.
  auto notify_callback = base::BindRepeating(
      &CrosSettings::FireObservers, base::Unretained(cros_settings_.get()));

  cros_settings_->SetSupervisedUserCrosSettingsProvider(
      std::make_unique<SupervisedUserCrosSettingsProvider>(notify_callback));
  cros_settings_->AddSettingsProvider(std::make_unique<DeviceSettingsProvider>(
      notify_callback, device_settings_service, local_state));
  cros_settings_->AddSettingsProvider(
      std::make_unique<SystemSettingsProvider>(notify_callback));

  if (CrosSettings::IsInitialized()) {
    // On tests, CrosSettings may be overwritten to inject testing behavior.
    // Keep the existing one.
    LOG(WARNING)
        << "CrosSettings instance was injected. Skipping initialization.";
    CHECK_IS_TEST();
  } else {
    CrosSettings::SetInstance(cros_settings_.get());
  }
}

CrosSettingsHolder::~CrosSettingsHolder() {
  if (CrosSettings::IsInitialized() &&
      CrosSettings::Get() == cros_settings_.get()) {
    CrosSettings::SetInstance(nullptr);
  }
}

}  // namespace ash
