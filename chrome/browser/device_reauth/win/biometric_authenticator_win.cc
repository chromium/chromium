// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/device_reauth/win/biometric_authenticator_win.h"

#include "base/bind.h"
#include "base/callback.h"
#include "base/notreached.h"
#include "base/task/thread_pool.h"
#include "base/threading/sequenced_task_runner_handle.h"
#include "chrome/browser/browser_process.h"
#include "components/password_manager/core/common/password_manager_pref_names.h"
#include "components/prefs/pref_service.h"

namespace {

void SaveAvailability(bool availability) {
  g_browser_process->local_state()->SetBoolean(
      password_manager::prefs::kIsBiometricAvailable, availability);
}

}  // namespace

BiometricAuthenticatorWin::BiometricAuthenticatorWin(
    std::unique_ptr<AuthenticatorWinInterface> authenticator)
    : authenticator_(std::move(authenticator)) {}

BiometricAuthenticatorWin::~BiometricAuthenticatorWin() = default;

// static
scoped_refptr<BiometricAuthenticatorWin>
BiometricAuthenticatorWin::CreateForTesting(
    std::unique_ptr<AuthenticatorWinInterface> authenticator) {
  return base::WrapRefCounted(
      new BiometricAuthenticatorWin(std::move(authenticator)));
}

bool BiometricAuthenticatorWin::CanAuthenticate(
    device_reauth::BiometricAuthRequester requester) {
  // Setting that pref happens once when the ChromeBiometricAuthenticatorFactory
  // is created and it is async so it can technically happen that this pref
  // doesn't have the latest value when you check it.
  return g_browser_process->local_state()->GetBoolean(
      password_manager::prefs::kIsBiometricAvailable);
}

void BiometricAuthenticatorWin::Authenticate(
    device_reauth::BiometricAuthRequester requester,
    AuthenticateCallback callback,
    bool use_last_valid_auth) {
  NOTIMPLEMENTED();
}

void BiometricAuthenticatorWin::AuthenticateWithMessage(
    device_reauth::BiometricAuthRequester requester,
    const std::u16string& message,
    AuthenticateCallback callback) {
  if (!NeedsToAuthenticate()) {
    base::SequencedTaskRunnerHandle::Get()->PostTask(
        FROM_HERE, base::BindOnce(std::move(callback), /*success=*/true));
    return;
  }

  base::SequencedTaskRunnerHandle::Get()->PostTask(
      FROM_HERE,
      base::BindOnce(std::move(callback),
                     RecordAuthenticationResult(
                         authenticator_->AuthenticateUser(message))));
}

void BiometricAuthenticatorWin::Cancel(
    device_reauth::BiometricAuthRequester requester) {
  // TODO(crbug.com/1354552): Add implementation of the Cancel method.
  NOTIMPLEMENTED();
}

void BiometricAuthenticatorWin::CacheIfBiometricsAvailable() {
  authenticator_->CheckIfBiometricsAvailable(base::BindOnce(&SaveAvailability));
}
