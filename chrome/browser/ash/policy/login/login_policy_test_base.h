// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_POLICY_LOGIN_LOGIN_POLICY_TEST_BASE_H_
#define CHROME_BROWSER_ASH_POLICY_LOGIN_LOGIN_POLICY_TEST_BASE_H_

#include <memory>
#include <string>

#include "chrome/browser/ash/login/test/cryptohome_mixin.h"
#include "chrome/browser/ash/login/test/login_manager_mixin.h"
#include "chrome/browser/ash/login/test/oobe_base_test.h"
#include "chrome/browser/ash/policy/test_support/embedded_policy_test_server_mixin.h"
#include "chrome/test/base/fake_gaia_mixin.h"
#include "components/account_id/account_id.h"

namespace enterprise_management {
class CloudPolicySettings;
}

namespace policy {

class UserPolicyTestHelper;

// This class can be used to implement tests which need policy to be set prior
// to login.
// TODO (crbug/1014663): Deprecate this class in favor of LoggedInUserMixin.
class LoginPolicyTestBase : public ash::OobeBaseTest {
 public:
  LoginPolicyTestBase(const LoginPolicyTestBase&) = delete;
  LoginPolicyTestBase& operator=(const LoginPolicyTestBase&) = delete;

 protected:
  LoginPolicyTestBase();
  ~LoginPolicyTestBase() override;

  // ash::OobeBaseTest:
  void SetUpCommandLine(base::CommandLine* command_line) override;
  void SetUpInProcessBrowserTestFixture() override;
  void SetUpOnMainThread() override;

  virtual void GetPolicySettings(
      enterprise_management::CloudPolicySettings* settings) const;
  virtual std::string GetIdToken() const;

  UserPolicyTestHelper* user_policy_helper() {
    return user_policy_helper_.get();
  }

  Profile* GetProfileForActiveUser();

  void SkipToLoginScreen();

  // Triggers the login, but does not wait for a user session to start.
  void TriggerLogIn();

  // Triggers the login and waits for a user session to start.
  void LogIn();

  const AccountId& account_id() const { return account_id_; }

  FakeGaiaMixin fake_gaia_{&mixin_host_};
  ash::EmbeddedPolicyTestServerMixin policy_test_server_mixin_{&mixin_host_};
  ash::LoginManagerMixin login_manager_{&mixin_host_};
  ash::CryptohomeMixin cryptohome_mixin_{&mixin_host_};

 private:
  void SetConfiguration();

  const AccountId account_id_;  // Test AccountId.
  std::unique_ptr<UserPolicyTestHelper> user_policy_helper_;
};

}  // namespace policy

#endif  // CHROME_BROWSER_ASH_POLICY_LOGIN_LOGIN_POLICY_TEST_BASE_H_
