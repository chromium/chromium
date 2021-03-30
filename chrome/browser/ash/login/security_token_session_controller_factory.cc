// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/login/security_token_session_controller_factory.h"

#include "chrome/browser/ash/certificate_provider/certificate_provider_service_factory.h"
#include "chrome/browser/ash/login/challenge_response_auth_keys_loader.h"
#include "chrome/browser/ash/login/security_token_session_controller.h"
#include "chrome/browser/ash/profiles/profile_helper.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/profiles/incognito_helpers.h"
#include "chrome/browser/profiles/profile.h"
#include "components/keyed_service/content/browser_context_dependency_manager.h"
#include "components/user_manager/user.h"

namespace chromeos {
namespace login {

SecurityTokenSessionControllerFactory::SecurityTokenSessionControllerFactory()
    : BrowserContextKeyedServiceFactory(
          "SecurityTokenSessionController",
          BrowserContextDependencyManager::GetInstance()) {
  DependsOn(CertificateProviderServiceFactory::GetInstance());
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
  // The service should only exist for the primary profile.
  Profile* profile = Profile::FromBrowserContext(context);
  if (!chromeos::ProfileHelper::IsPrimaryProfile(profile))
    return nullptr;

  PrefService* local_state = g_browser_process->local_state();
  if (!local_state) {
    // This can happen in tests that do not have local state.
    return nullptr;
  }

  // The service is only relevant for users who authenticate with a security
  // token used for a challenge-response flow.
  // TODO(crbug.com/1164373): This check produces false negatives for ephemeral
  // users.
  user_manager::User* user = ProfileHelper::Get()->GetUserByProfile(
      Profile::FromBrowserContext(context));
  if (!ChallengeResponseAuthKeysLoader::CanAuthenticateUser(
          user->GetAccountId()))
    return nullptr;

  CertificateProviderService* certificate_provider_service =
      CertificateProviderServiceFactory::GetForBrowserContext(context);
  return new SecurityTokenSessionController(local_state, profile->GetPrefs(),
                                            user, certificate_provider_service);
}

content::BrowserContext*
SecurityTokenSessionControllerFactory::GetBrowserContextToUse(
    content::BrowserContext* context) const {
  return chrome::GetBrowserContextRedirectedInIncognito(context);
}

bool SecurityTokenSessionControllerFactory::ServiceIsCreatedWithBrowserContext()
    const {
  return true;
}

}  // namespace login
}  // namespace chromeos
