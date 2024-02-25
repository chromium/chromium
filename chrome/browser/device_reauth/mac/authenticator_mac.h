// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_DEVICE_REAUTH_MAC_AUTHENTICATOR_MAC_H_
#define CHROME_BROWSER_DEVICE_REAUTH_MAC_AUTHENTICATOR_MAC_H_

#import <string>

// This interface is need to simplify testing as mac authentication happens
// through free function which is hard to mock.
class AuthenticatorMacInterface {
 public:
  virtual ~AuthenticatorMacInterface() = default;
  virtual bool CheckIfBiometricsAvailable() = 0;
  virtual bool CheckIfBiometricsOrScreenLockAvailable() = 0;
  virtual bool AuthenticateUserWithNonBiometrics(
      const std::u16string& message) = 0;
};

// Implementation of the interface that handles communication with the OS.
class AuthenticatorMac : public AuthenticatorMacInterface {
 public:
  AuthenticatorMac();
  ~AuthenticatorMac() override;
  bool CheckIfBiometricsAvailable() override;
  bool CheckIfBiometricsOrScreenLockAvailable() override;
  bool AuthenticateUserWithNonBiometrics(
      const std::u16string& message) override;
};

#endif  // CHROME_BROWSER_DEVICE_REAUTH_MAC_AUTHENTICATOR_MAC_H_
