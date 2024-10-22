// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/scanner/scanner_keyed_service_factory.h"

#include <memory>
#include <utility>

#include "base/memory/scoped_refptr.h"
#include "base/no_destructor.h"
#include "chrome/browser/ash/scanner/scanner_keyed_service.h"
#include "chrome/browser/manta/manta_service_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_keyed_service_factory.h"
#include "chrome/browser/profiles/profile_selections.h"
#include "chrome/browser/signin/identity_manager_factory.h"
#include "components/keyed_service/core/keyed_service.h"
#include "components/manta/manta_service.h"
#include "components/signin/public/identity_manager/identity_manager.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/storage_partition.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"

ScannerKeyedService* ScannerKeyedServiceFactory::GetForProfile(
    Profile* profile) {
  // The create parameter indicates to the underlying KeyedServiceFactory that
  // it is allowed to create a new ScannerKeyedService for this context if it
  // cannot find one. It does not mean it should create a new instance on every
  // call to this method.
  return static_cast<ScannerKeyedService*>(
      GetInstance()->GetServiceForBrowserContext(/*context=*/profile,
                                                 /*create=*/true));
}

ScannerKeyedServiceFactory* ScannerKeyedServiceFactory::GetInstance() {
  static base::NoDestructor<ScannerKeyedServiceFactory> instance;
  return instance.get();
}

ScannerKeyedServiceFactory::ScannerKeyedServiceFactory()
    : ProfileKeyedServiceFactory(
          "ScannerKeyedServiceFactory",
          ProfileSelections::Builder()
              .WithRegular(ProfileSelection::kRedirectedToOriginal)
              .Build()) {
  DependsOn(IdentityManagerFactory::GetInstance());
  DependsOn(manta::MantaServiceFactory::GetInstance());
}

ScannerKeyedServiceFactory::~ScannerKeyedServiceFactory() = default;

std::unique_ptr<KeyedService> ScannerKeyedServiceFactory::BuildInstanceFor(
    content::BrowserContext* context) {
  Profile* profile = Profile::FromBrowserContext(context);

  signin::IdentityManager* identity_manager =
      IdentityManagerFactory::GetForProfile(profile);

  scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory =
      profile->GetDefaultStoragePartition()
          ->GetURLLoaderFactoryForBrowserProcess();

  std::unique_ptr<manta::ScannerProvider> scanner_provider;
  manta::MantaService* manta_service =
      manta::MantaServiceFactory::GetForProfile(profile);
  if (manta_service) {
    scanner_provider = manta_service->CreateScannerProvider();
  }
  return std::make_unique<ScannerKeyedService>(identity_manager,
                                               std::move(url_loader_factory),
                                               std::move(scanner_provider));
}

std::unique_ptr<KeyedService>
ScannerKeyedServiceFactory::BuildServiceInstanceForBrowserContext(
    content::BrowserContext* context) const {
  return BuildInstanceFor(context);
}
