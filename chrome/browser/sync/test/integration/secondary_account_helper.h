// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SYNC_TEST_INTEGRATION_SECONDARY_ACCOUNT_HELPER_H_
#define CHROME_BROWSER_SYNC_TEST_INTEGRATION_SECONDARY_ACCOUNT_HELPER_H_

#include <memory>
#include <string>

#include "build/build_config.h"
#include "build/chromeos_buildflags.h"
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

#if BUILDFLAG(IS_CHROMEOS_ASH)
// Sets up necessary fakes for fake network responses to work. Meant to be
// called from SetUpOnMainThread.
// TODO(crbug.com/882770): On ChromeOS, we need to set up a fake
// NetworkPortalDetector, otherwise chromeos::DelayNetworkCall will think it's
// behind a captive portal and delay all network requests forever, which means
// the ListAccounts requests (i.e. getting cookie accounts) will never make it
// far enough to even request our fake response.
void InitNetwork();
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)

// Makes a non-primary account available with both a refresh token and cookie.
AccountInfo SignInSecondaryAccount(
    Profile* profile,
    network::TestURLLoaderFactory* test_url_loader_factory,
    const std::string& email);

// Clears signin cookies and removes the refresh token for the given account.
void SignOutSecondaryAccount(
    Profile* profile,
    network::TestURLLoaderFactory* test_url_loader_factory,
    const CoreAccountId& account_id);

#if !BUILDFLAG(IS_CHROMEOS_ASH)
// Makes the given account Chrome's primary one. The account must already be
// signed in (per SignInSecondaryAccount).
void MakeAccountPrimary(Profile* profile, const std::string& email);
#endif  // !BUILDFLAG(IS_CHROMEOS_ASH)

}  // namespace secondary_account_helper

#endif  // CHROME_BROWSER_SYNC_TEST_INTEGRATION_SECONDARY_ACCOUNT_HELPER_H_
