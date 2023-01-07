// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_DEVICE_REAUTH_WIN_AUTHENTICATOR_WIN_H_
#define CHROME_BROWSER_DEVICE_REAUTH_WIN_AUTHENTICATOR_WIN_H_

#include <string>

#include "base/callback.h"

// This interface is need to simplify testing as windows authentication happens
// through free function which is hard to mock.
class AuthenticatorWinInterface {
 public:
  using AvailabilityCallback = base::OnceCallback<void(bool)>;

  virtual ~AuthenticatorWinInterface() = default;
  virtual bool AuthenticateUser(const std::u16string& message) = 0;
  virtual void CheckIfBiometricsAvailable(AvailabilityCallback callback) = 0;
};

// Implementation of the interface that handles communication with the OS.
class AuthenticatorWin : public AuthenticatorWinInterface {
 public:
  AuthenticatorWin();
  ~AuthenticatorWin() override;

  AuthenticatorWin(const AuthenticatorWin&) = delete;
  AuthenticatorWin& operator=(const AuthenticatorWin&) = delete;

  bool AuthenticateUser(const std::u16string& message) override;

  // Runs `callback` with a biometrics availability as a parameter. Check
  // happens on the background thread as it is expensive.
  void CheckIfBiometricsAvailable(AvailabilityCallback callback) override;
};

#endif  // CHROME_BROWSER_DEVICE_REAUTH_WIN_AUTHENTICATOR_WIN_H_
