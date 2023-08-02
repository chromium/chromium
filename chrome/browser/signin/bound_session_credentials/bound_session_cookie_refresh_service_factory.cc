// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/signin/bound_session_credentials/bound_session_cookie_refresh_service_factory.h"
#include <memory>

#include "base/no_destructor.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_keyed_service_factory.h"
#include "chrome/browser/signin/bound_session_credentials/bound_session_cookie_refresh_service.h"
#include "chrome/browser/signin/bound_session_credentials/bound_session_cookie_refresh_service_impl.h"
#include "chrome/browser/signin/bound_session_credentials/unexportable_key_service_factory.h"
#include "chrome/browser/signin/chrome_signin_client_factory.h"
#include "components/pref_registry/pref_registry_syncable.h"
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
              // TODO(b/279719658): Enable on OTR profiles after removing
              // dependency on `ChromeSigninClient`.
              .WithRegular(ProfileSelection::kOriginalOnly)
              .WithGuest(ProfileSelection::kOriginalOnly)
              .Build()) {
  DependsOn(ChromeSigninClientFactory::GetInstance());
  DependsOn(UnexportableKeyServiceFactory::GetInstance());
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
  unexportable_keys::UnexportableKeyService* key_service =
      UnexportableKeyServiceFactory::GetForProfile(profile);

  if (!key_service) {
    // A bound session requires a crypto provider.
    return nullptr;
  }
  std::unique_ptr<BoundSessionCookieRefreshService>
      bound_session_cookie_refresh_service =
          std::make_unique<BoundSessionCookieRefreshServiceImpl>(
              *key_service, profile->GetPrefs(),
              ChromeSigninClientFactory::GetForProfile(profile));
  bound_session_cookie_refresh_service->Initialize();
  return bound_session_cookie_refresh_service;
}

void BoundSessionCookieRefreshServiceFactory::RegisterProfilePrefs(
    user_prefs::PrefRegistrySyncable* registry) {
  BoundSessionCookieRefreshServiceImpl::RegisterProfilePrefs(registry);
}
