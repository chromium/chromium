// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/password_manager/password_manager_settings_service_impl.h"

#include "components/password_manager/core/common/password_manager_pref_names.h"
#include "components/prefs/pref_service.h"

using password_manager::PasswordManagerSetting;

PasswordManagerSettingsServiceImpl::PasswordManagerSettingsServiceImpl(
    PrefService* pref_service)
    : pref_service_(pref_service) {}

bool PasswordManagerSettingsServiceImpl::IsSettingEnabled(
    PasswordManagerSetting setting) const {
  switch (setting) {
    case PasswordManagerSetting::kOfferToSavePasswords:
      return pref_service_->GetBoolean(
          password_manager::prefs::kCredentialsEnableService);
    case PasswordManagerSetting::kAutoSignIn:
      return pref_service_->GetBoolean(
          password_manager::prefs::kCredentialsEnableAutosignin);
  }
}

void PasswordManagerSettingsServiceImpl::RequestSettingsFromBackend() {
  // This method is invoked only on android when UPM is enabled.
  NOTREACHED();
}

void PasswordManagerSettingsServiceImpl::TurnOffAutoSignIn() {
  pref_service_->SetBoolean(
      password_manager::prefs::kCredentialsEnableAutosignin, false);
}
