// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_DEVICE_REAUTH_CHROMEOS_DEVICE_AUTHENTICATOR_CHROMEOS_H_
#define CHROME_BROWSER_DEVICE_REAUTH_CHROMEOS_DEVICE_AUTHENTICATOR_CHROMEOS_H_

#include "base/functional/callback.h"
#include "base/sequence_checker.h"
#include "chrome/browser/device_reauth/chrome_device_authenticator_factory.h"
#include "chrome/browser/device_reauth/chromeos/authenticator_chromeos.h"
#include "components/device_reauth/device_authenticator.h"
#include "components/device_reauth/device_authenticator_common.h"

class DeviceAuthenticatorChromeOS : public DeviceAuthenticatorCommon {
 public:
  DeviceAuthenticatorChromeOS(
      std::unique_ptr<AuthenticatorChromeOSInterface> authenticator,
      DeviceAuthenticatorProxy* proxy,
      const device_reauth::DeviceAuthParams& params);
  ~DeviceAuthenticatorChromeOS() override;

  bool CanAuthenticateWithBiometrics() override;

  bool CanAuthenticateWithBiometricOrScreenLock() override;

  void AuthenticateWithMessage(const std::u16string& message,
                               AuthenticateCallback callback) override;

  void Cancel() override;

 private:
  // Records authentication status and executes |callback| with |success|
  // parameter.
  void OnAuthenticationCompleted(bool success);

  std::unique_ptr<AuthenticatorChromeOSInterface> authenticator_;

  // Callback to be executed after the authentication completes.
  AuthenticateCallback callback_;

  // Factory for weak pointers to this class.
  base::WeakPtrFactory<DeviceAuthenticatorChromeOS> weak_ptr_factory_{this};
};

#endif  // CHROME_BROWSER_DEVICE_REAUTH_CHROMEOS_DEVICE_AUTHENTICATOR_CHROMEOS_H_
