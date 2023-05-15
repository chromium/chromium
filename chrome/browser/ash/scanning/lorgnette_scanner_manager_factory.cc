// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/scanning/lorgnette_scanner_manager_factory.h"

#include "base/memory/singleton.h"
#include "chrome/browser/ash/scanning/lorgnette_scanner_manager.h"
#include "chrome/browser/ash/scanning/zeroconf_scanner_detector.h"
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
  return base::Singleton<LorgnetteScannerManagerFactory>::get();
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

KeyedService* LorgnetteScannerManagerFactory::BuildServiceInstanceFor(
    content::BrowserContext* context) const {
  return LorgnetteScannerManager::Create(ZeroconfScannerDetector::Create())
      .release();
}

bool LorgnetteScannerManagerFactory::ServiceIsCreatedWithBrowserContext()
    const {
  return true;
}

bool LorgnetteScannerManagerFactory::ServiceIsNULLWhileTesting() const {
  return true;
}

}  // namespace ash
