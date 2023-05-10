// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/lacros/cert/cert_db_initializer_factory.h"

#include "base/no_destructor.h"
#include "base/system/sys_info.h"
#include "chrome/browser/lacros/cert/cert_db_initializer_impl.h"
#include "chrome/browser/profiles/profile.h"
#include "chromeos/lacros/lacros_service.h"

class CertDbInitializer;

// static
CertDbInitializerFactory* CertDbInitializerFactory::GetInstance() {
  static base::NoDestructor<CertDbInitializerFactory> factory;
  return factory.get();
}

// static
CertDbInitializer* CertDbInitializerFactory::GetForBrowserContext(
    content::BrowserContext* context) {
  return static_cast<CertDbInitializerImpl*>(
      GetInstance()->GetServiceForBrowserContext(context, /*create=*/false));
}

CertDbInitializerFactory::CertDbInitializerFactory()
    : ProfileKeyedServiceFactory(
          "CertDbInitializerFactory",
          ProfileSelections::Builder()
              .WithRegular(ProfileSelection::kRedirectedToOriginal)
              // TODO(crbug.com/1418376): Check if this service is needed in
              // Guest mode.
              .WithGuest(ProfileSelection::kRedirectedToOriginal)
              .Build()) {}

bool CertDbInitializerFactory::ServiceIsCreatedWithBrowserContext() const {
  return should_create_with_browser_context_;
}

void CertDbInitializerFactory::SetCreateWithBrowserContextForTesting(
    bool should_create) {
  should_create_with_browser_context_ = should_create;
}

KeyedService* CertDbInitializerFactory::BuildServiceInstanceFor(
    content::BrowserContext* context) const {
  Profile* profile = Profile::FromBrowserContext(context);

  CertDbInitializerImpl* result = new CertDbInitializerImpl(profile);
  result->Start();
  return result;
}

bool CertDbInitializerFactory::ServiceIsNULLWhileTesting() const {
  return true;
}
