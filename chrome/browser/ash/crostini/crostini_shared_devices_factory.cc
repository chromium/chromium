// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/crostini/crostini_shared_devices_factory.h"

#include "chrome/browser/ash/crostini/crostini_shared_devices.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_selections.h"

namespace crostini {

// static
CrostiniSharedDevices* CrostiniSharedDevicesFactory::GetForProfile(
    Profile* profile) {
  return static_cast<CrostiniSharedDevices*>(
      GetInstance()->GetServiceForBrowserContext(profile, true));
}

// static
CrostiniSharedDevicesFactory* CrostiniSharedDevicesFactory::GetInstance() {
  static base::NoDestructor<CrostiniSharedDevicesFactory> factory;
  return factory.get();
}

CrostiniSharedDevicesFactory::CrostiniSharedDevicesFactory()
    : ProfileKeyedServiceFactory(
          "CrostiniSharedDevicesService",
          ProfileSelections::Builder()
              .WithRegular(ProfileSelection::kOriginalOnly)
              // TODO(crbug.com/40257657): Check if this service is needed in
              // Guest mode.
              .WithGuest(ProfileSelection::kOriginalOnly)
              // TODO(crbug.com/41488885): Check if this service is needed for
              // Ash Internals.
              .WithAshInternals(ProfileSelection::kOriginalOnly)
              .Build()) {}

CrostiniSharedDevicesFactory::~CrostiniSharedDevicesFactory() = default;

KeyedService* CrostiniSharedDevicesFactory::BuildServiceInstanceFor(
    content::BrowserContext* context) const {
  Profile* profile = Profile::FromBrowserContext(context);
  return new CrostiniSharedDevices(profile);
}

}  // namespace crostini
