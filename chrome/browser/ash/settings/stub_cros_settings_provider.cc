// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/settings/stub_cros_settings_provider.h"

#include <string_view>

#include "base/logging.h"
#include "base/memory/ptr_util.h"
#include "base/values.h"
#include "chrome/browser/ash/settings/device_settings_provider.h"
#include "chromeos/ash/components/settings/cros_settings.h"
#include "chromeos/ash/components/settings/cros_settings_names.h"

namespace ash {

StubCrosSettingsProvider::StubCrosSettingsProvider(
    const NotifyObserversCallback& notify_cb)
    : CrosSettingsProvider(notify_cb),
      fake_provider_(
          base::BindRepeating(&StubCrosSettingsProvider::OnValueChanged,
                              base::Unretained(this))) {
  SetDefaults();
}

StubCrosSettingsProvider::~StubCrosSettingsProvider() = default;

const base::Value* StubCrosSettingsProvider::Get(std::string_view path) const {
  DCHECK(HandlesSetting(path));
  return fake_provider_.Get(path);
}

CrosSettingsProvider::TrustedStatus
StubCrosSettingsProvider::PrepareTrustedValues(base::OnceClosure* callback) {
  return fake_provider_.PrepareTrustedValues(callback);
}

bool StubCrosSettingsProvider::HandlesSetting(std::string_view path) const {
  return DeviceSettingsProvider::IsDeviceSetting(path);
}

void StubCrosSettingsProvider::SetTrustedStatus(TrustedStatus status) {
  fake_provider_.SetTrustedStatus(status);
}

void StubCrosSettingsProvider::SetCurrentUserIsOwner(bool owner) {
  current_user_is_owner_ = owner;
}

void StubCrosSettingsProvider::Set(const std::string& path,
                                   const base::Value& value) {
  if (!current_user_is_owner_) {
    LOG(WARNING) << "Blocked changing setting from non-owner, setting=" << path;
    NotifyObservers(path);
    return;
  }
  fake_provider_.Set(path, value.Clone());
}

void StubCrosSettingsProvider::SetBoolean(const std::string& path,
                                          bool in_value) {
  Set(path, base::Value(in_value));
}

void StubCrosSettingsProvider::SetInteger(const std::string& path,
                                          int in_value) {
  Set(path, base::Value(in_value));
}

void StubCrosSettingsProvider::SetDouble(const std::string& path,
                                         double in_value) {
  Set(path, base::Value(in_value));
}

void StubCrosSettingsProvider::SetString(const std::string& path,
                                         const std::string& in_value) {
  Set(path, base::Value(in_value));
}

void StubCrosSettingsProvider::SetDefaults() {
  fake_provider_.Set(kAccountsPrefAllowGuest, true);
  fake_provider_.Set(kAccountsPrefAllowNewUser, true);
  fake_provider_.Set(kAccountsPrefFamilyLinkAccountsAllowed, false);
  fake_provider_.Set(kAccountsPrefShowUserNamesOnSignIn, true);
  fake_provider_.Set(kAccountsPrefUsers, base::Value(base::Value::Type::LIST));
  fake_provider_.Set(kAllowBluetooth, true);
  fake_provider_.Set(kDeviceWiFiAllowed, true);
  fake_provider_.Set(kAttestationForContentProtectionEnabled, true);
  fake_provider_.Set(kStatsReportingPref, true);
  fake_provider_.Set(kAccountsPrefDeviceLocalAccounts,
                     base::Value(base::Value::Type::LIST));
  fake_provider_.Set(kDevicePeripheralDataAccessEnabled, true);
  fake_provider_.Set(kRevenEnableDeviceHWDataUsage, false);
  // |kDeviceOwner| will be set to the logged-in user by |UserManager|.
}

void StubCrosSettingsProvider::OnValueChanged(const std::string& path) {
  NotifyObservers(path);
}

}  // namespace ash
