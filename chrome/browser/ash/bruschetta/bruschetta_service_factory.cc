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
          // Takes care of not creating the service for OTR and non user
          // profiles.
          ProfileSelections::Builder()
              .WithGuest(ProfileSelections::kRegularProfileDefault)
              .WithAshInternals(ProfileSelection::kNone)
              .Build()) {}

BruschettaServiceFactory::~BruschettaServiceFactory() = default;

KeyedService* BruschettaServiceFactory::BuildServiceInstanceFor(
    content::BrowserContext* context) const {
  auto* profile = Profile::FromBrowserContext(context);

  // Don't create a BruschettaService for anything except the primary user
  // profile for the session.
  if (!ash::ProfileHelper::Get()->IsPrimaryProfile(profile) ||
      ash::ProfileHelper::Get()->IsEphemeralUserProfile(profile))
    return nullptr;

  return new BruschettaService(profile);
}

// Force BruschettaService to be set up when a BrowserContext is
// initialized. This lets us do set up that should happen at the start of the
// session (e.g. registering existing Bruschetta VMs with other services).
bool BruschettaServiceFactory::ServiceIsCreatedWithBrowserContext() const {
  return true;
}

// Most tests don't set up the dependencies (e.g. user_manager::UserManager) for
// the ash::ProfileHelper calls in ::BuildServiceInstanceFor we use to check if
// we should create a BruschettaService for a given BrowserContext. Since they
// probably don't actually want a BruschettaService anyway, we use this to
// default to not creating one for test BrowserContexts.
bool BruschettaServiceFactory::ServiceIsNULLWhileTesting() const {
  return true;
}

// Static helper function for tests that do use BruschettaService. This bypasses
// the checks we normally use, so it doesn't introduce a dependency on
// user_manager::UserManager etc.
void BruschettaServiceFactory::EnableForTesting(Profile* profile) {
  GetInstance()->SetTestingFactory(
      profile, base::BindRepeating([](content::BrowserContext* context) {
        return base::WrapUnique<KeyedService>(
            new BruschettaService(Profile::FromBrowserContext(context)));
      }));
}

}  // namespace bruschetta
