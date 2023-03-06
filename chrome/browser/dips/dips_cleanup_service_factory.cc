// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/dips/dips_cleanup_service_factory.h"

#include "base/memory/singleton.h"
#include "chrome/browser/dips/dips_cleanup_service.h"
#include "chrome/browser/dips/dips_features.h"

// static
DIPSCleanupService* DIPSCleanupServiceFactory::GetForBrowserContext(
    content::BrowserContext* context) {
  return static_cast<DIPSCleanupService*>(
      GetInstance()->GetServiceForBrowserContext(context, /*create=*/true));
}

DIPSCleanupServiceFactory* DIPSCleanupServiceFactory::GetInstance() {
  return base::Singleton<DIPSCleanupServiceFactory>::get();
}

/*static*/
ProfileSelections DIPSCleanupServiceFactory::CreateProfileSelections() {
  if (!base::FeatureList::IsEnabled(dips::kFeature)) {
    return GetHumanProfileSelections();
  }

  return ProfileSelections::BuildNoProfilesSelected();
}

DIPSCleanupServiceFactory::DIPSCleanupServiceFactory()
    : ProfileKeyedServiceFactory("DIPSCleanupService",
                                 CreateProfileSelections()) {}

DIPSCleanupServiceFactory::~DIPSCleanupServiceFactory() = default;

KeyedService* DIPSCleanupServiceFactory::BuildServiceInstanceFor(
    content::BrowserContext* context) const {
  return new DIPSCleanupService(context);
}

bool DIPSCleanupServiceFactory::ServiceIsCreatedWithBrowserContext() const {
  return true;
}
