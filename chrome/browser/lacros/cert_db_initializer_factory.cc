// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/lacros/cert_db_initializer_factory.h"

#include "base/system/sys_info.h"
#include "chrome/browser/lacros/cert_db_initializer_impl.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/signin/identity_manager_factory.h"
#include "chromeos/lacros/lacros_service.h"
#include "components/keyed_service/content/browser_context_dependency_manager.h"

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
    : BrowserContextKeyedServiceFactory(
          "CertDbInitializerFactory",
          BrowserContextDependencyManager::GetInstance()) {
  DependsOn(IdentityManagerFactory::GetInstance());
}

bool CertDbInitializerFactory::ServiceIsCreatedWithBrowserContext() const {
  // Here `IsRunningOnChromeOS()` is equivalent to "is not running in a test".
  // In production the service must be created together with its profile. But
  // most tests don't need it. If they do, this still allows to create it
  // manually.
  // TODO(b/202098971): When certificate verification is blocked on the NSS
  // database being loaded in lacros, there will need to be a
  // FakeCertDbInitializer in lacros tests by default.
  return base::SysInfo::IsRunningOnChromeOS();
}

KeyedService* CertDbInitializerFactory::BuildServiceInstanceFor(
    content::BrowserContext* context) const {
  Profile* profile = Profile::FromBrowserContext(context);

  CertDbInitializerImpl* result = new CertDbInitializerImpl(profile);
  result->Start();
  return result;
}
