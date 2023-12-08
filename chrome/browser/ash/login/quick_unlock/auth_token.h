// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_LOGIN_QUICK_UNLOCK_AUTH_TOKEN_H_
#define CHROME_BROWSER_ASH_LOGIN_QUICK_UNLOCK_AUTH_TOKEN_H_

#include <memory>
#include <optional>

#include "base/memory/weak_ptr.h"
#include "base/time/time.h"
#include "base/unguessable_token.h"

namespace ash {

class UserContext;

namespace quick_unlock {

// Security token with an identifyier string and a predetermined life time,
// after which it is irrevocably "reset" and can be considered invalid. In
// particular this makes the identifier string inaccessible from outside the
// class.
class AuthToken {
 public:
  // How long the token lives.
  static const base::TimeDelta kTokenExpiration;

  explicit AuthToken(const UserContext& user_context);

  AuthToken(const AuthToken&) = delete;
  AuthToken& operator=(const AuthToken&) = delete;

  ~AuthToken();

  // An unguessable identifier that can be passed to webui to verify the token
  // instance has not changed. Returns nullopt if Reset() was called.
  std::optional<std::string> Identifier() const;

  // Similar to the above, but returns the strongly typed
  // `base::UnguessableToken` instead
  std::optional<base::UnguessableToken> GetUnguessableToken() const;

  // Time since token was created or `std::nullopt` if Reset() was called.
  std::optional<base::TimeDelta> GetAge() const;

  // The UserContext returned here can be null if Reset() was called.
  const UserContext* user_context() const { return user_context_.get(); }
  UserContext* user_context() { return user_context_.get(); }

  // Replace the user context that is stored with this token. If Reset() has
  // been called earlier, the call is ignored, and the user context passed to
  // this function is destroyed.
  void ReplaceUserContext(std::unique_ptr<UserContext>);

 private:
  friend class QuickUnlockStorageUnitTest;

  // Expires the token. In particular this makes the identifier string
  // inaccessible from outside the class.
  void Reset();

  base::UnguessableToken identifier_;
  base::TimeTicks creation_time_;
  std::unique_ptr<UserContext> user_context_;

  base::WeakPtrFactory<AuthToken> weak_factory_{this};
};

}  // namespace quick_unlock
}  // namespace ash

#endif  // CHROME_BROWSER_ASH_LOGIN_QUICK_UNLOCK_AUTH_TOKEN_H_
