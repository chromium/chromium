// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/net/nss_service_factory.h"

#include "base/no_destructor.h"
#include "build/chromeos_buildflags.h"
#include "chrome/browser/net/nss_service.h"

#if BUILDFLAG(IS_CHROMEOS_LACROS)
#include "chrome/browser/lacros/cert/cert_db_initializer_factory.h"
#endif  // BUILDFLAG(IS_CHROMEOS_LACROS)

NssService* NssServiceFactory::GetForContext(
    content::BrowserContext* browser_context) {
  return static_cast<NssService*>(
      GetInstance()->GetServiceForBrowserContext(browser_context, true));
}

NssServiceFactory::NssServiceFactory()
    : ProfileKeyedServiceFactory(
          "NssServiceFactory",
          // Create separate service for incognito profiles.
          ProfileSelections::Builder()
              .WithRegular(ProfileSelection::kOwnInstance)
              // TODO(crbug.com/40257657): Check if this service is needed in
              // Guest mode.
              .WithGuest(ProfileSelection::kOwnInstance)
              // TODO(crbug.com/41488885): Check if this service is needed for
              // Ash Internals.
              .WithAshInternals(ProfileSelection::kOwnInstance)
              .Build()) {
#if BUILDFLAG(IS_CHROMEOS_LACROS)
  DependsOn(CertDbInitializerFactory::GetInstance());
#endif  // BUILDFLAG(IS_CHROMEOS_LACROS)
}

NssServiceFactory::~NssServiceFactory() = default;

NssServiceFactory* NssServiceFactory::GetInstance() {
  static base::NoDestructor<NssServiceFactory> instance;
  return instance.get();
}

std::unique_ptr<KeyedService>
NssServiceFactory::BuildServiceInstanceForBrowserContext(
    content::BrowserContext* context) const {
  return std::make_unique<NssService>(context);
}
