// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_DEVICE_REAUTH_WIN_DEVICE_AUTHENTICATOR_WIN_H_
#define CHROME_BROWSER_DEVICE_REAUTH_WIN_DEVICE_AUTHENTICATOR_WIN_H_

#include <memory>

#include "chrome/browser/device_reauth/chrome_device_authenticator_factory.h"
#include "chrome/browser/device_reauth/win/authenticator_win.h"
#include "components/device_reauth/device_authenticator.h"
#include "components/device_reauth/device_authenticator_common.h"

class DeviceAuthenticatorWin : public DeviceAuthenticatorCommon {
 public:
  DeviceAuthenticatorWin(
      std::unique_ptr<AuthenticatorWinInterface> authenticator,
      DeviceAuthenticatorProxy* proxy,
      const device_reauth::DeviceAuthParams& params);
  ~DeviceAuthenticatorWin() override;

  // Returns true, when biometrics are available.
  bool CanAuthenticateWithBiometrics() override;

  // Returns true, when biometrics or screen lock is available.
  bool CanAuthenticateWithBiometricOrScreenLock() override;

  // Triggers an authentication flow based on biometrics. Request user to
  // authenticate(a prompt with that information will appear on the screen and
  // the `message` will be displayed there) using their windows hello or if it's
  // not set up, default one with password will appear.
  void AuthenticateWithMessage(const std::u16string& message,
                               AuthenticateCallback callback) override;

  // Should be called by the object using the authenticator if the purpose
  // for which the auth was requested becomes obsolete or the object is
  // destroyed.
  void Cancel() override;

  // Asks Windows if user has configured and enabled biometrics on
  // their machine. Stores the response in a local state pref for future usage,
  // as that check is very expensive. Prefer using the cached value over calling
  // this for every auth attempt.
  static void CacheIfBiometricsAvailable(
      AuthenticatorWinInterface* authenticator);

 private:
  // Records authentication status and executes |callback| with |success|
  // parameter.
  void OnAuthenticationCompleted(base::OnceCallback<void(bool)> callback,
                                 bool success);

  std::unique_ptr<AuthenticatorWinInterface> authenticator_;

  // Factory for weak pointers to this class.
  base::WeakPtrFactory<DeviceAuthenticatorWin> weak_ptr_factory_{this};
};

#endif  // CHROME_BROWSER_DEVICE_REAUTH_WIN_DEVICE_AUTHENTICATOR_WIN_H_
