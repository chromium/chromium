// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/sync/test/integration/secondary_account_helper.h"

#include <string>
#include <vector>

#include "base/containers/flat_map.h"
#include "base/functional/bind.h"
#include "build/chromeos_buildflags.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/signin/chrome_signin_client_factory.h"
#include "chrome/browser/signin/chrome_signin_client_test_util.h"
#include "chrome/browser/signin/identity_manager_factory.h"
#include "components/signin/public/identity_manager/accounts_in_cookie_jar_info.h"
#include "components/signin/public/identity_manager/identity_manager.h"
#include "components/signin/public/identity_manager/identity_test_utils.h"
#include "components/signin/public/identity_manager/primary_account_mutator.h"
#include "google_apis/gaia/gaia_auth_util.h"

#if BUILDFLAG(IS_CHROMEOS_ASH)
#include "chrome/browser/ash/net/network_portal_detector_test_impl.h"
#include "chromeos/ash/components/network/network_handler.h"
#include "chromeos/ash/components/network/network_state.h"
#include "chromeos/ash/components/network/network_state_handler.h"
#include "chromeos/ash/components/network/portal_detector/network_portal_detector.h"
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)

namespace secondary_account_helper {

namespace {

void OnWillCreateBrowserContextServices(
    network::TestURLLoaderFactory* test_url_loader_factory,
    content::BrowserContext* context) {
  ChromeSigninClientFactory::GetInstance()->SetTestingFactory(
      context, base::BindRepeating(&BuildChromeSigninClientWithURLLoader,
                                   test_url_loader_factory));
}

void SetCookieForGaiaId(
    const std::string& gaia_id,
    const std::string& email,
    bool signed_out,
    signin::IdentityManager* identity_manager,
    network::TestURLLoaderFactory* test_url_loader_factory) {
  base::flat_map<std::string, signin::CookieParamsForTest> cookies_by_gaia_id;
  signin::AccountsInCookieJarInfo cookies =
      identity_manager->GetAccountsInCookieJar();
  for (const gaia::ListedAccount& account :
       cookies.GetPotentiallyInvalidSignedInAccounts()) {
    cookies_by_gaia_id[account.gaia_id] = {.email = account.email,
                                           .gaia_id = account.gaia_id,
                                           .signed_out = false};
  }
  for (const gaia::ListedAccount& account : cookies.GetSignedOutAccounts()) {
    cookies_by_gaia_id[account.gaia_id] = {
        .email = account.email, .gaia_id = account.gaia_id, .signed_out = true};
  }

  cookies_by_gaia_id[gaia_id] = {
      .email = email, .gaia_id = gaia_id, .signed_out = signed_out};

  std::vector<signin::CookieParamsForTest> new_cookie_list;
  for (const auto& [k, v] : cookies_by_gaia_id) {
    new_cookie_list.push_back(v);
  }
  signin::SetCookieAccounts(identity_manager, test_url_loader_factory,
                            new_cookie_list);
}

}  // namespace

base::CallbackListSubscription SetUpSigninClient(
    network::TestURLLoaderFactory* test_url_loader_factory) {
  return BrowserContextDependencyManager::GetInstance()
      ->RegisterCreateServicesCallbackForTesting(base::BindRepeating(
          &OnWillCreateBrowserContextServices, test_url_loader_factory));
}

#if BUILDFLAG(IS_CHROMEOS_ASH)
void InitNetwork() {
  auto* portal_detector = new ash::NetworkPortalDetectorTestImpl();

  const ash::NetworkState* default_network =
      ash::NetworkHandler::Get()->network_state_handler()->DefaultNetwork();

  portal_detector->SetDefaultNetworkForTesting(default_network->guid());

  // Takes ownership.
  ash::network_portal_detector::InitializeForTesting(portal_detector);
}
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)

AccountInfo SignInUnconsentedAccount(
    Profile* profile,
    network::TestURLLoaderFactory* test_url_loader_factory,
    const std::string& email) {
  signin::IdentityManager* identity_manager =
      IdentityManagerFactory::GetForProfile(profile);
#if !BUILDFLAG(IS_CHROMEOS_ASH)
  AccountInfo account_info = signin::MakePrimaryAccountAvailable(
      identity_manager, email, signin::ConsentLevel::kSignin);
#else
  AccountInfo account_info =
      signin::MakeAccountAvailable(identity_manager, email);
#endif  // !BUILDFLAG(IS_CHROMEOS_ASH)
  SetCookieForGaiaId(account_info.gaia, account_info.email,
                     /*signed_out=*/false, identity_manager,
                     test_url_loader_factory);
  return account_info;
}

AccountInfo ImplicitSignInUnconsentedAccount(
    Profile* profile,
    network::TestURLLoaderFactory* test_url_loader_factory,
    const std::string& email) {
  signin::IdentityManager* identity_manager =
      IdentityManagerFactory::GetForProfile(profile);
  signin::AccountAvailabilityOptionsBuilder builder;
  AccountInfo account_info = signin::MakeAccountAvailable(
      identity_manager,
      builder
#if !BUILDFLAG(IS_CHROMEOS_ASH)
          .AsPrimary(signin::ConsentLevel::kSignin)
#endif  // !BUILDFLAG(IS_CHROMEOS_ASH)
        // `ACCESS_POINT_WEB_SIGNIN` is not explicit signin.
          .WithAccessPoint(signin_metrics::AccessPoint::ACCESS_POINT_WEB_SIGNIN)
          .Build(email));
  SetCookieForGaiaId(account_info.gaia, account_info.email,
                     /*signed_out=*/false, identity_manager,
                     test_url_loader_factory);
  return account_info;
}

void SignOut(Profile* profile,
             network::TestURLLoaderFactory* test_url_loader_factory) {
  signin::IdentityManager* identity_manager =
      IdentityManagerFactory::GetForProfile(profile);
  CoreAccountInfo account =
      identity_manager->GetPrimaryAccountInfo(signin::ConsentLevel::kSignin);
  CHECK(!account.IsEmpty());
  SetCookieForGaiaId(account.gaia, account.email,
                     /*signed_out=*/true, identity_manager,
                     test_url_loader_factory);
  signin::ClearPrimaryAccount(identity_manager);
  signin::RemoveRefreshTokenForPrimaryAccount(identity_manager);
}

#if !BUILDFLAG(IS_CHROMEOS_ASH)
void GrantSyncConsent(Profile* profile, const std::string& email) {
  signin::IdentityManager* identity_manager =
      IdentityManagerFactory::GetForProfile(profile);
  AccountInfo account =
      identity_manager->FindExtendedAccountInfoByEmailAddress(email);
  DCHECK(!account.IsEmpty());
  signin::PrimaryAccountMutator* primary_account_mutator =
      identity_manager->GetPrimaryAccountMutator();
  primary_account_mutator->SetPrimaryAccount(account.account_id,
                                             signin::ConsentLevel::kSync);
}
#endif  // !BUILDFLAG(IS_CHROMEOS_ASH)

}  // namespace secondary_account_helper
