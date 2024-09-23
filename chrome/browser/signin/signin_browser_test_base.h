// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SIGNIN_SIGNIN_BROWSER_TEST_BASE_H_
#define CHROME_BROWSER_SIGNIN_SIGNIN_BROWSER_TEST_BASE_H_

#include <concepts>
#include <memory>
#include <vector>

#include "base/memory/raw_ptr.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/profiles/profile_test_util.h"
#include "chrome/browser/signin/chrome_signin_client_factory.h"
#include "chrome/browser/signin/chrome_signin_client_test_util.h"
#include "chrome/browser/signin/identity_test_environment_profile_adaptor.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "components/signin/public/identity_manager/account_info.h"
#include "components/signin/public/identity_manager/identity_test_environment.h"
#include "components/trusted_vault/trusted_vault_histograms.h"
#include "services/network/test/test_url_loader_factory.h"

// Template for adding account management utilities to any test fixture which is
// derived from InProcessBrowserTest.
//
// Sets up the test environment and account consistency to simplify the
// management of accounts and cookies state.
//
// If you don't need to derive from some existing test class, prefer to use
// `SigninBrowserTestBase`.
template <typename T>
  requires(std::derived_from<T, InProcessBrowserTest>)
class SigninBrowserTestBaseT : public T {
 public:
  // `use_main_profile` controls whether the main profile is used (the default
  // `Profile` created by `InProcessBrowserTest`). On Lacros the main profile
  // behaves differently, and signout is not allowed.
  explicit SigninBrowserTestBaseT(bool use_main_profile = true)
      : use_main_profile_(use_main_profile) {}

  ~SigninBrowserTestBaseT() override = default;

  // Sets accounts in the environment to new ones based on the given `emails`.
  // The primary account is automatically set by Chrome when
  // `switches::kExplicitBrowserSigninUIOnDesktop` is disabled, and remains
  // unset when it is enabled. Returns `AccountInfo`s for each added account, in
  // the same order as `emails`.
  std::vector<AccountInfo> SetAccountsCookiesAndTokens(
      const std::vector<std::string>& emails) {
    auto account_availability_options =
        identity_test_env()
            ->CreateAccountAvailabilityOptionsBuilder()
            .WithCookie();

    std::vector<AccountInfo> accounts_info;
    for (const auto& email : emails) {
      accounts_info.push_back(identity_test_env()->MakeAccountAvailable(
          account_availability_options.Build(email)));
    }
    return accounts_info;
  }

  // Returns the profile attached to the `signin::IdentityTestEnvironment`. This
  // may not be the same as `browser()->profile()`.
  Profile* GetProfile() const { return profile_; }

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
  void SetUpOnMainThread() override {
    T::SetUpOnMainThread();

    ProfileManager* profile_manager = g_browser_process->profile_manager();
    base::FilePath profile_path =
        profile_manager->GenerateNextProfileDirectoryPath();
    profile_ = use_main_profile_ ? this->browser()->profile()
                                 : &profiles::testing::CreateProfileSync(
                                       profile_manager, profile_path);

    DCHECK(GetProfile());
#if (IS_CHROMEOS_LACROS)
    DCHECK_EQ(GetProfile()->IsMainProfile(), use_main_profile_);
#endif

    if (GetProfile()->IsOffTheRecord()) {
      return;
    }

    identity_test_env_profile_adaptor_ =
        std::make_unique<IdentityTestEnvironmentProfileAdaptor>(GetProfile());
    identity_test_env()->SetTestURLLoaderFactory(&test_url_loader_factory_);
  }

  void TearDownOnMainThread() override {
    // Must be destroyed before the Profile.
    identity_test_env_profile_adaptor_.reset();

    T::TearDownOnMainThread();
  }

  void SetUpInProcessBrowserTestFixture() override {
    T::SetUpInProcessBrowserTestFixture();

    create_services_subscription_ =
        BrowserContextDependencyManager::GetInstance()
            ->RegisterCreateServicesCallbackForTesting(base::BindRepeating(
                &SigninBrowserTestBaseT::OnWillCreateBrowserContextServices,
                base::Unretained(this)));
  }

  virtual void OnWillCreateBrowserContextServices(
      content::BrowserContext* context) {
    IdentityTestEnvironmentProfileAdaptor::
        SetIdentityTestEnvironmentFactoriesOnBrowserContext(context);
    ChromeSigninClientFactory::GetInstance()->SetTestingFactory(
        context, base::BindRepeating(&BuildChromeSigninClientWithURLLoader,
                                     &test_url_loader_factory_));
  }

 private:
  std::unique_ptr<IdentityTestEnvironmentProfileAdaptor>
      identity_test_env_profile_adaptor_;
  base::CallbackListSubscription create_services_subscription_;
  raw_ptr<Profile, DanglingUntriaged> profile_ = nullptr;
  const bool use_main_profile_;

  network::TestURLLoaderFactory test_url_loader_factory_;
};

// Base class for browser tests that rely on accounts.
//
// Sets up the test environment and account consistency to simplify the
// management of accounts and cookies state.
using SigninBrowserTestBase = SigninBrowserTestBaseT<InProcessBrowserTest>;

#endif  // CHROME_BROWSER_SIGNIN_SIGNIN_BROWSER_TEST_BASE_H_
