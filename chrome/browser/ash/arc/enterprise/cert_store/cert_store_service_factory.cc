// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/arc/enterprise/cert_store/cert_store_service_factory.h"

#include "chrome/browser/ash/arc/enterprise/cert_store/cert_store_service.h"
#include "chrome/browser/net/nss_service_factory.h"
#include "chrome/browser/profiles/profile_selections.h"

namespace arc {

// static
CertStoreService* CertStoreServiceFactory::GetForBrowserContext(
    content::BrowserContext* context) {
  return static_cast<CertStoreService*>(
      GetInstance()->GetServiceForBrowserContext(context, true));
}

// static
CertStoreServiceFactory* CertStoreServiceFactory::GetInstance() {
  return base::Singleton<CertStoreServiceFactory>::get();
}

CertStoreServiceFactory::CertStoreServiceFactory()
    : ProfileKeyedServiceFactory(
          "CertStoreService",
          ProfileSelections::Builder()
              .WithRegular(ProfileSelection::kOwnInstance)
              // TODO(crbug.com/40257657): Check if this service is needed in
              // Guest mode.
              .WithGuest(ProfileSelection::kOwnInstance)
              // TODO(crbug.com/41488885): Check if this service is needed for
              // Ash Internals.
              .WithAshInternals(ProfileSelection::kOwnInstance)
              .Build()) {
  DependsOn(NssServiceFactory::GetInstance());
}

CertStoreServiceFactory::~CertStoreServiceFactory() = default;

bool CertStoreServiceFactory::ServiceIsNULLWhileTesting() const {
  return true;
}

KeyedService* CertStoreServiceFactory::BuildServiceInstanceFor(
    content::BrowserContext* context) const {
  return new CertStoreService(context);
}

}  // namespace arc
