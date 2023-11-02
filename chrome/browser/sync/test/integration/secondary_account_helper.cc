// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/sync/test/integration/secondary_account_helper.h"

#include "base/bind.h"
#include "build/chromeos_buildflags.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/signin/chrome_signin_client_factory.h"
#include "chrome/browser/signin/chrome_signin_client_test_util.h"
#include "chrome/browser/signin/identity_manager_factory.h"
#include "components/signin/public/identity_manager/identity_manager.h"
#include "components/signin/public/identity_manager/identity_test_utils.h"
#include "components/signin/public/identity_manager/primary_account_mutator.h"

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

  portal_detector->SetDetectionResultsForTesting(
      default_network->guid(),
      ash::NetworkPortalDetector::CAPTIVE_PORTAL_STATUS_ONLINE, 204);

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
  AccountInfo account_info =
      signin::MakeAccountAvailable(identity_manager, email);
#if BUILDFLAG(IS_CHROMEOS_LACROS)
  // Unlike other platforms, ChromeOS does not get the primary account from the
  // cookies, and it needs to be set explicitly.
  identity_manager->GetPrimaryAccountMutator()->SetPrimaryAccount(
      account_info.account_id, signin::ConsentLevel::kSignin);
#endif
  signin::SetCookieAccounts(identity_manager, test_url_loader_factory,
                            {{account_info.email, account_info.gaia}});
  return account_info;
}

void SignOutAccount(Profile* profile,
                    network::TestURLLoaderFactory* test_url_loader_factory,
                    const CoreAccountId& account_id) {
  signin::IdentityManager* identity_manager =
      IdentityManagerFactory::GetForProfile(profile);
  signin::SetCookieAccounts(identity_manager, test_url_loader_factory, {});
  signin::RemoveRefreshTokenForAccount(identity_manager, account_id);
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
