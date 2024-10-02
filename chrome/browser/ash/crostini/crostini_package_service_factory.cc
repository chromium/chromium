// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/crostini/crostini_package_service_factory.h"

#include "chrome/browser/ash/crostini/crostini_manager_factory.h"
#include "chrome/browser/ash/crostini/crostini_package_service.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_selections.h"

namespace crostini {

// static
CrostiniPackageService* CrostiniPackageServiceFactory::GetForProfile(
    Profile* profile) {
  return static_cast<CrostiniPackageService*>(
      GetInstance()->GetServiceForBrowserContext(profile, true));
}

// static
CrostiniPackageServiceFactory* CrostiniPackageServiceFactory::GetInstance() {
  static base::NoDestructor<CrostiniPackageServiceFactory> factory;
  return factory.get();
}

CrostiniPackageServiceFactory::CrostiniPackageServiceFactory()
    : ProfileKeyedServiceFactory(
          "CrostiniPackageService",
          ProfileSelections::Builder()
              .WithRegular(ProfileSelection::kOriginalOnly)
              // TODO(crbug.com/40257657): Check if this service is needed in
              // Guest mode.
              .WithGuest(ProfileSelection::kOriginalOnly)
              // TODO(crbug.com/41488885): Check if this service is needed for
              // Ash Internals.
              .WithAshInternals(ProfileSelection::kOriginalOnly)
              .Build()) {
  DependsOn(CrostiniManagerFactory::GetInstance());
}

CrostiniPackageServiceFactory::~CrostiniPackageServiceFactory() = default;

KeyedService* CrostiniPackageServiceFactory::BuildServiceInstanceFor(
    content::BrowserContext* context) const {
  Profile* profile = Profile::FromBrowserContext(context);
  return new CrostiniPackageService(profile);
}

}  // namespace crostini
