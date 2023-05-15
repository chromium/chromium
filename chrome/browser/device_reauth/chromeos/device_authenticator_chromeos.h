// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_DEVICE_REAUTH_CHROMEOS_DEVICE_AUTHENTICATOR_CHROMEOS_H_
#define CHROME_BROWSER_DEVICE_REAUTH_CHROMEOS_DEVICE_AUTHENTICATOR_CHROMEOS_H_

#include "base/functional/callback.h"
#include "base/sequence_checker.h"
#include "chrome/browser/device_reauth/chrome_device_authenticator_common.h"
#include "chrome/browser/device_reauth/chrome_device_authenticator_factory.h"
#include "chrome/browser/device_reauth/chromeos/authenticator_chromeos.h"
#include "components/device_reauth/device_authenticator.h"

class DeviceAuthenticatorChromeOS : public ChromeDeviceAuthenticatorCommon {
 public:
  // Creates an instance of DeviceAuthenticatorChromeOS for testing purposes
  // only.
  static scoped_refptr<DeviceAuthenticatorChromeOS> CreateForTesting(
      std::unique_ptr<AuthenticatorChromeOSInterface> authenticator);

  bool CanAuthenticateWithBiometrics() override;

  void Authenticate(device_reauth::DeviceAuthRequester requester,
                    AuthenticateCallback callback,
                    bool use_last_valid_auth) override;

  void AuthenticateWithMessage(const std::u16string& message,
                               AuthenticateCallback callback) override;

  void Cancel(device_reauth::DeviceAuthRequester requester) override;

 private:
  friend class ChromeDeviceAuthenticatorFactory;

  explicit DeviceAuthenticatorChromeOS(
      std::unique_ptr<AuthenticatorChromeOSInterface> authenticator);
  ~DeviceAuthenticatorChromeOS() override;

  // Records authentication status and executes |callback| with |success|
  // parameter.
  void OnAuthenticationCompleted(base::OnceCallback<void(bool)> callback,
                                 bool success);

  std::unique_ptr<AuthenticatorChromeOSInterface> authenticator_;

  // Factory for weak pointers to this class.
  base::WeakPtrFactory<DeviceAuthenticatorChromeOS> weak_ptr_factory_{this};
};

#endif  // CHROME_BROWSER_DEVICE_REAUTH_CHROMEOS_DEVICE_AUTHENTICATOR_CHROMEOS_H_
