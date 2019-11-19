// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/settings/stub_cros_settings_provider.h"

#include "base/logging.h"
#include "base/memory/ptr_util.h"
#include "base/values.h"
#include "chrome/browser/chromeos/settings/cros_settings.h"
#include "chrome/browser/chromeos/settings/device_settings_provider.h"
#include "chromeos/settings/cros_settings_names.h"

namespace chromeos {

StubCrosSettingsProvider::StubCrosSettingsProvider(
    const NotifyObserversCallback& notify_cb)
    : CrosSettingsProvider(notify_cb) {
  SetDefaults();
}

StubCrosSettingsProvider::StubCrosSettingsProvider()
  : CrosSettingsProvider(CrosSettingsProvider::NotifyObserversCallback()) {
  SetDefaults();
}

StubCrosSettingsProvider::~StubCrosSettingsProvider() {
}

const base::Value* StubCrosSettingsProvider::Get(
    const std::string& path) const {
  DCHECK(HandlesSetting(path));
  const base::Value* value;
  if (values_.GetValue(path, &value))
    return value;
  return NULL;
}

CrosSettingsProvider::TrustedStatus
    StubCrosSettingsProvider::PrepareTrustedValues(const base::Closure& cb) {
  if (trusted_status_ == TEMPORARILY_UNTRUSTED)
    callbacks_.push_back(cb);
  return trusted_status_;
}

bool StubCrosSettingsProvider::HandlesSetting(const std::string& path) const {
  return DeviceSettingsProvider::IsDeviceSetting(path);
}

void StubCrosSettingsProvider::SetTrustedStatus(TrustedStatus status) {
  trusted_status_ = status;
  if (trusted_status_ != TEMPORARILY_UNTRUSTED) {
    std::vector<base::Closure> callbacks_to_invoke = std::move(callbacks_);
    for (base::Closure cb : callbacks_to_invoke)
      cb.Run();
  }
}

void StubCrosSettingsProvider::SetCurrentUserIsOwner(bool owner) {
  current_user_is_owner_ = owner;
}

void StubCrosSettingsProvider::Set(const std::string& path,
                                   const base::Value& value) {
  bool is_value_changed = false;
  if (current_user_is_owner_)
    is_value_changed = values_.SetValue(path, value.Clone());
  else
    LOG(WARNING) << "Blocked changing setting from non-owner, setting=" << path;

  if (is_value_changed || !current_user_is_owner_)
    NotifyObservers(path);
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
  values_.SetBoolean(kAccountsPrefAllowGuest, true);
  values_.SetBoolean(kAccountsPrefAllowNewUser, true);
  values_.SetBoolean(kAccountsPrefSupervisedUsersEnabled, true);
  values_.SetBoolean(kAccountsPrefShowUserNamesOnSignIn, true);
  values_.SetValue(kAccountsPrefUsers, base::Value(base::Value::Type::LIST));
  values_.SetBoolean(kAllowBluetooth, true);
  values_.SetBoolean(kDeviceWiFiAllowed, true);
  values_.SetBoolean(kAttestationForContentProtectionEnabled, true);
  values_.SetBoolean(kStatsReportingPref, true);
  values_.SetValue(kAccountsPrefDeviceLocalAccounts,
                   base::Value(base::Value::Type::LIST));
  // |kDeviceOwner| will be set to the logged-in user by |UserManager|.
}

}  // namespace chromeos
