// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_LOGIN_QUICK_UNLOCK_QUICK_UNLOCK_STORAGE_H_
#define CHROME_BROWSER_CHROMEOS_LOGIN_QUICK_UNLOCK_QUICK_UNLOCK_STORAGE_H_

#include "chrome/browser/chromeos/login/quick_unlock/auth_token.h"
#include "chrome/browser/chromeos/login/quick_unlock/fingerprint_storage.h"
#include "chrome/browser/chromeos/login/quick_unlock/pin_storage_prefs.h"
#include "chromeos/login/auth/user_context.h"
#include "components/keyed_service/core/keyed_service.h"

class Profile;

namespace base {
class Time;
}

namespace chromeos {

class QuickUnlockStorageTestApi;
class QuickUnlockStorageUnitTest;

namespace quick_unlock {

// Helper class for managing state for quick unlock services (pin and
// fingerprint), and general lock screen management (tokens for extension API
// authentication used by Settings).
class QuickUnlockStorage : public KeyedService {
 public:
  explicit QuickUnlockStorage(Profile* profile);
  ~QuickUnlockStorage() override;

  // Mark that the user has had a strong authentication. This means
  // that they authenticated with their password, for example. Quick
  // unlock will timeout after a delay.
  void MarkStrongAuth();

  // Returns true if the user has been strongly authenticated.
  bool HasStrongAuth() const;

  // Returns the time since the last strong authentication. This should not be
  // called if HasStrongAuth returns false.
  base::TimeDelta TimeSinceLastStrongAuth() const;

  // Returns the time until next strong authentication required. This should
  // not be called if HasStrongAuth returns false.
  base::TimeDelta TimeUntilNextStrongAuth() const;

  // Returns true if fingerprint unlock is currently available.
  // This checks whether there's fingerprint setup, as well as HasStrongAuth.
  bool IsFingerprintAuthenticationAvailable() const;

  // Returns true if PIN unlock is currently available.
  bool IsPinAuthenticationAvailable() const;

  // Tries to authenticate the given pin. This will consume a pin unlock
  // attempt. This always returns false if HasStrongAuth returns false.
  bool TryAuthenticatePin(const Key& key);

  // Creates a new authentication token to be used by the quickSettingsPrivate
  // API for authenticating requests. Resets the expiration timer and
  // invalidates any previously issued tokens.
  std::string CreateAuthToken(const chromeos::UserContext& user_context);

  // Returns true if the current authentication token has expired.
  bool GetAuthTokenExpired();

  // Checks the token expiration time and returns the current authentication
  // token if valid, or an empty string if it has expired.
  std::string GetAuthToken();

  // Fetch the user context if |auth_token| is valid. May return null.
  UserContext* GetUserContext(const std::string& auth_token);

  FingerprintStorage* fingerprint_storage() {
    return fingerprint_storage_.get();
  }

  // Fetch the underlying pref pin storage. If iteracting with pin generally,
  // use the PinBackend APIs.
  PinStoragePrefs* pin_storage_prefs() { return pin_storage_prefs_.get(); }

 private:
  friend class chromeos::QuickUnlockStorageTestApi;
  friend class chromeos::QuickUnlockStorageUnitTest;

  // KeyedService:
  void Shutdown() override;

  Profile* const profile_;
  base::TimeTicks last_strong_auth_;
  std::unique_ptr<FingerprintStorage> fingerprint_storage_;
  std::unique_ptr<PinStoragePrefs> pin_storage_prefs_;
  std::unique_ptr<AuthToken> auth_token_;

  DISALLOW_COPY_AND_ASSIGN(QuickUnlockStorage);
};

}  // namespace quick_unlock
}  // namespace chromeos

#endif  // CHROME_BROWSER_CHROMEOS_LOGIN_QUICK_UNLOCK_QUICK_UNLOCK_STORAGE_H_
