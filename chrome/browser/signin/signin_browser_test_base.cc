// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/signin/signin_browser_test_base.h"

#include "chrome/browser/signin/chrome_signin_client_factory.h"
#include "chrome/browser/signin/chrome_signin_client_test_util.h"
#include "chrome/browser/ui/browser.h"
#include "components/signin/public/base/account_consistency_method.h"
#include "components/signin/public/base/signin_buildflags.h"
#include "testing/gtest/include/gtest/gtest.h"

SigninBrowserTestBase::SigninBrowserTestBase() = default;

SigninBrowserTestBase::~SigninBrowserTestBase() = default;

std::vector<AccountInfo> SigninBrowserTestBase::SetAccounts(
    const std::vector<std::string>& emails) {
  return identity_test_env()->MakeAccountsAvailableWithCookies(emails);
}

void SigninBrowserTestBase::SetUpOnMainThread() {
  identity_test_env_profile_adaptor_ =
      std::make_unique<IdentityTestEnvironmentProfileAdaptor>(
          browser()->profile());
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
