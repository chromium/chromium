// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/login/security_token_session_controller_factory.h"

#include "base/check_is_test.h"
#include "chrome/browser/ash/login/challenge_response_auth_keys_loader.h"
#include "chrome/browser/ash/login/security_token_session_controller.h"
#include "chrome/browser/ash/profiles/profile_helper.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/certificate_provider/certificate_provider_service_factory.h"
#include "chrome/browser/profiles/incognito_helpers.h"
#include "chrome/browser/profiles/profile.h"
#include "components/user_manager/user.h"

namespace ash {
namespace login {

SecurityTokenSessionControllerFactory::SecurityTokenSessionControllerFactory()
    : ProfileKeyedServiceFactory(
          "SecurityTokenSessionController",
          ProfileSelections::BuildRedirectedInIncognito()) {
  DependsOn(chromeos::CertificateProviderServiceFactory::GetInstance());
}

SecurityTokenSessionControllerFactory::
    ~SecurityTokenSessionControllerFactory() = default;

// static
SecurityTokenSessionController*
SecurityTokenSessionControllerFactory::GetForBrowserContext(
    content::BrowserContext* browser_context) {
  return static_cast<SecurityTokenSessionController*>(
      GetInstance()->GetServiceForBrowserContext(browser_context,
                                                 /*create=*/true));
}

// static
SecurityTokenSessionControllerFactory*
SecurityTokenSessionControllerFactory::GetInstance() {
  return base::Singleton<SecurityTokenSessionControllerFactory>::get();
}

KeyedService* SecurityTokenSessionControllerFactory::BuildServiceInstanceFor(
    content::BrowserContext* context) const {
  // The service should only exist for the primary and the sign-in profiles.
  Profile* profile = Profile::FromBrowserContext(context);
  if (!profile)
    return nullptr;
  const bool is_primary_profile = ProfileHelper::IsPrimaryProfile(profile);
  const bool is_signin_profile = ProfileHelper::IsSigninProfile(profile);
  if (!is_primary_profile && !is_signin_profile)
    return nullptr;

  PrefService* local_state = g_browser_process->local_state();
  if (!local_state) {
    // This can happen in tests that do not have local state.
    CHECK_IS_TEST();
    return nullptr;
  }

  auto* const user_manager = user_manager::UserManager::Get();
  DCHECK(user_manager);
  const user_manager::User* primary_user = user_manager->GetPrimaryUser();
  DCHECK(primary_user);

  chromeos::CertificateProviderService* certificate_provider_service =
      chromeos::CertificateProviderServiceFactory::GetForBrowserContext(
          context);
  return new SecurityTokenSessionController(is_primary_profile, local_state,
                                            primary_user,
                                            certificate_provider_service);
}

bool SecurityTokenSessionControllerFactory::ServiceIsCreatedWithBrowserContext()
    const {
  // The controller is only useful for users using authentication via security
  // tokens. However, we can not reliably check if the user uses a security
  // token when the browser context is created.
  // Instead, we instantiate the controller only once we know that a user did
  // use a security token.
  return false;
}

}  // namespace login
}  // namespace ash
