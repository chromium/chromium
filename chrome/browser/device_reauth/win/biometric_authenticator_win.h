// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_DEVICE_REAUTH_WIN_BIOMETRIC_AUTHENTICATOR_WIN_H_
#define CHROME_BROWSER_DEVICE_REAUTH_WIN_BIOMETRIC_AUTHENTICATOR_WIN_H_

#include <memory>

#include "chrome/browser/device_reauth/chrome_biometric_authenticator_common.h"
#include "chrome/browser/device_reauth/chrome_biometric_authenticator_factory.h"
#include "chrome/browser/device_reauth/win/authenticator_win.h"
#include "components/device_reauth/biometric_authenticator.h"

class BiometricAuthenticatorWin : public ChromeBiometricAuthenticatorCommon {
 public:
  // Creates an instance of BiometricAuthenticatorWin for testing purposes
  // only.
  static scoped_refptr<BiometricAuthenticatorWin> CreateForTesting(
      std::unique_ptr<AuthenticatorWinInterface> authenticator);

  // Returns true, when biometrics are available.
  bool CanAuthenticate(
      device_reauth::BiometricAuthRequester requester) override;

  // Trigges an authentication flow based on biometrics.
  // Note: this only supports one authentication request at a time.
  // |use_last_valid_auth| if set to false, ignores the grace 60 seconds
  // period between the last valid authentication and the current
  // authentication, and re-invokes system authentication.
  void Authenticate(device_reauth::BiometricAuthRequester requester,
                    AuthenticateCallback callback,
                    bool use_last_valid_auth) override;

  // Trigges an authentication flow based on biometrics. Request user to
  // authenticate(a prompt with that information will appear on the screen and
  // the `message` will be displayed there) using their windows hello or if it's
  // not set up, default one with password will appear.
  void AuthenticateWithMessage(device_reauth::BiometricAuthRequester requester,
                               const std::u16string& message,
                               AuthenticateCallback callback) override;

  // Should be called by the object using the authenticator if the purpose
  // for which the auth was requested becomes obsolete or the object is
  // destroyed.
  void Cancel(device_reauth::BiometricAuthRequester requester) override;

  // Asks Windows if user has configured and enabled biometrics on
  // their machine. Stores the response in a local state pref for future usage,
  // as that check is very expensive. Prefer using the cached value over calling
  // this for every auth attempt.
  void CacheIfBiometricsAvailable();

 private:
  friend class ChromeBiometricAuthenticatorFactory;

  explicit BiometricAuthenticatorWin(
      std::unique_ptr<AuthenticatorWinInterface> authenticator);
  ~BiometricAuthenticatorWin() override;

  // Records authentication status and executes |callback| with |success|
  // parameter.
  void OnAuthenticationCompleted(base::OnceCallback<void(bool)> callback,
                                 bool success);

  std::unique_ptr<AuthenticatorWinInterface> authenticator_;

  // Factory for weak pointers to this class.
  base::WeakPtrFactory<BiometricAuthenticatorWin> weak_ptr_factory_{this};
};

#endif  // CHROME_BROWSER_DEVICE_REAUTH_WIN_BIOMETRIC_AUTHENTICATOR_WIN_H_
