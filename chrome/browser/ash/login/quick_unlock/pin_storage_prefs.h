// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_LOGIN_QUICK_UNLOCK_PIN_STORAGE_PREFS_H_
#define CHROME_BROWSER_ASH_LOGIN_QUICK_UNLOCK_PIN_STORAGE_PREFS_H_

#include <string>

#include "base/gtest_prod_util.h"
#include "base/macros.h"
#include "base/time/time.h"
#include "chromeos/login/auth/key.h"

class PrefRegistrySimple;
class PrefService;

namespace chromeos {
namespace quick_unlock {

class PinStoragePrefs {
 public:
  // TODO(sammiequon): Pull this value in from policy. See
  // https://crbug.com/612271.
  static const int kMaximumUnlockAttempts = 3;

  // Registers profile prefs.
  static void RegisterProfilePrefs(PrefRegistrySimple* registry);

  explicit PinStoragePrefs(PrefService* pref_service);
  ~PinStoragePrefs();

  // Add a PIN unlock attempt count.
  void AddUnlockAttempt();
  // Reset the unlock attempt count to 0. Not applicable to all implementations.
  void ResetUnlockAttemptCount();
  // Returns the number of unlock attempts.
  int unlock_attempt_count() const { return unlock_attempt_count_; }

  // Returns true if a pin is set.
  bool IsPinSet() const;
  // Sets the pin to the given value; IsPinSet will return true.
  void SetPin(const std::string& pin);
  // Removes the pin; IsPinSet will return false.
  void RemovePin();

  // Is PIN entry currently available?
  bool IsPinAuthenticationAvailable() const;

  // Tries to authenticate the given pin. This will consume an unlock attempt.
  // This always returns false if IsPinAuthenticationAvailable returns false.
  bool TryAuthenticatePin(const Key& key);

  // Return the stored salt/secret. This is fetched directly from pref_service_.
  std::string PinSalt() const;
  std::string PinSecret() const;

 private:
  PrefService* pref_service_;
  int unlock_attempt_count_ = 0;

  DISALLOW_COPY_AND_ASSIGN(PinStoragePrefs);
};

}  // namespace quick_unlock
}  // namespace chromeos

#endif  // CHROME_BROWSER_ASH_LOGIN_QUICK_UNLOCK_PIN_STORAGE_PREFS_H_
