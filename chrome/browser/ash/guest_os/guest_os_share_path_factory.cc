// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/guest_os/guest_os_share_path_factory.h"

#include "base/no_destructor.h"
#include "chrome/browser/ash/crostini/crostini_manager_factory.h"
#include "chrome/browser/ash/drive/drive_integration_service.h"
#include "chrome/browser/ash/file_manager/volume_manager_factory.h"
#include "chrome/browser/ash/guest_os/guest_os_share_path.h"
#include "chrome/browser/profiles/profile.h"

namespace guest_os {

// static
GuestOsSharePath* GuestOsSharePathFactory::GetForProfile(Profile* profile) {
  return static_cast<GuestOsSharePath*>(
      GetInstance()->GetServiceForBrowserContext(profile, true));
}

// static
GuestOsSharePathFactory* GuestOsSharePathFactory::GetInstance() {
  static base::NoDestructor<GuestOsSharePathFactory> factory;
  return factory.get();
}

GuestOsSharePathFactory::GuestOsSharePathFactory()
    : ProfileKeyedServiceFactory(
          "GuestOsSharePath",
          ProfileSelections::Builder()
              .WithRegular(ProfileSelection::kRedirectedToOriginal)
              .WithGuest(ProfileSelection::kNone)
              .WithAshInternals(ProfileSelection::kNone)
              .WithSystem(ProfileSelection::kNone)
              .Build()) {
  DependsOn(crostini::CrostiniManagerFactory::GetInstance());
  DependsOn(file_manager::VolumeManagerFactory::GetInstance());
  DependsOn(drive::DriveIntegrationServiceFactory::GetInstance());
}

GuestOsSharePathFactory::~GuestOsSharePathFactory() = default;

std::unique_ptr<KeyedService>
GuestOsSharePathFactory::BuildServiceInstanceForBrowserContext(
    content::BrowserContext* context) const {
  Profile* profile = Profile::FromBrowserContext(context);
  return std::make_unique<GuestOsSharePath>(profile);
}

}  // namespace guest_os
