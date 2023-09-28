// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/device_reauth/chromeos/device_authenticator_chromeos.h"

#include "base/memory/ptr_util.h"
#include "base/notreached.h"
#include "base/task/sequenced_task_runner.h"

DeviceAuthenticatorChromeOS::DeviceAuthenticatorChromeOS(
    std::unique_ptr<AuthenticatorChromeOSInterface> authenticator,
    DeviceAuthenticatorProxy* proxy,
    const device_reauth::DeviceAuthParams& params)
    : ChromeDeviceAuthenticatorCommon(proxy,
                                      params.GetAuthenticationValidityPeriod()),
      authenticator_(std::move(authenticator)) {}

DeviceAuthenticatorChromeOS::~DeviceAuthenticatorChromeOS() = default;

bool DeviceAuthenticatorChromeOS::CanAuthenticateWithBiometrics() {
  // TODO(crbug.com/1440090): Add implementation of the biometric
  // authentication.
  NOTIMPLEMENTED();
  return false;
}

bool DeviceAuthenticatorChromeOS::CanAuthenticateWithBiometricOrScreenLock() {
  // TODO(crbug.com/1440090): Add implementation of the biometric or screen lock
  // authentication.
  NOTIMPLEMENTED();
  return false;
}

void DeviceAuthenticatorChromeOS::AuthenticateWithMessage(
    const std::u16string& message,
    AuthenticateCallback callback) {
  if (!NeedsToAuthenticate()) {
    base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
        FROM_HERE, base::BindOnce(std::move(callback), /*success=*/true));
    return;
  }

  callback_ = std::move(callback);

  authenticator_->AuthenticateUser(
      base::BindOnce(&DeviceAuthenticatorChromeOS::OnAuthenticationCompleted,
                     weak_ptr_factory_.GetWeakPtr()));
}

void DeviceAuthenticatorChromeOS::Cancel() {
  // TODO(b/292097975): Cancel the in session auth dialog.
  if (callback_) {
    std::move(callback_).Run(false);
  }
}

void DeviceAuthenticatorChromeOS::OnAuthenticationCompleted(bool success) {
  if (!callback_) {
    return;
  }

  RecordAuthenticationTimeIfSuccessful(success);
  std::move(callback_).Run(success);
}
