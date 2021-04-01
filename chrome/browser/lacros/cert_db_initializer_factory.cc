// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/lacros/cert_db_initializer_factory.h"

#include "chrome/browser/lacros/cert_db_initializer_impl.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/signin/identity_manager_factory.h"
#include "chromeos/lacros/lacros_chrome_service_impl.h"
#include "components/keyed_service/content/browser_context_dependency_manager.h"

class CertDbInitializer;
class Profile;

// static
CertDbInitializerFactory* CertDbInitializerFactory::GetInstance() {
  static base::NoDestructor<CertDbInitializerFactory> factory;
  return factory.get();
}

// static
CertDbInitializer* CertDbInitializerFactory::GetForProfileIfExists(
    Profile* profile) {
  return static_cast<CertDbInitializerImpl*>(
      GetInstance()->GetServiceForBrowserContext(profile, /*create=*/false));
}

CertDbInitializerFactory::CertDbInitializerFactory()
    : BrowserContextKeyedServiceFactory(
          "CertDbInitializerFactory",
          BrowserContextDependencyManager::GetInstance()) {
  DependsOn(IdentityManagerFactory::GetInstance());
}

bool CertDbInitializerFactory::ServiceIsCreatedWithBrowserContext() const {
  return true;
}

KeyedService* CertDbInitializerFactory::BuildServiceInstanceFor(
    content::BrowserContext* context) const {
  Profile* profile = Profile::FromBrowserContext(context);

  if (!chromeos::LacrosChromeServiceImpl::Get() ||
      !chromeos::LacrosChromeServiceImpl::Get()
           ->IsAvailable<crosapi::mojom::CertDatabase>()) {
    return nullptr;
  }

  CertDbInitializerImpl* result = new CertDbInitializerImpl(profile);
  result->Start(IdentityManagerFactory::GetForProfile(profile));
  return result;
}
