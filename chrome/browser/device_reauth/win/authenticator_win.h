// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_DEVICE_REAUTH_WIN_AUTHENTICATOR_WIN_H_
#define CHROME_BROWSER_DEVICE_REAUTH_WIN_AUTHENTICATOR_WIN_H_

#include <string>

#include "base/functional/callback.h"
#include "base/functional/callback_forward.h"

// Enum specifying possible states of biometric authentication availability on
// Windows. These values are persisted to logs. Entries should not be renumbered
// and numeric values should never be reused.
enum class BiometricAuthenticationStatusWin {
  kUnknown = 0,
  kAvailable = 1,
  kDeviceBusy = 2,
  kDisabledByPolicy = 3,
  kDeviceNotPresent = 4,
  kNotConfiguredForUser = 5,
  kMaxValue = kNotConfiguredForUser,
};

// Enum specifying possible results of Windows Hello authentication. These
// values are persisted to logs. Entries should not be renumbered and numeric
// values should never be reused.
enum class AuthenticationResultStatusWin {
  kVerified = 0,
  kDeviceNotPresent = 1,
  kNotConfiguredForUser = 2,
  kDisabledByPolicy = 3,
  kDeviceBusy = 4,
  kRetriesExhausted = 5,
  kCanceled = 6,
  kFailedToCreateFactory = 7,
  kFailedToCallAPI = 8,
  kFailedToPostTask = 9,
  kUnknown = 10,
  kAsyncOperationFailed = 11,
  kFailedToFindBrowser = 12,
  kFailedToFindHWNDForNativeWindow = 13,
  kMaxValue = kFailedToFindHWNDForNativeWindow,
};

// This interface is need to simplify testing as windows authentication happens
// through free function which is hard to mock.
class AuthenticatorWinInterface {
 public:
  using AvailabilityCallback =
      base::OnceCallback<void(BiometricAuthenticationStatusWin)>;

  virtual ~AuthenticatorWinInterface() = default;
  virtual void AuthenticateUser(const std::u16string& message,
                                base::OnceCallback<void(bool)> callback) = 0;
  virtual void CheckIfBiometricsAvailable(AvailabilityCallback callback) = 0;
  virtual bool CanAuthenticateWithScreenLock() = 0;
};

// Implementation of the interface that handles communication with the OS.
class AuthenticatorWin : public AuthenticatorWinInterface {
 public:
  AuthenticatorWin();
  ~AuthenticatorWin() override;

  AuthenticatorWin(const AuthenticatorWin&) = delete;
  AuthenticatorWin& operator=(const AuthenticatorWin&) = delete;

  void AuthenticateUser(
      const std::u16string& message,
      base::OnceCallback<void(bool)> result_callback) override;

  // Runs `callback` with a biometrics availability as a parameter. Check
  // happens on the background thread as it is expensive.
  void CheckIfBiometricsAvailable(AvailabilityCallback callback) override;

  // Returns true if there is screen lock present on the machine, false
  // otherwise.
  bool CanAuthenticateWithScreenLock() override;
};

#endif  // CHROME_BROWSER_DEVICE_REAUTH_WIN_AUTHENTICATOR_WIN_H_
