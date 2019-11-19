// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_POLICY_LOGIN_POLICY_TEST_BASE_H_
#define CHROME_BROWSER_CHROMEOS_POLICY_LOGIN_POLICY_TEST_BASE_H_

#include <memory>
#include <string>

#include "base/macros.h"
#include "chrome/browser/chromeos/login/test/fake_gaia_mixin.h"
#include "chrome/browser/chromeos/login/test/local_policy_test_server_mixin.h"
#include "chrome/browser/chromeos/login/test/oobe_base_test.h"

namespace base {
class DictionaryValue;
}

namespace policy {

class UserPolicyTestHelper;

// This class can be used to implement tests which need policy to be set prior
// to login.
// TODO (crbug/1014663): Deprecate this class in favor of LoggedInUserMixin.
class LoginPolicyTestBase : public chromeos::OobeBaseTest {
 protected:
  LoginPolicyTestBase();
  ~LoginPolicyTestBase() override;

  // chromeos::OobeBaseTest::
  void SetUpInProcessBrowserTestFixture() override;
  void SetUpOnMainThread() override;

  virtual void GetMandatoryPoliciesValue(base::DictionaryValue* policy) const;
  virtual void GetRecommendedPoliciesValue(base::DictionaryValue* policy) const;
  virtual std::string GetAccount() const;
  virtual std::string GetIdToken() const;

  UserPolicyTestHelper* user_policy_helper() {
    return user_policy_helper_.get();
  }

  Profile* GetProfileForActiveUser();

  void SkipToLoginScreen();

  // Triggers the login, but does not wait for a user session to start.
  void TriggerLogIn(const std::string& user_id,
                    const std::string& password,
                    const std::string& services);

  // Triggers the login and waits for a user session to start.
  void LogIn(const std::string& user_id,
             const std::string& password,
             const std::string& services);

  static const char kAccountPassword[];
  static const char kAccountId[];
  static const char kEmptyServices[];

  chromeos::FakeGaiaMixin fake_gaia_{&mixin_host_, embedded_test_server()};
  chromeos::LocalPolicyTestServerMixin local_policy_server_{&mixin_host_};

 private:
  void SetUpGaiaServerWithAccessTokens();
  void SetMergeSessionParams();

  std::unique_ptr<UserPolicyTestHelper> user_policy_helper_;

  DISALLOW_COPY_AND_ASSIGN(LoginPolicyTestBase);
};

}  // namespace policy

#endif  // CHROME_BROWSER_CHROMEOS_POLICY_LOGIN_POLICY_TEST_BASE_H_
