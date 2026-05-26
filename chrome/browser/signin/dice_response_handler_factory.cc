// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/signin/dice_response_handler_factory.h"

#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/signin/about_signin_internals_factory.h"
#include "chrome/browser/signin/account_reconcilor_factory.h"
#include "chrome/browser/signin/chrome_signin_client_factory.h"
#include "chrome/browser/signin/dice_response_handler.h"
#include "chrome/browser/signin/identity_manager_factory.h"

// static
DiceResponseHandler* DiceResponseHandlerFactory::GetForProfile(
    Profile* profile) {
  return static_cast<DiceResponseHandler*>(
      GetInstance()->GetServiceForBrowserContext(profile, true));
}

// static
DiceResponseHandlerFactory* DiceResponseHandlerFactory::GetInstance() {
  static base::NoDestructor<DiceResponseHandlerFactory> instance;
  return instance.get();
}

DiceResponseHandlerFactory::DiceResponseHandlerFactory()
    : ProfileKeyedServiceFactory("DiceResponseHandler") {
  DependsOn(AboutSigninInternalsFactory::GetInstance());
  DependsOn(AccountReconcilorFactory::GetInstance());
  DependsOn(ChromeSigninClientFactory::GetInstance());
  DependsOn(IdentityManagerFactory::GetInstance());
}

DiceResponseHandlerFactory::~DiceResponseHandlerFactory() = default;

// BrowserContextKeyedServiceFactory:
std::unique_ptr<KeyedService>
DiceResponseHandlerFactory::BuildServiceInstanceForBrowserContext(
    content::BrowserContext* context) const {
  Profile* profile = static_cast<Profile*>(context);
  return std::make_unique<DiceResponseHandler>(
      ChromeSigninClientFactory::GetForProfile(profile),
      IdentityManagerFactory::GetForProfile(profile),
      AccountReconcilorFactory::GetForProfile(profile),
      AboutSigninInternalsFactory::GetForProfile(profile));
}
