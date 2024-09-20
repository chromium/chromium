// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_DEVICE_REAUTH_CHROMEOS_AUTHENTICATOR_CHROMEOS_H_
#define CHROME_BROWSER_DEVICE_REAUTH_CHROMEOS_AUTHENTICATOR_CHROMEOS_H_

#include "base/functional/callback.h"
#include "base/functional/callback_forward.h"

// Enum specifying possible states of biometric authentication availability on
// ChromeOS. These values are persisted to logs. Entries should not be
// renumbered and numeric values should never be reused.
enum class BiometricsStatusChromeOS {
  kAvailable = 1,
  kUnavailable = 2,
  kNotConfiguredForUser = 3,
  kMaxValue = kNotConfiguredForUser,
};

// This interface is need to simplify testing as chromeos authentication happens
// through free function which is hard to mock.
class AuthenticatorChromeOSInterface {
 public:
  using AvailabilityCallback =
      base::OnceCallback<void(BiometricsStatusChromeOS)>;
  virtual ~AuthenticatorChromeOSInterface() = default;
  virtual void AuthenticateUser(const std::u16string& message,
                                base::OnceCallback<void(bool)> callback) = 0;
  virtual BiometricsStatusChromeOS CheckIfBiometricsAvailable() = 0;
};

// Implementation of the interface that handles communication with the OS.
class AuthenticatorChromeOS : public AuthenticatorChromeOSInterface {
 public:
  AuthenticatorChromeOS();
  ~AuthenticatorChromeOS() override;

  AuthenticatorChromeOS(const AuthenticatorChromeOS&) = delete;
  AuthenticatorChromeOS& operator=(const AuthenticatorChromeOS&) = delete;

  void AuthenticateUser(
      const std::u16string& message,
      base::OnceCallback<void(bool)> result_callback) override;

  // Returns the status for biometric authentication availability on the
  // chromebook.
  BiometricsStatusChromeOS CheckIfBiometricsAvailable() override;
};
#endif  // CHROME_BROWSER_DEVICE_REAUTH_CHROMEOS_AUTHENTICATOR_CHROMEOS_H_
