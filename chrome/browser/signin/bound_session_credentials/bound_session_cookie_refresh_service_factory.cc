// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/signin/bound_session_credentials/bound_session_cookie_refresh_service_factory.h"

#include "base/no_destructor.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_keyed_service_factory.h"
#include "chrome/browser/signin/account_consistency_mode_manager_factory.h"
#include "chrome/browser/signin/bound_session_credentials/bound_session_cookie_refresh_service.h"
#include "chrome/browser/signin/identity_manager_factory.h"

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
    : ProfileKeyedServiceFactory("BoundSessionCookieRefreshService") {
  DependsOn(IdentityManagerFactory::GetInstance());
  DependsOn(AccountConsistencyModeManagerFactory::GetInstance());
}

BoundSessionCookieRefreshServiceFactory::
    ~BoundSessionCookieRefreshServiceFactory() = default;

KeyedService* BoundSessionCookieRefreshServiceFactory::BuildServiceInstanceFor(
    content::BrowserContext* context) const {
  Profile* profile = Profile::FromBrowserContext(context);
  // The account consistency method should not change during the lifetime of a
  // profile. This service is needed when Dice is enabled.
  if (!AccountConsistencyModeManager::IsDiceEnabledForProfile(profile)) {
    return nullptr;
  }

  return new BoundSessionCookieRefreshService(
      IdentityManagerFactory::GetForProfile(profile));
}
