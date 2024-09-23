// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/login/auth/chrome_safe_mode_delegate.h"

#include "chrome/browser/ash/ownership/owner_settings_service_ash.h"
#include "chrome/browser/ash/ownership/owner_settings_service_ash_factory.h"
#include "chromeos/ash/components/login/login_state/login_state.h"
#include "chromeos/ash/components/settings/cros_settings.h"

namespace ash {

bool ChromeSafeModeDelegate::IsSafeMode() {
  bool is_safe_mode = false;
  CrosSettings::Get()->GetBoolean(kPolicyMissingMitigationMode, &is_safe_mode);
  return is_safe_mode;
}

void ChromeSafeModeDelegate::CheckSafeModeOwnership(
    const std::string& user_id_hash,
    IsOwnerCallback callback) {
  // `IsOwnerForSafeModeAsync` expects logged in state to be
  // LOGGED_IN_SAFE_MODE.
  if (LoginState::IsInitialized()) {
    LoginState::Get()->SetLoggedInState(LoginState::LOGGED_IN_SAFE_MODE,
                                        LoginState::LOGGED_IN_USER_NONE);
  }

  OwnerSettingsServiceAsh::IsOwnerForSafeModeAsync(
      user_id_hash,
      OwnerSettingsServiceAshFactory::GetInstance()->GetOwnerKeyUtil(),
      std::move(callback));
}

}  // namespace ash
