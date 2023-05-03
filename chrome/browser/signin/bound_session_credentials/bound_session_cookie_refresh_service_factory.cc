// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/signin/bound_session_credentials/bound_session_cookie_refresh_service_factory.h"
#include <memory>

#include "base/no_destructor.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_keyed_service_factory.h"
#include "chrome/browser/signin/account_consistency_mode_manager_factory.h"
#include "chrome/browser/signin/bound_session_credentials/bound_session_cookie_refresh_service.h"
#include "chrome/browser/signin/bound_session_credentials/bound_session_cookie_refresh_service_impl.h"
#include "chrome/browser/signin/chrome_signin_client_factory.h"
#include "chrome/browser/signin/identity_manager_factory.h"
#include "components/signin/public/base/signin_switches.h"

// static
BoundSessionCookieRefreshServiceFactory*
BoundSessionCookieRefreshServiceFactory::GetInstance() {
  static base::NoDestructor<BoundSessionCookieRefreshServiceFactory> factory;
  return factory.get();
}

// static
BoundSessionCookieRefreshService*
BoundSessionCookieRefreshServiceFactory::GetForProfile(Profile* profile) {
  return static_cast<BoundSessionCookieRefreshService*>(
      GetInstance()->GetServiceForBrowserContext(profile, true));
}

BoundSessionCookieRefreshServiceFactory::
    BoundSessionCookieRefreshServiceFactory()
    : ProfileKeyedServiceFactory(
          "BoundSessionCookieRefreshService",
          ProfileSelections::Builder()
              .WithRegular(ProfileSelection::kOriginalOnly)
              // TODO(crbug.com/1418376): Check if this service is needed in
              // Guest mode.
              .WithGuest(ProfileSelection::kOriginalOnly)
              .Build()) {
  DependsOn(IdentityManagerFactory::GetInstance());
  DependsOn(AccountConsistencyModeManagerFactory::GetInstance());
  DependsOn(ChromeSigninClientFactory::GetInstance());
}

BoundSessionCookieRefreshServiceFactory::
    ~BoundSessionCookieRefreshServiceFactory() = default;

std::unique_ptr<KeyedService>
BoundSessionCookieRefreshServiceFactory::BuildServiceInstanceForBrowserContext(
    content::BrowserContext* context) const {
  if (!switches::IsBoundSessionCredentialsEnabled()) {
    return nullptr;
  }

  Profile* profile = Profile::FromBrowserContext(context);
  // The account consistency method should not change during the lifetime of a
  // profile. This service is needed when Dice is enabled.
  if (!AccountConsistencyModeManager::IsDiceEnabledForProfile(profile)) {
    return nullptr;
  }

  std::unique_ptr<BoundSessionCookieRefreshService>
      bound_session_cookie_refresh_service =
          std::make_unique<BoundSessionCookieRefreshServiceImpl>(
              ChromeSigninClientFactory::GetForProfile(profile),
              IdentityManagerFactory::GetForProfile(profile));
  bound_session_cookie_refresh_service->Initialize();
  return bound_session_cookie_refresh_service;
}
