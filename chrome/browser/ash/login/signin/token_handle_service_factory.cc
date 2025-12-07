// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/login/signin/token_handle_service_factory.h"

#include <memory>

#include "base/no_destructor.h"
#include "chrome/browser/ash/login/signin/token_handle_service.h"
#include "chrome/browser/ash/login/signin/token_handle_store_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_selections.h"
#include "chrome/browser/signin/identity_manager_factory.h"

namespace ash {

TokenHandleServiceFactory::TokenHandleServiceFactory()
    : ProfileKeyedServiceFactory(
          "TokenHandleService",
          ProfileSelections::Builder()
              .WithRegular(ProfileSelection::kOriginalOnly)
              .WithGuest(ProfileSelection::kNone)
              .WithAshInternals(ProfileSelection::kNone)
              .Build()) {
  DependsOn(IdentityManagerFactory::GetInstance());
}

TokenHandleServiceFactory::~TokenHandleServiceFactory() = default;

// static
TokenHandleService* TokenHandleServiceFactory::GetForProfile(Profile* profile) {
  return static_cast<TokenHandleService*>(
      GetInstance()->GetServiceForBrowserContext(profile, true));
}

// static
TokenHandleServiceFactory* TokenHandleServiceFactory::GetInstance() {
  static base::NoDestructor<TokenHandleServiceFactory> instance;
  return instance.get();
}

std::unique_ptr<KeyedService>
TokenHandleServiceFactory::BuildServiceInstanceForBrowserContext(
    content::BrowserContext* context) const {
  Profile* profile = static_cast<Profile*>(context);
  return std::make_unique<TokenHandleService>(
      profile, TokenHandleStoreFactory::Get()->GetTokenHandleStore());
}

}  // namespace ash
