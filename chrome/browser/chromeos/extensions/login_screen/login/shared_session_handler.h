// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_EXTENSIONS_LOGIN_SCREEN_LOGIN_SHARED_SESSION_HANDLER_H_
#define CHROME_BROWSER_CHROMEOS_EXTENSIONS_LOGIN_SCREEN_LOGIN_SHARED_SESSION_HANDLER_H_

#include <optional>
#include <string>

#include "base/functional/callback.h"
#include "base/no_destructor.h"

namespace chromeos {

// Singleton which manages the logic for the shared session methods of the
// Login API.
class SharedSessionHandler {
 public:
  // Gets the global instance of `SharedSessionHandler`, and creates one if
  // there is none.
  static SharedSessionHandler* Get();

  SharedSessionHandler(const SharedSessionHandler&) = delete;
  SharedSessionHandler& operator=(const SharedSessionHandler&) = delete;

  using CallbackWithOptionalError =
      base::OnceCallback<void(const std::optional<std::string>&)>;

  // Starts a lockable Managed Guest Session with a randomly generated secret
  // as the Cryptohome key. The session secret is stored in memory to be used
  // later for unlocking the session.
  // An initial shared session is started with `password` as the password.
  // Returns the error encountered, if any.
  std::optional<std::string> LaunchSharedManagedGuestSession(
      const std::string& password);

  // Enters a new shared session. Can only be called from the lock screen. The
  // session can only be unlocked by calling `UnlockSharedSession()` with the
  // same password.
  void EnterSharedSession(const std::string& password,
                          CallbackWithOptionalError callback);

  // Unlocks an existing shared session. Can only be called from the lock
  // screen.
  void UnlockSharedSession(const std::string& password,
                           CallbackWithOptionalError callback);

  // Ends an existing shared session. The session will be locked if it is not
  // already on the lock screen.
  void EndSharedSession(CallbackWithOptionalError callback);

  const std::string& GetSessionSecretForTesting() const;

  const std::string& GetUserSecretHashForTesting() const;

  const std::string& GetUserSecretSaltForTesting() const;

  void ResetStateForTesting();

 private:
  friend class base::NoDestructor<SharedSessionHandler>;

  SharedSessionHandler();
  ~SharedSessionHandler();

  std::optional<std::string> GetHashFromScrypt(const std::string& password,
                                               const std::string& salt);

  void UnlockWithSessionSecret(base::OnceCallback<void(bool)> callback);

  bool CreateAndSetUserSecretHashAndSalt(const std::string& password);

  void OnAuthenticateDone(CallbackWithOptionalError callback,
                          bool auth_success);

  void OnCleanupDone(CallbackWithOptionalError callback,
                     const std::optional<std::string>& errors);

  std::string GenerateRandomString(size_t size);

  std::string session_secret_;
  std::string user_secret_hash_;
  std::string user_secret_salt_;
};

}  // namespace chromeos

#endif  // CHROME_BROWSER_CHROMEOS_EXTENSIONS_LOGIN_SCREEN_LOGIN_SHARED_SESSION_HANDLER_H_
