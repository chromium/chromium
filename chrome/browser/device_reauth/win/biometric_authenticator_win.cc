// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/device_reauth/win/biometric_authenticator_win.h"

#include "base/bind.h"
#include "base/notreached.h"
#include "base/threading/sequenced_task_runner_handle.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_finder.h"
#include "chrome/browser/ui/browser_window.h"

AuthenticatorWin::~AuthenticatorWin() = default;

bool AuthenticatorWin::AuthenticateUser(const std::u16string& message) {
  bool success = false;
  Browser* browser = chrome::FindBrowserWithActiveWindow();
  if (!browser) {
    success = false;
  } else {
    gfx::NativeWindow window = browser->window()->GetNativeWindow();
    success = password_manager_util_win::AuthenticateUser(window, message);
  }
  return success;
}

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
  NOTIMPLEMENTED();
  return false;
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
