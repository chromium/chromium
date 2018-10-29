// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_LOGIN_QUICK_UNLOCK_FINGERPRINT_STORAGE_H_
#define CHROME_BROWSER_CHROMEOS_LOGIN_QUICK_UNLOCK_FINGERPRINT_STORAGE_H_

#include "base/time/time.h"

class PrefRegistrySimple;
class Profile;

namespace chromeos {

class FingerprintStorageTestApi;

namespace quick_unlock {

class QuickUnlockStorage;

class FingerprintStorage {
 public:
  static const int kMaximumUnlockAttempts = 5;

  // Registers profile prefs.
  static void RegisterProfilePrefs(PrefRegistrySimple* registry);

  explicit FingerprintStorage(Profile* profile);
  ~FingerprintStorage();

  // Returns true if fingerprint unlock is currently available.
  // This does not check if strong auth is available.
  bool IsFingerprintAvailable() const;

  // Returns true if the user has fingerprint record registered.
  bool HasRecord() const;

  // Add a fingerprint unlock attempt count.
  void AddUnlockAttempt();

  // Reset the number of unlock attempts to 0.
  void ResetUnlockAttemptCount();

  // Returns true if the user has exceeded fingerprint unlock attempts.
  bool ExceededUnlockAttempts() const;

  int unlock_attempt_count() const { return unlock_attempt_count_; }

 private:
  friend class chromeos::FingerprintStorageTestApi;
  friend class QuickUnlockStorage;

  Profile* const profile_;
  // Number of fingerprint unlock attempt.
  int unlock_attempt_count_ = 0;

  DISALLOW_COPY_AND_ASSIGN(FingerprintStorage);
};

}  // namespace quick_unlock
}  // namespace chromeos

#endif  // CHROME_BROWSER_CHROMEOS_LOGIN_QUICK_UNLOCK_FINGERPRINT_STORAGE_H_
