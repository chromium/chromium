// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/device_reauth/chromeos/device_authenticator_chromeos.h"

#include "base/notreached.h"
#include "base/task/sequenced_task_runner.h"

DeviceAuthenticatorChromeOS::DeviceAuthenticatorChromeOS(
    std::unique_ptr<AuthenticatorChromeOSInterface> authenticator)
    : authenticator_(std::move(authenticator)) {}

DeviceAuthenticatorChromeOS::~DeviceAuthenticatorChromeOS() = default;

// static
scoped_refptr<DeviceAuthenticatorChromeOS>
DeviceAuthenticatorChromeOS::CreateForTesting(
    std::unique_ptr<AuthenticatorChromeOSInterface> authenticator) {
  return base::WrapRefCounted(
      new DeviceAuthenticatorChromeOS(std::move(authenticator)));
}

bool DeviceAuthenticatorChromeOS::CanAuthenticateWithBiometrics() {
  // TODO(crbug.com/1440090): Add implementation the biometric authentication.
  NOTIMPLEMENTED();
  return false;
}

void DeviceAuthenticatorChromeOS::Authenticate(
    device_reauth::DeviceAuthRequester requester,
    AuthenticateCallback callback,
    bool use_last_valid_auth) {
  NOTIMPLEMENTED();
}

void DeviceAuthenticatorChromeOS::AuthenticateWithMessage(
    const std::u16string& message,
    AuthenticateCallback callback) {
  if (!NeedsToAuthenticate()) {
    base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
        FROM_HERE, base::BindOnce(std::move(callback), /*success=*/true));
    return;
  }

  authenticator_->AuthenticateUser(
      base::BindOnce(&DeviceAuthenticatorChromeOS::OnAuthenticationCompleted,
                     weak_ptr_factory_.GetWeakPtr(), std::move(callback)));
}

void DeviceAuthenticatorChromeOS::Cancel(
    device_reauth::DeviceAuthRequester requester) {
  // TODO(crbug.com/1440090): Add implementation of the Cancel method.
  NOTIMPLEMENTED();
}

void DeviceAuthenticatorChromeOS::OnAuthenticationCompleted(
    base::OnceCallback<void(bool)> callback,
    bool success) {
  RecordAuthenticationTimeIfSuccessful(success);
  std::move(callback).Run(success);
}
