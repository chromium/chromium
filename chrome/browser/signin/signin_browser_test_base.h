// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SIGNIN_SIGNIN_BROWSER_TEST_BASE_H_
#define CHROME_BROWSER_SIGNIN_SIGNIN_BROWSER_TEST_BASE_H_

#include "chrome/browser/signin/identity_test_environment_profile_adaptor.h"

#include "chrome/test/base/in_process_browser_test.h"
#include "components/signin/public/identity_manager/account_info.h"
#include "components/signin/public/identity_manager/identity_test_environment.h"
#include "services/network/test/test_url_loader_factory.h"

// Base class for browser tests that rely on accounts.
//
// Sets up the test environment and account consistency to simplify the
// management of accounts and cookies state.
class SigninBrowserTestBase : public InProcessBrowserTest {
 public:
  SigninBrowserTestBase();
  ~SigninBrowserTestBase() override;

  // Sets accounts in the environment to new ones based on the given `emails`,
  // and makes the first one primary.
  // Returns `AccountInfo`s for each added account, in the same order as
  // `emails`.
  std::vector<AccountInfo> SetAccounts(const std::vector<std::string>& emails);

  signin::IdentityTestEnvironment* identity_test_env() const {
    return identity_test_env_profile_adaptor_->identity_test_env();
  }

  signin::IdentityManager* identity_manager() const {
    return identity_test_env()->identity_manager();
  }

  network::TestURLLoaderFactory* test_url_loader_factory() {
    return &test_url_loader_factory_;
  }

 protected:
  // InProcessBrowserTest:
  void SetUpOnMainThread() override;
  void TearDownOnMainThread() override;
  void SetUpInProcessBrowserTestFixture() override;

  virtual void OnWillCreateBrowserContextServices(
      content::BrowserContext* context);

 private:
  std::unique_ptr<IdentityTestEnvironmentProfileAdaptor>
      identity_test_env_profile_adaptor_;
  base::CallbackListSubscription create_services_subscription_;

  network::TestURLLoaderFactory test_url_loader_factory_;
};

#endif  // CHROME_BROWSER_SIGNIN_SIGNIN_BROWSER_TEST_BASE_H_
