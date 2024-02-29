// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/scanning/lorgnette_scanner_manager_factory.h"

#include "base/no_destructor.h"
#include "chrome/browser/ash/profiles/profile_helper.h"
#include "chrome/browser/ash/scanning/lorgnette_scanner_manager.h"
#include "chrome/browser/ash/scanning/zeroconf_scanner_detector.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_selections.h"
#include "content/public/browser/browser_context.h"

namespace ash {

// static
LorgnetteScannerManager* LorgnetteScannerManagerFactory::GetForBrowserContext(
    content::BrowserContext* context) {
  return static_cast<LorgnetteScannerManager*>(
      LorgnetteScannerManagerFactory::GetInstance()
          ->GetServiceForBrowserContext(context, /*create=*/true));
}

// static
LorgnetteScannerManagerFactory* LorgnetteScannerManagerFactory::GetInstance() {
  static base::NoDestructor<LorgnetteScannerManagerFactory> instance;
  return instance.get();
}

LorgnetteScannerManagerFactory::LorgnetteScannerManagerFactory()
    : ProfileKeyedServiceFactory(
          "LorgnetteScannerManager",
          ProfileSelections::Builder()
              .WithGuest(ProfileSelection::kOriginalOnly)
              .WithAshInternals(ProfileSelection::kNone)
              // Prevent an instance of LorgnetteScannerManager from being
              // created on the lock screen.
              .Build()) {}

LorgnetteScannerManagerFactory::~LorgnetteScannerManagerFactory() = default;

std::unique_ptr<KeyedService>
LorgnetteScannerManagerFactory::BuildServiceInstanceForBrowserContext(
    content::BrowserContext* context) const {
  auto* profile = Profile::FromBrowserContext(context);
  return LorgnetteScannerManager::Create(ZeroconfScannerDetector::Create(),
                                         profile);
}

bool LorgnetteScannerManagerFactory::ServiceIsCreatedWithBrowserContext()
    const {
  return true;
}

bool LorgnetteScannerManagerFactory::ServiceIsNULLWhileTesting() const {
  return true;
}

}  // namespace ash
