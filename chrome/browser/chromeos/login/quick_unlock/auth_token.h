// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_LOGIN_QUICK_UNLOCK_AUTH_TOKEN_H_
#define CHROME_BROWSER_CHROMEOS_LOGIN_QUICK_UNLOCK_AUTH_TOKEN_H_

#include <memory>

#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "base/optional.h"
#include "base/time/time.h"
#include "base/unguessable_token.h"

namespace chromeos {

class UserContext;
class QuickUnlockStorageUnitTest;

namespace quick_unlock {

// Security token with an identifyier string and a predetermined life time,
// after which it is irrevocably "reset" and can be considered invalid. In
// particular this makes the identifier string inaccessible from outside the
// class.
class AuthToken {
 public:
  // How long the token lives.
  static const int kTokenExpirationSeconds;

  explicit AuthToken(const chromeos::UserContext& user_context);
  ~AuthToken();

  // An unguessable identifier that can be passed to webui to verify the token
  // instance has not changed. Returns nullopt if Reset() was called.
  base::Optional<std::string> Identifier() const;

  // Time since token was created or |base::nullopt| if Reset() was called.
  base::Optional<base::TimeDelta> GetAge() const;

  // The UserContext returned here can be null if Reset() was called.
  const chromeos::UserContext* user_context() const {
    return user_context_.get();
  }

 private:
  friend class chromeos::QuickUnlockStorageUnitTest;

  // Expires the token. In particular this makes the identifier string
  // inaccessible from outside the class.
  void Reset();

  base::UnguessableToken identifier_;
  base::TimeTicks creation_time_;
  std::unique_ptr<chromeos::UserContext> user_context_;

  base::WeakPtrFactory<AuthToken> weak_factory_{this};

  DISALLOW_COPY_AND_ASSIGN(AuthToken);
};

}  // namespace quick_unlock
}  // namespace chromeos

#endif  // CHROME_BROWSER_CHROMEOS_LOGIN_QUICK_UNLOCK_AUTH_TOKEN_H_
