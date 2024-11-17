// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_LOGIN_ENROLLMENT_TIMEBOUND_USER_CONTEXT_HOLDER_H_
#define CHROME_BROWSER_ASH_LOGIN_ENROLLMENT_TIMEBOUND_USER_CONTEXT_HOLDER_H_

#include <memory>

#include "base/component_export.h"
#include "base/functional/callback.h"
#include "base/functional/callback_forward.h"
#include "base/memory/weak_ptr.h"
#include "base/scoped_observation.h"
#include "base/time/time.h"
#include "chrome/browser/ash/login/enrollment/oauth2_token_revoker.h"
#include "chromeos/ash/components/login/auth/public/user_context.h"
#include "components/account_id/account_id.h"
#include "components/session_manager/core/session_manager.h"
#include "components/session_manager/core/session_manager_observer.h"

namespace ash {

// This class is used to revoke OAuth2 token and delete a UserContext once one
// of the following conditions is met:
//   1. An expiration period elapses since the creation of this object.
//   2. Session state changes to LOGGED_IN_NOT_ACTIVE or ACTIVE and the
//      UserContext wasn't consumed.
//   3. This object is destroyed.
class TimeboundUserContextHolder
    : public session_manager::SessionManagerObserver {
 public:
  static constexpr base::TimeDelta kCredentialsVlidityPeriod =
      base::Minutes(10);

  explicit TimeboundUserContextHolder(
      std::unique_ptr<UserContext> user_context);
  TimeboundUserContextHolder(const TimeboundUserContextHolder& other) = delete;
  TimeboundUserContextHolder(const TimeboundUserContextHolder&& other) = delete;
  ~TimeboundUserContextHolder() override;

  // Transfers the ownership of the user context to the caller.
  std::unique_ptr<UserContext> GetUserContext() {
    CHECK(user_context_);
    return std::move(user_context_);
  }

  bool HasUserContext() const { return static_cast<bool>(user_context_); }
  // Before calling the getters make sure that the holder actually holds the
  // user context.
  const AccountId& GetAccountId() const {
    CHECK(user_context_);
    return user_context_->GetAccountId();
  }
  std::string GetRefreshToken() const {
    CHECK(user_context_);
    return user_context_->GetRefreshToken();
  }
  std::optional<PasswordInput> GetPassword() const {
    CHECK(user_context_);
    return user_context_->GetPassword();
  }
  std::string GetGaiaID() const {
    CHECK(user_context_);
    return user_context_->GetGaiaID();
  }

  void TriggerTimeoutForTesting() { OnTimeout(); }
  void InjectTokenRevokerForTesting(
      std::unique_ptr<OAuth2TokenRevokerBase> token_revoker) {
    token_revoker_ = std::move(token_revoker);
  }

 private:
  // session_manager::SessionManagerObserver:
  void OnSessionStateChanged() override;
  void OnTimeout();
  void ClearUserContext();

  std::unique_ptr<UserContext> user_context_;
  std::unique_ptr<OAuth2TokenRevokerBase> token_revoker_;
  base::ScopedObservation<session_manager::SessionManager,
                          session_manager::SessionManagerObserver>
      session_observation_{this};

  base::WeakPtrFactory<TimeboundUserContextHolder> weak_ptr_factory_{this};
};

}  // namespace ash

#endif  // CHROME_BROWSER_ASH_LOGIN_ENROLLMENT_TIMEBOUND_USER_CONTEXT_HOLDER_H_
