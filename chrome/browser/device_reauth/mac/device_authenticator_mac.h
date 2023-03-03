// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_DEVICE_REAUTH_MAC_DEVICE_AUTHENTICATOR_MAC_H_
#define CHROME_BROWSER_DEVICE_REAUTH_MAC_DEVICE_AUTHENTICATOR_MAC_H_

#include "base/functional/callback.h"
#include "base/sequence_checker.h"
#include "chrome/browser/device_reauth/chrome_device_authenticator_common.h"
#include "chrome/browser/device_reauth/chrome_device_authenticator_factory.h"
#include "components/device_reauth/device_authenticator.h"

class AuthenticatorMacInterface;

namespace device {
namespace fido {
namespace mac {
class TouchIdContext;
}  // namespace mac
}  // namespace fido
}  // namespace device

class DeviceAuthenticatorMac : public ChromeDeviceAuthenticatorCommon {
 public:
  // Creates an instance of DeviceAuthenticatorWin for testing purposes
  // only.
  static scoped_refptr<DeviceAuthenticatorMac> CreateForTesting(
      std::unique_ptr<AuthenticatorMacInterface> authenticator);

  bool CanAuthenticateWithBiometrics() override;

  // Triggers an authentication flow based on biometrics, with the
  // screen lock as fallback. Note: this only supports one authentication
  // request at a time.
  // |use_last_valid_auth| if set to false, ignores the grace 60 seconds
  // period between the last valid authentication and the current
  // authentication, and re-invokes system authentication.
  void Authenticate(device_reauth::DeviceAuthRequester requester,
                    AuthenticateCallback callback,
                    bool use_last_valid_auth) override;

  // Triggers an OS-level authentication flow.
  // If biometrics are available, it creates touchIdAuthentication object,
  // request user to authenticate(proper box with that information will appear
  // on the screen and the `message` will be displayed there) using his touchId
  // or if it's not setUp default one with password will appear. If biometrics
  // aren't available, it falls back to the legacy authentication flow.

  void AuthenticateWithMessage(const std::u16string& message,
                               AuthenticateCallback callback) override;

  // Should be called by the object using the authenticator if the purpose
  // for which the auth was requested becomes obsolete or the object is
  // destroyed.
  void Cancel(device_reauth::DeviceAuthRequester requester) override;

 private:
  friend class ChromeDeviceAuthenticatorFactory;
  explicit DeviceAuthenticatorMac(
      std::unique_ptr<AuthenticatorMacInterface> authenticator);
  ~DeviceAuthenticatorMac() override;

  // Called when the authentication completes with the result |success|.
  void OnAuthenticationCompleted(bool success);

  // Callback to be executed after the authentication completes.
  AuthenticateCallback callback_;

  // TouchId authenticator object that will handle biometric authentication
  // itself.
  std::unique_ptr<device::fido::mac::TouchIdContext> touch_id_auth_context_;

  SEQUENCE_CHECKER(sequence_checker_);

  std::unique_ptr<AuthenticatorMacInterface> authenticator_;

  // Factory for weak pointers to this class.
  base::WeakPtrFactory<DeviceAuthenticatorMac> weak_ptr_factory_{this};
};

#endif  // CHROME_BROWSER_DEVICE_REAUTH_MAC_DEVICE_AUTHENTICATOR_MAC_H_
