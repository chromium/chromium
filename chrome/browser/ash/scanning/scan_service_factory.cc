// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/scanning/scan_service_factory.h"

#include "base/no_destructor.h"
#include "chrome/browser/ash/drive/drive_integration_service.h"
#include "chrome/browser/ash/file_manager/path_util.h"
#include "chrome/browser/ash/scanning/lorgnette_scanner_manager_factory.h"
#include "chrome/browser/ash/scanning/scan_service.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/ash/holding_space/holding_space_keyed_service_factory.h"
#include "content/public/browser/browser_context.h"

namespace ash {

// static
ScanService* ScanServiceFactory::GetForBrowserContext(
    content::BrowserContext* context) {
  return static_cast<ScanService*>(
      ScanServiceFactory::GetInstance()->GetServiceForBrowserContext(
          context, /*create=*/true));
}

// static
ScanServiceFactory* ScanServiceFactory::GetInstance() {
  static base::NoDestructor<ScanServiceFactory> instance;
  return instance.get();
}

// static
KeyedService* ScanServiceFactory::BuildInstanceFor(
    content::BrowserContext* context) {
  Profile* profile = Profile::FromBrowserContext(context);
  auto* integration_service =
      drive::DriveIntegrationServiceFactory::FindForProfile(profile);
  bool drive_available = integration_service &&
                         integration_service->is_enabled() &&
                         integration_service->IsMounted();
  return new ScanService(
      LorgnetteScannerManagerFactory::GetForBrowserContext(context),
      file_manager::util::GetMyFilesFolderForProfile(profile),
      drive_available ? integration_service->GetMountPointPath()
                      : base::FilePath(),
      context);
}

ScanServiceFactory::ScanServiceFactory()
    : ProfileKeyedServiceFactory(
          "ScanService",
          ProfileSelections::Builder()
              .WithRegular(ProfileSelection::kRedirectedToOriginal)
              // Guest Profile follows Regular Profile selection mode.
              .WithGuest(ProfileSelection::kRedirectedToOriginal)
              // Prevent an instance of ScanService from being created on the
              // lock screen.
              .WithAshInternals(ProfileSelection::kNone)
              .Build()) {
  DependsOn(LorgnetteScannerManagerFactory::GetInstance());
  DependsOn(HoldingSpaceKeyedServiceFactory::GetInstance());
}

ScanServiceFactory::~ScanServiceFactory() = default;

KeyedService* ScanServiceFactory::BuildServiceInstanceFor(
    content::BrowserContext* context) const {
  return BuildInstanceFor(context);
}

bool ScanServiceFactory::ServiceIsNULLWhileTesting() const {
  return true;
}

}  // namespace ash
