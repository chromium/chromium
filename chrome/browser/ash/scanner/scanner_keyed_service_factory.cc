// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/scanner/scanner_keyed_service_factory.h"

#include <memory>

#include "base/no_destructor.h"
#include "chrome/browser/ash/scanner/scanner_keyed_service.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_keyed_service_factory.h"
#include "content/public/browser/browser_context.h"

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
              .WithGuest(ProfileSelection::kNone)
              .WithSystem(ProfileSelection::kNone)
              .WithAshInternals(ProfileSelection::kNone)
              .Build()) {}

ScannerKeyedServiceFactory::~ScannerKeyedServiceFactory() = default;

std::unique_ptr<KeyedService> ScannerKeyedServiceFactory::BuildInstanceFor(
    content::BrowserContext* context) {
  return std::make_unique<ScannerKeyedService>(
      Profile::FromBrowserContext(context));
}

std::unique_ptr<KeyedService>
ScannerKeyedServiceFactory::BuildServiceInstanceForBrowserContext(
    content::BrowserContext* context) const {
  return BuildInstanceFor(context);
}
