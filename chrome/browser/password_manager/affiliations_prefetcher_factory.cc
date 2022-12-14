// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/password_manager/affiliations_prefetcher_factory.h"

#include "base/no_destructor.h"
#include "chrome/browser/password_manager/affiliation_service_factory.h"
#include "chrome/browser/password_manager/password_store_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "components/password_manager/core/browser/affiliation/affiliations_prefetcher.h"
#include "content/public/browser/browser_context.h"

AffiliationsPrefetcherFactory::AffiliationsPrefetcherFactory()
    : ProfileKeyedServiceFactory("AffiliationsPrefetcher") {
  DependsOn(AffiliationServiceFactory::GetInstance());
}

AffiliationsPrefetcherFactory::~AffiliationsPrefetcherFactory() = default;

AffiliationsPrefetcherFactory* AffiliationsPrefetcherFactory::GetInstance() {
  static base::NoDestructor<AffiliationsPrefetcherFactory> instance;
  return instance.get();
}

password_manager::AffiliationsPrefetcher*
AffiliationsPrefetcherFactory::GetForProfile(Profile* profile) {
  return static_cast<password_manager::AffiliationsPrefetcher*>(
      GetInstance()->GetServiceForBrowserContext(
          profile, /*create=*/!profile->ShutdownStarted()));
}

KeyedService* AffiliationsPrefetcherFactory::BuildServiceInstanceFor(
    content::BrowserContext* context) const {
  Profile* profile = Profile::FromBrowserContext(context);
  // Since Password Manager doesn't work for non-standard profiles,
  // AffiliationsPrefetcher shouldn't be created for these profiles too.
  DCHECK(!profile->IsOffTheRecord());
  DCHECK(profile->IsRegularProfile());

  password_manager::AffiliationService* affiliation_service =
      AffiliationServiceFactory::GetForProfile(profile);

  return new password_manager::AffiliationsPrefetcher(affiliation_service);
}
