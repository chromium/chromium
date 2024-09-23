// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/lacros/cert/cert_db_initializer_factory.h"

#include "base/check_is_test.h"
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
      GetInstance()->GetServiceForBrowserContext(
          context, GetInstance()->should_create_on_demand_));
}

CertDbInitializerFactory::CertDbInitializerFactory()
    : ProfileKeyedServiceFactory(
          "CertDbInitializerFactory",
          ProfileSelections::Builder()
              .WithRegular(ProfileSelection::kRedirectedToOriginal)
              .WithGuest(ProfileSelection::kRedirectedToOriginal)
              .Build()) {}

bool CertDbInitializerFactory::ServiceIsCreatedWithBrowserContext() const {
  return should_create_with_browser_context_;
}

void CertDbInitializerFactory::SetCreateWithBrowserContextForTesting(
    bool should_create) {
  CHECK_IS_TEST();
  should_create_with_browser_context_ = should_create;
}

void CertDbInitializerFactory::SetCreateOnDemandForTesting(bool should_create) {
  CHECK_IS_TEST();
  should_create_on_demand_ = should_create;
}

std::unique_ptr<KeyedService>
CertDbInitializerFactory::BuildServiceInstanceForBrowserContext(
    content::BrowserContext* context) const {
  Profile* profile = Profile::FromBrowserContext(context);

  std::unique_ptr<CertDbInitializerImpl> result =
      std::make_unique<CertDbInitializerImpl>(profile);
  result->Start();
  return result;
}
