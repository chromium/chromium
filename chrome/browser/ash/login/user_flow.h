// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_LOGIN_USER_FLOW_H_
#define CHROME_BROWSER_ASH_LOGIN_USER_FLOW_H_

#include "chromeos/ash/components/login/auth/auth_status_consumer.h"
#include "components/account_id/account_id.h"
#include "components/user_manager/user.h"

namespace ash {

class UserContext;

// Defines possible variants of user flow upon logging in.
// See UserManager::SetUserFlow for usage contract.
class UserFlow {
 public:
  UserFlow();
  virtual ~UserFlow() = 0;

  virtual bool HandleLoginFailure(const AuthFailure& failure) = 0;
  virtual void HandleLoginSuccess(const UserContext& context) = 0;
};

// UserFlow implementation for regular login flow.
class DefaultUserFlow : public UserFlow {
 public:
  ~DefaultUserFlow() override;

  // UserFlow:
  bool HandleLoginFailure(const AuthFailure& failure) override;
  void HandleLoginSuccess(const UserContext& context) override;
};

// UserFlow stub for non-regular flows.
class ExtendedUserFlow : public UserFlow {
 public:
  explicit ExtendedUserFlow(const AccountId& account_id);
  ~ExtendedUserFlow() override;

 protected:
  // Subclasses can call this method to unregister flow in the next event.
  virtual void UnregisterFlowSoon();
  const AccountId& account_id() { return account_id_; }

 private:
  const AccountId account_id_;
};

}  // namespace ash

#endif  // CHROME_BROWSER_ASH_LOGIN_USER_FLOW_H_
