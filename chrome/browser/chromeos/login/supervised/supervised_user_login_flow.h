// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_LOGIN_SUPERVISED_SUPERVISED_USER_LOGIN_FLOW_H_
#define CHROME_BROWSER_CHROMEOS_LOGIN_SUPERVISED_SUPERVISED_USER_LOGIN_FLOW_H_

#include "base/compiler_specific.h"
#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "base/strings/string16.h"
#include "chrome/browser/chromeos/login/user_flow.h"
#include "chromeos/login/auth/extended_authenticator.h"
#include "chromeos/login/auth/user_context.h"

namespace chromeos {

// UserFlow implementation for signing in supervised user.
class SupervisedUserLoginFlow
    : public ExtendedUserFlow,
      public ExtendedAuthenticator::NewAuthStatusConsumer {
 public:
  explicit SupervisedUserLoginFlow(const AccountId& account_id);
  ~SupervisedUserLoginFlow() override;

  // ExtendedUserFlow overrides.
  void AppendAdditionalCommandLineSwitches() override;
  bool CanLockScreen() override;
  bool CanStartArc() override;
  bool ShouldLaunchBrowser() override;
  bool ShouldSkipPostLoginScreens() override;
  bool SupportsEarlyRestartToApplyFlags() override;
  bool HandleLoginFailure(const AuthFailure& failure) override;
  void HandleLoginSuccess(const UserContext& context) override;
  void LaunchExtraSteps(Profile* profile) override;

  // ExtendedAuthenticator::NewAuthStatusConsumer overrides.
  void OnAuthenticationFailure(ExtendedAuthenticator::AuthState state) override;

 private:
  void Launch();
  void Finish();

  void OnSyncSetupDataLoaded(const std::string& token);
  void ConfigureSync(const std::string& token);
  void OnPasswordChangeDataLoaded(const base::DictionaryValue* password_data);
  void OnPasswordChangeDataLoadFailed();
  void OnNewKeyAdded(std::unique_ptr<base::DictionaryValue> password_data);
  void OnOldKeyRemoved();
  void OnPasswordUpdated(std::unique_ptr<base::DictionaryValue> password_data);

  scoped_refptr<ExtendedAuthenticator> authenticator_;

  bool data_loaded_ = false;
  UserContext context_;
  Profile* profile_ = nullptr;
  base::WeakPtrFactory<SupervisedUserLoginFlow> weak_factory_{this};

  DISALLOW_COPY_AND_ASSIGN(SupervisedUserLoginFlow);
};

}  // namespace chromeos

#endif  // CHROME_BROWSER_CHROMEOS_LOGIN_SUPERVISED_SUPERVISED_USER_LOGIN_FLOW_H_
