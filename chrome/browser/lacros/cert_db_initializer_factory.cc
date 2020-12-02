// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/lacros/cert_db_initializer_factory.h"

#include "chrome/browser/lacros/cert_db_initializer.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/signin/identity_manager_factory.h"
#include "chromeos/lacros/lacros_chrome_service_impl.h"
#include "components/keyed_service/content/browser_context_dependency_manager.h"

class Profile;

// static
CertDbInitializerFactory* CertDbInitializerFactory::GetInstance() {
  static base::NoDestructor<CertDbInitializerFactory> factory;
  return factory.get();
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

  if (!chromeos::LacrosChromeServiceImpl::Get()->IsCertDbAvailable()) {
    return nullptr;
  }

  CertDbInitializer* result = new CertDbInitializer(
      chromeos::LacrosChromeServiceImpl::Get()->cert_database_remote(),
      profile);
  // TODO(crbug.com/1145946): Enable certificate database initialization when
  // the policy stack is ready (expected to happen before Feb 2021).
  if (/* DISABLES CODE */ (false)) {
    result->Start(IdentityManagerFactory::GetForProfile(profile));
  }
  return result;
}
