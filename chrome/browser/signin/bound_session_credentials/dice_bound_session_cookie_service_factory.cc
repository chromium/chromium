// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/signin/bound_session_credentials/dice_bound_session_cookie_service_factory.h"

#include <memory>

#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/signin/account_consistency_mode_manager_factory.h"
#include "chrome/browser/signin/bound_session_credentials/bound_session_cookie_refresh_service.h"
#include "chrome/browser/signin/bound_session_credentials/bound_session_cookie_refresh_service_factory.h"
#include "chrome/browser/signin/bound_session_credentials/dice_bound_session_cookie_service.h"
#include "chrome/browser/signin/identity_manager_factory.h"
#include "components/signin/public/base/account_consistency_method.h"

// static
DiceBoundSessionCookieServiceFactory*
DiceBoundSessionCookieServiceFactory::GetInstance() {
  static base::NoDestructor<DiceBoundSessionCookieServiceFactory> factory;
  return factory.get();
}

// static
DiceBoundSessionCookieService*
DiceBoundSessionCookieServiceFactory::GetForProfile(Profile* profile) {
  return static_cast<DiceBoundSessionCookieService*>(
      GetInstance()->GetServiceForBrowserContext(profile, true));
}

DiceBoundSessionCookieServiceFactory::DiceBoundSessionCookieServiceFactory()
    : ProfileKeyedServiceFactory("DiceBoundSessionCookieService",
                                 ProfileSelections::BuildForRegularProfile()) {
  DependsOn(IdentityManagerFactory::GetInstance());
  DependsOn(BoundSessionCookieRefreshServiceFactory::GetInstance());
  DependsOn(AccountConsistencyModeManagerFactory::GetInstance());
}

DiceBoundSessionCookieServiceFactory::~DiceBoundSessionCookieServiceFactory() =
    default;

std::unique_ptr<KeyedService>
DiceBoundSessionCookieServiceFactory::BuildServiceInstanceForBrowserContext(
    content::BrowserContext* context) const {
  Profile* profile = Profile::FromBrowserContext(context);
  if (AccountConsistencyModeManager::GetMethodForProfile(profile) !=
      signin::AccountConsistencyMethod::kDice) {
    return nullptr;
  }

  BoundSessionCookieRefreshService* bound_session_cookie_refresh_service =
      BoundSessionCookieRefreshServiceFactory::GetForProfile(profile);
  if (!bound_session_cookie_refresh_service) {
    return nullptr;
  }

  signin::IdentityManager* identity_manager =
      IdentityManagerFactory::GetForProfile(profile);
  if (!identity_manager) {
    return nullptr;
  }

  return std::make_unique<DiceBoundSessionCookieService>(
      *bound_session_cookie_refresh_service, *identity_manager,
      *profile->GetDefaultStoragePartition());
}

bool DiceBoundSessionCookieServiceFactory::ServiceIsCreatedWithBrowserContext()
    const {
  return true;
}
