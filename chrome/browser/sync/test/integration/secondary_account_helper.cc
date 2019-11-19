// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/sync/test/integration/secondary_account_helper.h"

#include "base/bind.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/signin/chrome_signin_client_factory.h"
#include "chrome/browser/signin/chrome_signin_client_test_util.h"
#include "chrome/browser/signin/identity_manager_factory.h"
#include "components/keyed_service/content/browser_context_dependency_manager.h"
#include "components/signin/public/identity_manager/identity_manager.h"
#include "components/signin/public/identity_manager/identity_test_utils.h"
#include "components/signin/public/identity_manager/primary_account_mutator.h"

#if defined(OS_CHROMEOS)
#include "chrome/browser/chromeos/net/network_portal_detector_test_impl.h"
#include "chromeos/network/network_handler.h"
#include "chromeos/network/network_state.h"
#include "chromeos/network/network_state_handler.h"
#include "chromeos/network/portal_detector/network_portal_detector.h"
#endif  // defined(OS_CHROMEOS)

namespace secondary_account_helper {

namespace {

void OnWillCreateBrowserContextServices(
    network::TestURLLoaderFactory* test_url_loader_factory,
    content::BrowserContext* context) {
  ChromeSigninClientFactory::GetInstance()->SetTestingFactory(
      context, base::BindRepeating(&BuildChromeSigninClientWithURLLoader,
                                   test_url_loader_factory));
}

}  // namespace

ScopedSigninClientFactory SetUpSigninClient(
    network::TestURLLoaderFactory* test_url_loader_factory) {
  return BrowserContextDependencyManager::GetInstance()
      ->RegisterWillCreateBrowserContextServicesCallbackForTesting(
          base::BindRepeating(&OnWillCreateBrowserContextServices,
                              test_url_loader_factory));
}

#if defined(OS_CHROMEOS)
void InitNetwork() {
  auto* portal_detector = new chromeos::NetworkPortalDetectorTestImpl();

  const chromeos::NetworkState* default_network =
      chromeos::NetworkHandler::Get()
          ->network_state_handler()
          ->DefaultNetwork();

  portal_detector->SetDefaultNetworkForTesting(default_network->guid());

  chromeos::NetworkPortalDetector::CaptivePortalState online_state;
  online_state.status =
      chromeos::NetworkPortalDetector::CAPTIVE_PORTAL_STATUS_ONLINE;
  online_state.response_code = 204;
  portal_detector->SetDetectionResultsForTesting(default_network->guid(),
                                                 online_state);

  // Takes ownership.
  chromeos::network_portal_detector::InitializeForTesting(portal_detector);
}
#endif  // defined(OS_CHROMEOS)

AccountInfo SignInSecondaryAccount(
    Profile* profile,
    network::TestURLLoaderFactory* test_url_loader_factory,
    const std::string& email) {
  signin::IdentityManager* identity_manager =
      IdentityManagerFactory::GetForProfile(profile);
  AccountInfo account_info =
      signin::MakeAccountAvailable(identity_manager, email);
  signin::SetCookieAccounts(identity_manager, test_url_loader_factory,
                            {{account_info.email, account_info.gaia}});
  return account_info;
}

void SignOutSecondaryAccount(
    Profile* profile,
    network::TestURLLoaderFactory* test_url_loader_factory,
    const CoreAccountId& account_id) {
  signin::IdentityManager* identity_manager =
      IdentityManagerFactory::GetForProfile(profile);
  signin::SetCookieAccounts(identity_manager, test_url_loader_factory, {});
  signin::RemoveRefreshTokenForAccount(identity_manager, account_id);
}

#if !defined(OS_CHROMEOS)
void MakeAccountPrimary(Profile* profile, const std::string& email) {
  signin::IdentityManager* identity_manager =
      IdentityManagerFactory::GetForProfile(profile);
  base::Optional<AccountInfo> maybe_account =
      identity_manager
          ->FindExtendedAccountInfoForAccountWithRefreshTokenByEmailAddress(
              email);
  DCHECK(maybe_account.has_value());
  auto* primary_account_mutator = identity_manager->GetPrimaryAccountMutator();
  primary_account_mutator->SetPrimaryAccount(maybe_account->account_id);
}
#endif  // !defined(OS_CHROMEOS)

}  // namespace secondary_account_helper
