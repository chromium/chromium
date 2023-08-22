// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/bruschetta/bruschetta_service_factory.h"

#include "base/functional/callback.h"
#include "base/memory/ptr_util.h"
#include "chrome/browser/ash/bruschetta/bruschetta_service.h"
#include "chrome/browser/ash/profiles/profile_helper.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_selections.h"

namespace bruschetta {

// static
BruschettaService* bruschetta::BruschettaServiceFactory::GetForProfile(
    Profile* profile) {
  return static_cast<BruschettaService*>(
      GetInstance()->GetServiceForBrowserContext(profile, true));
}

// static
BruschettaServiceFactory* BruschettaServiceFactory::GetInstance() {
  static base::NoDestructor<BruschettaServiceFactory> factory;
  return factory.get();
}

BruschettaServiceFactory::BruschettaServiceFactory()
    : ProfileKeyedServiceFactory(
          "BruschettaService",
          // Only create one instance per login session. For OTR profiles,
          // we use the same instance as their parent profile.
          ProfileSelections::Builder()
              .WithRegular(ProfileSelection::kRedirectedToOriginal)
              .WithGuest(ProfileSelection::kNone)
              .WithAshInternals(ProfileSelection::kNone)
              .WithSystem(ProfileSelection::kNone)
              .Build()) {}

BruschettaServiceFactory::~BruschettaServiceFactory() = default;

std::unique_ptr<KeyedService>
BruschettaServiceFactory::BuildServiceInstanceForBrowserContext(
    content::BrowserContext* context) const {
  auto* profile = Profile::FromBrowserContext(context);
  return std::make_unique<BruschettaService>(profile);
}

// Force BruschettaService to be set up when a BrowserContext is
// initialized. This lets us do set up that should happen at the start of the
// session (e.g. registering existing Bruschetta VMs with other services).
bool BruschettaServiceFactory::ServiceIsCreatedWithBrowserContext() const {
  return true;
}

}  // namespace bruschetta
