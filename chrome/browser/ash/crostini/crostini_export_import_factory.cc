// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/crostini/crostini_export_import_factory.h"

#include "chrome/browser/ash/crostini/crostini_export_import.h"
#include "chrome/browser/ash/crostini/crostini_manager_factory.h"
#include "chrome/browser/ash/guest_os/guest_os_share_path_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_selections.h"

namespace crostini {

// static
CrostiniExportImport* CrostiniExportImportFactory::GetForProfile(
    Profile* profile) {
  return static_cast<CrostiniExportImport*>(
      GetInstance()->GetServiceForBrowserContext(profile, true));
}

// static
CrostiniExportImportFactory* CrostiniExportImportFactory::GetInstance() {
  static base::NoDestructor<CrostiniExportImportFactory> factory;
  return factory.get();
}

CrostiniExportImportFactory::CrostiniExportImportFactory()
    : ProfileKeyedServiceFactory(
          "CrostiniExportImportService",
          ProfileSelections::Builder()
              .WithRegular(ProfileSelection::kOriginalOnly)
              // TODO(crbug.com/40257657): Check if this service is needed in
              // Guest mode.
              .WithGuest(ProfileSelection::kOriginalOnly)
              // TODO(crbug.com/41488885): Check if this service is needed for
              // Ash Internals.
              .WithAshInternals(ProfileSelection::kOriginalOnly)
              .Build()) {
  DependsOn(guest_os::GuestOsSharePathFactory::GetInstance());
  DependsOn(CrostiniManagerFactory::GetInstance());
}

CrostiniExportImportFactory::~CrostiniExportImportFactory() = default;

KeyedService* CrostiniExportImportFactory::BuildServiceInstanceFor(
    content::BrowserContext* context) const {
  Profile* profile = Profile::FromBrowserContext(context);
  return new CrostiniExportImport(profile);
}

}  // namespace crostini
