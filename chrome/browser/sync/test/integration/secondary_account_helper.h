// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SYNC_TEST_INTEGRATION_SECONDARY_ACCOUNT_HELPER_H_
#define CHROME_BROWSER_SYNC_TEST_INTEGRATION_SECONDARY_ACCOUNT_HELPER_H_

#include <memory>
#include <string>

#include "build/build_config.h"
#include "components/keyed_service/content/browser_context_dependency_manager.h"
#include "components/signin/public/identity_manager/account_info.h"

class Profile;

namespace network {
class TestURLLoaderFactory;
}

namespace secondary_account_helper {

// Sets up a factory to create a SigninClient which uses the
// provided |test_url_loader_factory| for cookie-related requests. Meant to be
// called from SetUpInProcessBrowserTestFixture. The caller should hold on to
// the returned object for the duration of the test, e.g. store it in a member
// of the test fixture class.
base::CallbackListSubscription SetUpSigninClient(
    network::TestURLLoaderFactory* test_url_loader_factory);

// Sets an account as primary with `signin::ConsentLevel::kSignin`. There is no
// consent for Sync. The account is available with both a refresh token and
// cookie.
AccountInfo SignInUnconsentedAccount(
    Profile* profile,
    network::TestURLLoaderFactory* test_url_loader_factory,
    const std::string& email);

// Sets an account as primary with `signin::ConsentLevel::kSignin`. There is no
// consent for Sync. The account is available with both a refresh token and
// cookie. The signin is not considered explicit (it happened through Dice
// automatic signin), and account storage for passwords and addresses is not
// opted-in.
AccountInfo ImplicitSignInUnconsentedAccount(
    Profile* profile,
    network::TestURLLoaderFactory* test_url_loader_factory,
    const std::string& email);

// Clears signin cookies and signs out of the primary account.
void SignOut(Profile* profile,
             network::TestURLLoaderFactory* test_url_loader_factory);

#if !BUILDFLAG(IS_CHROMEOS)
// Grants sync consent to an account (`signin::ConsentLevel::kSync`). The
// account must already be signed in (per SignInUnconsentedAccount).
void GrantSyncConsent(Profile* profile, const std::string& email);
#endif  // !BUILDFLAG(IS_CHROMEOS)

}  // namespace secondary_account_helper

#endif  // CHROME_BROWSER_SYNC_TEST_INTEGRATION_SECONDARY_ACCOUNT_HELPER_H_
