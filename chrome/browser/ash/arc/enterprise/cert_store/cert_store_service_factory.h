// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_ARC_ENTERPRISE_CERT_STORE_CERT_STORE_SERVICE_FACTORY_H_
#define CHROME_BROWSER_ASH_ARC_ENTERPRISE_CERT_STORE_CERT_STORE_SERVICE_FACTORY_H_

#include "base/memory/singleton.h"
#include "chrome/browser/profiles/profile_keyed_service_factory.h"

namespace arc {

class CertStoreService;

// Singleton factory for CertStoreService.
class CertStoreServiceFactory : public ProfileKeyedServiceFactory {
 public:
  static CertStoreService* GetForBrowserContext(
      content::BrowserContext* context);

  static CertStoreServiceFactory* GetInstance();

  CertStoreServiceFactory(const CertStoreServiceFactory&) = delete;
  CertStoreServiceFactory& operator=(const CertStoreServiceFactory&) = delete;

 private:
  friend base::DefaultSingletonTraits<CertStoreServiceFactory>;

  CertStoreServiceFactory();
  ~CertStoreServiceFactory() override;

  // ProfileKeyedServiceFactory overrides.
  bool ServiceIsNULLWhileTesting() const override;
  KeyedService* BuildServiceInstanceFor(
      content::BrowserContext* context) const override;
};

}  // namespace arc

#endif  // CHROME_BROWSER_ASH_ARC_ENTERPRISE_CERT_STORE_CERT_STORE_SERVICE_FACTORY_H_
