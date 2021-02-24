// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/login/auth/chrome_cryptohome_authenticator.h"

#include "base/threading/thread_task_runner_handle.h"
#include "chrome/browser/chromeos/ownership/owner_settings_service_chromeos.h"
#include "chrome/browser/chromeos/ownership/owner_settings_service_chromeos_factory.h"
#include "chrome/browser/chromeos/settings/cros_settings.h"
#include "chromeos/login/login_state/login_state.h"
#include "components/ownership/owner_key_util.h"
#include "components/user_manager/user_manager.h"
#include "content/public/browser/browser_thread.h"

using content::BrowserThread;

namespace chromeos {

ChromeCryptohomeAuthenticator::ChromeCryptohomeAuthenticator(
    AuthStatusConsumer* consumer)
    : CryptohomeAuthenticator(base::ThreadTaskRunnerHandle::Get(), consumer) {}

ChromeCryptohomeAuthenticator::~ChromeCryptohomeAuthenticator() {}

bool ChromeCryptohomeAuthenticator::IsKnownUser(const UserContext& context) {
  return user_manager::UserManager::Get()->IsKnownUser(context.GetAccountId());
}

bool ChromeCryptohomeAuthenticator::IsSafeMode() {
  bool is_safe_mode = false;
  CrosSettings::Get()->GetBoolean(kPolicyMissingMitigationMode, &is_safe_mode);
  return is_safe_mode;
}

void ChromeCryptohomeAuthenticator::CheckSafeModeOwnership(
    const UserContext& context,
    IsOwnerCallback callback) {
  // `IsOwnerForSafeModeAsync` expects logged in state to be
  // LOGGED_IN_SAFE_MODE.
  if (LoginState::IsInitialized()) {
    LoginState::Get()->SetLoggedInState(LoginState::LOGGED_IN_SAFE_MODE,
                                        LoginState::LOGGED_IN_USER_NONE);
  }

  OwnerSettingsServiceChromeOS::IsOwnerForSafeModeAsync(
      context.GetUserIDHash(),
      OwnerSettingsServiceChromeOSFactory::GetInstance()->GetOwnerKeyUtil(),
      std::move(callback));
}

}  // namespace chromeos
