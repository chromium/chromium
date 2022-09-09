// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/ownership/fake_owner_settings_service.h"

#include "base/check.h"
#include "chrome/browser/ash/ownership/owner_settings_service_ash_factory.h"
#include "chrome/browser/ash/settings/stub_cros_settings_provider.h"
#include "chromeos/ash/components/install_attributes/install_attributes.h"
#include "components/ownership/mock_owner_key_util.h"

namespace ash {

FakeOwnerSettingsService::FakeOwnerSettingsService(
    StubCrosSettingsProvider* provider,
    Profile* profile)
    : OwnerSettingsServiceAsh(
          /* device_settings_service= */ nullptr,
          profile,
          OwnerSettingsServiceAshFactory::GetInstance()->GetOwnerKeyUtil()),
      set_management_settings_result_(true),
      settings_provider_(provider) {}

FakeOwnerSettingsService::FakeOwnerSettingsService(
    StubCrosSettingsProvider* provider,
    Profile* profile,
    const scoped_refptr<ownership::OwnerKeyUtil>& owner_key_util)
    : OwnerSettingsServiceAsh(nullptr, profile, owner_key_util),
      set_management_settings_result_(true),
      settings_provider_(provider) {}

FakeOwnerSettingsService::~FakeOwnerSettingsService() = default;

bool FakeOwnerSettingsService::IsOwner() {
  return !InstallAttributes::Get()->IsEnterpriseManaged() &&
         settings_provider_->current_user_is_owner();
}

bool FakeOwnerSettingsService::Set(const std::string& setting,
                                   const base::Value& value) {
  CHECK(settings_provider_);
  settings_provider_->Set(setting, value);
  return true;
}

}  // namespace ash
