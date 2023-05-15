// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/search/instant_service_factory.h"

#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/search/instant_service.h"
#include "chrome/browser/themes/theme_service_factory.h"
#include "components/search/search.h"

// static
InstantService* InstantServiceFactory::GetForProfile(Profile* profile) {
  DCHECK(search::IsInstantExtendedAPIEnabled());

  return static_cast<InstantService*>(
      GetInstance()->GetServiceForBrowserContext(profile, true));
}

// static
InstantServiceFactory* InstantServiceFactory::GetInstance() {
  return base::Singleton<InstantServiceFactory>::get();
}

InstantServiceFactory::InstantServiceFactory()
    : ProfileKeyedServiceFactory(
          "InstantService",
          ProfileSelections::Builder()
              .WithRegular(ProfileSelection::kOwnInstance)
              // TODO(crbug.com/1418376): Check if this service is needed in
              // Guest mode.
              .WithGuest(ProfileSelection::kOwnInstance)
              .Build()) {
  DependsOn(ThemeServiceFactory::GetInstance());
}

InstantServiceFactory::~InstantServiceFactory() = default;

KeyedService* InstantServiceFactory::BuildServiceInstanceFor(
    content::BrowserContext* context) const {
  DCHECK(search::IsInstantExtendedAPIEnabled());
  return new InstantService(Profile::FromBrowserContext(context));
}
