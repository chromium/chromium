// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/crostini/crostini_upgrader_factory.h"

#include "chrome/browser/ash/crostini/crostini_manager_factory.h"
#include "chrome/browser/ash/crostini/crostini_upgrader.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_selections.h"

namespace crostini {

// static
CrostiniUpgrader* CrostiniUpgraderFactory::GetForProfile(Profile* profile) {
  return static_cast<CrostiniUpgrader*>(
      GetInstance()->GetServiceForBrowserContext(profile, true));
}

// static
CrostiniUpgraderFactory* CrostiniUpgraderFactory::GetInstance() {
  static base::NoDestructor<CrostiniUpgraderFactory> factory;
  return factory.get();
}

CrostiniUpgraderFactory::CrostiniUpgraderFactory()
    : ProfileKeyedServiceFactory(
          "CrostiniUpgraderService",
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

CrostiniUpgraderFactory::~CrostiniUpgraderFactory() = default;

KeyedService* CrostiniUpgraderFactory::BuildServiceInstanceFor(
    content::BrowserContext* context) const {
  Profile* profile = Profile::FromBrowserContext(context);
  return new CrostiniUpgrader(profile);
}

}  // namespace crostini
