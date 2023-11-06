// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_PASSWORD_MANAGER_PASSWORD_MANAGER_SIGNIN_INTERCEPT_TEST_HELPER_H_
#define CHROME_BROWSER_PASSWORD_MANAGER_PASSWORD_MANAGER_SIGNIN_INTERCEPT_TEST_HELPER_H_

#include <string>

#include "base/memory/raw_ptr.h"
#include "base/memory/scoped_refptr.h"
#include "components/password_manager/core/browser/password_store/test_password_store.h"
#include "google_apis/gaia/core_account_id.h"

namespace base {
class CommandLine;
}

namespace content {
class WebContents;
}

namespace net {
namespace test_server {
class EmbeddedTestServer;
}
}  // namespace net

class DiceWebSigninInterceptor;
class Profile;

// Helper functions shared by password manager browser tests and interactive UI
// tests related to signin interception.
class PasswordManagerSigninInterceptTestHelper {
 public:
  explicit PasswordManagerSigninInterceptTestHelper(
      net::test_server::EmbeddedTestServer* https_test_server);

  ~PasswordManagerSigninInterceptTestHelper();

  void SetUpCommandLine(base::CommandLine* command_line);
  void SetUpOnMainThread();

  // Pre-populates the password store with Gaia credentials.
  void StoreGaiaCredentials(
      scoped_refptr<password_manager::TestPasswordStore> password_store);

  void NavigateToGaiaSigninPage(content::WebContents* contents);

  // Create another profile with the same Gaia account, so that the profile
  // switch promo can be shown.
  void SetupProfilesForInterception(Profile* current_profile);

  // Adds the account in the profile.
  CoreAccountId AddGaiaAccountToProfile(Profile* profile,
                                        const std::string& email,
                                        const std::string& gaia_id);

  DiceWebSigninInterceptor* GetSigninInterceptor(Profile* profile);

  std::string gaia_username() const;
  std::string gaia_email() const;
  std::string gaia_id() const;

 private:
  raw_ptr<const net::test_server::EmbeddedTestServer> https_test_server_;
};

#endif  // CHROME_BROWSER_PASSWORD_MANAGER_PASSWORD_MANAGER_SIGNIN_INTERCEPT_TEST_HELPER_H_
