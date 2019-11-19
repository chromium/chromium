// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_LOGIN_USER_FLOW_H_
#define CHROME_BROWSER_CHROMEOS_LOGIN_USER_FLOW_H_

#include "base/compiler_specific.h"
#include "chrome/browser/profiles/profile.h"
#include "chromeos/login/auth/auth_status_consumer.h"
#include "components/account_id/account_id.h"
#include "components/user_manager/user.h"

namespace chromeos {

class UserContext;

class LoginDisplayHost;
// Defines possible variants of user flow upon logging in.
// See UserManager::SetUserFlow for usage contract.
class UserFlow {
 public:
  UserFlow();
  virtual ~UserFlow() = 0;

  // Provides ability to alter command line before session has started.
  virtual void AppendAdditionalCommandLineSwitches() = 0;

  // Indicates if screen locking should be enabled or disabled for a flow.
  virtual bool CanLockScreen() = 0;
  virtual bool CanStartArc() = 0;

  // Whether or not the settings icon should be enabled in the system tray menu.
  virtual bool ShouldEnableSettings() = 0;

  // Whether or not the notifications tray should be visible.
  virtual bool ShouldShowNotificationTray() = 0;

  virtual bool ShouldLaunchBrowser() = 0;
  virtual bool ShouldSkipPostLoginScreens() = 0;
  virtual bool SupportsEarlyRestartToApplyFlags() = 0;
  virtual bool AllowsNotificationBalloons() = 0;
  virtual bool HandleLoginFailure(const AuthFailure& failure) = 0;
  virtual void HandleLoginSuccess(const UserContext& context) = 0;
  virtual void HandleOAuthTokenStatusChange(
      user_manager::User::OAuthTokenStatus status) = 0;
  virtual void LaunchExtraSteps(Profile* profile) = 0;
  void SetHost(LoginDisplayHost* host);

  LoginDisplayHost* host() { return host_; }

 private:
  LoginDisplayHost* host_;
};

// UserFlow implementation for regular login flow.
class DefaultUserFlow : public UserFlow {
 public:
  ~DefaultUserFlow() override;

  // UserFlow:
  void AppendAdditionalCommandLineSwitches() override;
  bool CanLockScreen() override;
  bool CanStartArc() override;
  bool ShouldEnableSettings() override;
  bool ShouldShowNotificationTray() override;
  bool ShouldLaunchBrowser() override;
  bool ShouldSkipPostLoginScreens() override;
  bool SupportsEarlyRestartToApplyFlags() override;
  bool AllowsNotificationBalloons() override;
  bool HandleLoginFailure(const AuthFailure& failure) override;
  void HandleLoginSuccess(const UserContext& context) override;
  void HandleOAuthTokenStatusChange(
      user_manager::User::OAuthTokenStatus status) override;
  void LaunchExtraSteps(Profile* profile) override;
};

// UserFlow stub for non-regular flows.
class ExtendedUserFlow : public UserFlow {
 public:
  explicit ExtendedUserFlow(const AccountId& account_id);
  ~ExtendedUserFlow() override;

  // UserFlow:
  void AppendAdditionalCommandLineSwitches() override;
  bool ShouldEnableSettings() override;
  bool ShouldShowNotificationTray() override;
  bool AllowsNotificationBalloons() override;
  void HandleOAuthTokenStatusChange(
      user_manager::User::OAuthTokenStatus status) override;

 protected:
  // Subclasses can call this method to unregister flow in the next event.
  virtual void UnregisterFlowSoon();
  const AccountId& account_id() { return account_id_; }

 private:
  const AccountId account_id_;
};

}  // namespace chromeos

#endif  // CHROME_BROWSER_CHROMEOS_LOGIN_USER_FLOW_H_
