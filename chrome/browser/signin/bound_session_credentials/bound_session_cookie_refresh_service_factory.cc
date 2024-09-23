// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/signin/bound_session_credentials/bound_session_cookie_refresh_service_factory.h"

#include <memory>

#include "base/feature_list.h"
#include "base/no_destructor.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_keyed_service_factory.h"
#include "chrome/browser/signin/account_consistency_mode_manager.h"
#include "chrome/browser/signin/account_consistency_mode_manager_factory.h"
#include "chrome/browser/signin/bound_session_credentials/bound_session_cookie_refresh_service.h"
#include "chrome/browser/signin/bound_session_credentials/bound_session_cookie_refresh_service_impl.h"
#include "chrome/browser/signin/bound_session_credentials/bound_session_params_storage.h"
#include "chrome/browser/signin/bound_session_credentials/unexportable_key_service_factory.h"
#include "components/pref_registry/pref_registry_syncable.h"
#include "components/signin/public/base/account_consistency_method.h"
#include "components/signin/public/base/signin_pref_names.h"
#include "components/signin/public/base/signin_switches.h"
#include "content/public/browser/network_service_instance.h"

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
              .WithRegular(ProfileSelection::kOwnInstance)
              // Only an OTR profile is used for browsing in the Guest Session.
              .WithGuest(ProfileSelection::kOffTheRecordOnly)
              .Build()) {
  DependsOn(UnexportableKeyServiceFactory::GetInstance());
  DependsOn(AccountConsistencyModeManagerFactory::GetInstance());
}

BoundSessionCookieRefreshServiceFactory::
    ~BoundSessionCookieRefreshServiceFactory() = default;

std::unique_ptr<KeyedService>
BoundSessionCookieRefreshServiceFactory::BuildServiceInstanceForBrowserContext(
    content::BrowserContext* context) const {
  Profile* profile = Profile::FromBrowserContext(context);
  if (!switches::IsBoundSessionCredentialsEnabled(profile->GetPrefs())) {
    return nullptr;
  }

  unexportable_keys::UnexportableKeyService* key_service =
      UnexportableKeyServiceFactory::GetForProfile(profile);

  if (!key_service) {
    // A bound session requires a crypto provider.
    return nullptr;
  }

  signin::AccountConsistencyMethod account_consistency_method =
      AccountConsistencyModeManager::GetMethodForProfile(profile);
  bool should_create_service =
      account_consistency_method ==
          signin::AccountConsistencyMethod::kDisabled ||
      (account_consistency_method == signin::AccountConsistencyMethod::kDice &&
       switches::kEnableBoundSessionCredentialsDiceSupport.Get() ==
           switches::EnableBoundSessionCredentialsDiceSupport::kEnabled);

  if (!should_create_service) {
    return nullptr;
  }

  std::unique_ptr<BoundSessionCookieRefreshService>
      bound_session_cookie_refresh_service =
          std::make_unique<BoundSessionCookieRefreshServiceImpl>(
              *key_service,
              BoundSessionParamsStorage::CreateForProfile(*profile),
              profile->GetDefaultStoragePartition(),
              content::GetNetworkConnectionTracker(),
              profile->IsOffTheRecord());
  bound_session_cookie_refresh_service->Initialize();
  return bound_session_cookie_refresh_service;
}

void BoundSessionCookieRefreshServiceFactory::RegisterProfilePrefs(
    user_prefs::PrefRegistrySyncable* registry) {
  BoundSessionParamsStorage::RegisterProfilePrefs(registry);
  // Default value for this pref doesn't matter since it is only used when
  // explicitly set.
  registry->RegisterBooleanPref(prefs::kBoundSessionCredentialsEnabled, false);
}
