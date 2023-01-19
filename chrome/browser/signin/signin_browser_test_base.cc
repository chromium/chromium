// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/signin/signin_browser_test_base.h"

#include "base/check.h"
#include "build/chromeos_buildflags.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/profiles/profile_test_util.h"
#include "chrome/browser/signin/chrome_signin_client_factory.h"
#include "chrome/browser/signin/chrome_signin_client_test_util.h"
#include "chrome/browser/ui/browser.h"
#include "components/signin/public/base/account_consistency_method.h"
#include "components/signin/public/base/signin_buildflags.h"
#include "testing/gtest/include/gtest/gtest.h"

SigninBrowserTestBase::SigninBrowserTestBase(bool use_main_profile)
    : use_main_profile_(use_main_profile) {}

SigninBrowserTestBase::~SigninBrowserTestBase() = default;

std::vector<AccountInfo> SigninBrowserTestBase::SetAccounts(
    const std::vector<std::string>& emails) {
  return identity_test_env()->MakeAccountsAvailableWithCookies(emails);
}

void SigninBrowserTestBase::SetUpOnMainThread() {
  ProfileManager* profile_manager = g_browser_process->profile_manager();
  base::FilePath profile_path =
      profile_manager->GenerateNextProfileDirectoryPath();
  profile_ = use_main_profile_ ? browser()->profile()
                               : profiles::testing::CreateProfileSync(
                                     profile_manager, profile_path);

  DCHECK(GetProfile());
#if (IS_CHROMEOS_LACROS)
  DCHECK_EQ(GetProfile()->IsMainProfile(), use_main_profile_);
#endif

  identity_test_env_profile_adaptor_ =
      std::make_unique<IdentityTestEnvironmentProfileAdaptor>(GetProfile());
  identity_test_env()->SetTestURLLoaderFactory(&test_url_loader_factory_);
}

void SigninBrowserTestBase::TearDownOnMainThread() {
  // Must be destroyed before the Profile.
  identity_test_env_profile_adaptor_.reset();
}

void SigninBrowserTestBase::SetUpInProcessBrowserTestFixture() {
  InProcessBrowserTest::SetUpInProcessBrowserTestFixture();
  create_services_subscription_ =
      BrowserContextDependencyManager::GetInstance()
          ->RegisterCreateServicesCallbackForTesting(base::BindRepeating(
              &SigninBrowserTestBase::OnWillCreateBrowserContextServices,
              base::Unretained(this)));
}

void SigninBrowserTestBase::OnWillCreateBrowserContextServices(
    content::BrowserContext* context) {
  signin::AccountConsistencyMethod account_consistency_method =
#if BUILDFLAG(ENABLE_MIRROR)
      signin::AccountConsistencyMethod::kMirror;
#elif BUILDFLAG(ENABLE_DICE_SUPPORT)
      signin::AccountConsistencyMethod::kDice;
#else
      signin::AccountConsistencyMethod::kDisabled;
#endif

  IdentityTestEnvironmentProfileAdaptor::
      SetIdentityTestEnvironmentFactoriesOnBrowserContext(
          context, account_consistency_method);
  ChromeSigninClientFactory::GetInstance()->SetTestingFactory(
      context, base::BindRepeating(&BuildChromeSigninClientWithURLLoader,
                                   &test_url_loader_factory_));
}
