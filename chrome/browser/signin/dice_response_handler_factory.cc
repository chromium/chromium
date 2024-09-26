// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/signin/dice_response_handler_factory.h"

#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/signin/about_signin_internals_factory.h"
#include "chrome/browser/signin/account_reconcilor_factory.h"
#include "chrome/browser/signin/bound_session_credentials/registration_token_helper.h"
#include "chrome/browser/signin/chrome_signin_client_factory.h"
#include "chrome/browser/signin/dice_response_handler.h"
#include "chrome/browser/signin/identity_manager_factory.h"
#include "components/signin/public/base/signin_switches.h"

#if BUILDFLAG(ENABLE_BOUND_SESSION_CREDENTIALS)
#include "chrome/browser/signin/bound_session_credentials/unexportable_key_service_factory.h"  // nogncheck
#endif  // BUILDFLAG(ENABLE_BOUND_SESSION_CREDENTIALS)

namespace {

#if BUILDFLAG(ENABLE_BOUND_SESSION_CREDENTIALS)
std::unique_ptr<RegistrationTokenHelper> BuildRegistrationTokenHelper(
    unexportable_keys::UnexportableKeyService& unexportable_key_service,
    RegistrationTokenHelper::KeyInitParam key_init_param) {
  return std::make_unique<RegistrationTokenHelper>(unexportable_key_service,
                                                   std::move(key_init_param));
}

DiceResponseHandler::RegistrationTokenHelperFactory
CreateRegistrationTokenHelperFactory(
    const PrefService* profile_prefs,
    unexportable_keys::UnexportableKeyService* unexportable_key_service) {
  if (!unexportable_key_service) {
    return {};
  }

  if (!switches::IsChromeRefreshTokenBindingEnabled(profile_prefs)) {
    return {};
  }

  // The factory holds a non-owning reference to `unexportable_key_service`.
  return base::BindRepeating(&BuildRegistrationTokenHelper,
                             std::ref(*unexportable_key_service));
}
#endif  // BUILDFLAG(ENABLE_BOUND_SESSION_CREDENTIALS)

}  // namespace

// static
DiceResponseHandler* DiceResponseHandlerFactory::GetForProfile(
    Profile* profile) {
  return static_cast<DiceResponseHandler*>(
      GetInstance()->GetServiceForBrowserContext(profile, true));
}

// static
DiceResponseHandlerFactory* DiceResponseHandlerFactory::GetInstance() {
  return base::Singleton<DiceResponseHandlerFactory>::get();
}

DiceResponseHandlerFactory::DiceResponseHandlerFactory()
    : ProfileKeyedServiceFactory("DiceResponseHandler") {
  DependsOn(AboutSigninInternalsFactory::GetInstance());
  DependsOn(AccountReconcilorFactory::GetInstance());
  DependsOn(ChromeSigninClientFactory::GetInstance());
  DependsOn(IdentityManagerFactory::GetInstance());
#if BUILDFLAG(ENABLE_BOUND_SESSION_CREDENTIALS)
  DependsOn(UnexportableKeyServiceFactory::GetInstance());
#endif  // BUILDFLAG(ENABLE_BOUND_SESSION_CREDENTIALS)
}

DiceResponseHandlerFactory::~DiceResponseHandlerFactory() = default;

// BrowserContextKeyedServiceFactory:
KeyedService* DiceResponseHandlerFactory::BuildServiceInstanceFor(
    content::BrowserContext* context) const {
  Profile* profile = static_cast<Profile*>(context);
  DiceResponseHandler::RegistrationTokenHelperFactory
      registration_token_helper_factory;
#if BUILDFLAG(ENABLE_BOUND_SESSION_CREDENTIALS)
  registration_token_helper_factory = CreateRegistrationTokenHelperFactory(
      profile->GetPrefs(),
      UnexportableKeyServiceFactory::GetForProfile(profile));
#endif  // BUILDFLAG(ENABLE_BOUND_SESSION_CREDENTIALS)
  return new DiceResponseHandler(
      ChromeSigninClientFactory::GetForProfile(profile),
      IdentityManagerFactory::GetForProfile(profile),
      AccountReconcilorFactory::GetForProfile(profile),
      AboutSigninInternalsFactory::GetForProfile(profile),
      std::move(registration_token_helper_factory));
}
