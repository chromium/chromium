// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/indigo/indigo_service_factory.h"

#include "chrome/browser/indigo/indigo_service.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/signin/identity_manager_factory.h"

namespace indigo {

// static
IndigoService* IndigoServiceFactory::GetForProfile(Profile* profile) {
  return static_cast<IndigoService*>(
      GetInstance()->GetServiceForBrowserContext(profile, true));
}

// static
IndigoServiceFactory* IndigoServiceFactory::GetInstance() {
  static base::NoDestructor<IndigoServiceFactory> instance;
  return instance.get();
}

IndigoServiceFactory::IndigoServiceFactory()
    : ProfileKeyedServiceFactory("IndigoService",
                                 ProfileSelections::BuildForRegularProfile()) {
  DependsOn(IdentityManagerFactory::GetInstance());
}

IndigoServiceFactory::~IndigoServiceFactory() = default;

std::unique_ptr<KeyedService>
IndigoServiceFactory::BuildServiceInstanceForBrowserContext(
    content::BrowserContext* context) const {
  Profile* profile = Profile::FromBrowserContext(context);
  return std::make_unique<IndigoService>(
      profile, IdentityManagerFactory::GetForProfile(profile));
}

}  // namespace indigo
