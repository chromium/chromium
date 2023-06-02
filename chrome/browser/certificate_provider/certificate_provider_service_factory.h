// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CERTIFICATE_PROVIDER_CERTIFICATE_PROVIDER_SERVICE_FACTORY_H_
#define CHROME_BROWSER_CERTIFICATE_PROVIDER_CERTIFICATE_PROVIDER_SERVICE_FACTORY_H_

#include "chrome/browser/profiles/profile_keyed_service_factory.h"

namespace base {
template <typename T>
class NoDestructor;
}

namespace content {
class BrowserContext;
}

namespace chromeos {

class CertificateProviderService;

// Factory to create CertificateProviderService.
class CertificateProviderServiceFactory : public ProfileKeyedServiceFactory {
 public:
  static CertificateProviderService* GetForBrowserContext(
      content::BrowserContext* context);

  static CertificateProviderServiceFactory* GetInstance();

  CertificateProviderServiceFactory(const CertificateProviderServiceFactory&) =
      delete;
  CertificateProviderServiceFactory& operator=(
      const CertificateProviderServiceFactory&) = delete;

 private:
  friend base::NoDestructor<CertificateProviderServiceFactory>;

  CertificateProviderServiceFactory();

  // BrowserContextKeyedServiceFactory:
  bool ServiceIsNULLWhileTesting() const override;
  KeyedService* BuildServiceInstanceFor(
      content::BrowserContext* context) const override;
};

}  // namespace chromeos

#endif  // CHROME_BROWSER_CERTIFICATE_PROVIDER_CERTIFICATE_PROVIDER_SERVICE_FACTORY_H_
