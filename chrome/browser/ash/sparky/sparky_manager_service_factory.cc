// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/sparky/sparky_manager_service_factory.h"

#include <memory>

#include "chrome/browser/ash/profiles/profile_helper.h"
#include "chrome/browser/manta/manta_service_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_selections.h"
#include "chrome/browser/signin/identity_manager_factory.h"
#include "components/manta/features.h"
#include "components/manta/manta_service.h"
#include "content/public/browser/browser_context.h"

namespace ash {

// static
SparkyManagerImpl* SparkyManagerServiceFactory::GetForProfile(
    Profile* profile) {
  return static_cast<SparkyManagerImpl*>(
      GetInstance()->GetServiceForBrowserContext(profile, true));
}

// static
SparkyManagerServiceFactory* SparkyManagerServiceFactory::GetInstance() {
  static base::NoDestructor<SparkyManagerServiceFactory> instance;
  return instance.get();
}

SparkyManagerServiceFactory::SparkyManagerServiceFactory()
    : ProfileKeyedServiceFactory(
          "SparkyManagerServiceFactory",
          ProfileSelections::Builder()
              .WithRegular(ProfileSelection::kOriginalOnly)
              // TODO(crbug.com/41488885): Check if this service is needed for
              // Ash Internals.
              .WithAshInternals(ProfileSelection::kOriginalOnly)
              .Build()) {
  DependsOn(IdentityManagerFactory::GetInstance());
  DependsOn(manta::MantaServiceFactory::GetInstance());
}

SparkyManagerServiceFactory::~SparkyManagerServiceFactory() = default;

std::unique_ptr<KeyedService>
SparkyManagerServiceFactory::BuildServiceInstanceForBrowserContext(
    content::BrowserContext* const context) const {
  if (!manta::features::IsMantaServiceEnabled()) {
    return nullptr;
  }

  Profile* profile = Profile::FromBrowserContext(context);
  if (!ProfileHelper::IsPrimaryProfile(profile)) {
    return nullptr;
  }

  manta::MantaService* manta_service =
      manta::MantaServiceFactory::GetForProfile(profile);
  if (!manta_service) {
    return nullptr;
  }

  return std::make_unique<SparkyManagerImpl>(profile, manta_service);
}

}  // namespace ash
