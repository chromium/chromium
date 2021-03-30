// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_CERTIFICATE_PROVIDER_CERTIFICATE_PROVIDER_SERVICE_FACTORY_H_
#define CHROME_BROWSER_ASH_CERTIFICATE_PROVIDER_CERTIFICATE_PROVIDER_SERVICE_FACTORY_H_

#include "base/macros.h"
#include "components/keyed_service/content/browser_context_keyed_service_factory.h"

namespace base {
template <typename T>
struct DefaultSingletonTraits;
}

namespace content {
class BrowserContext;
}

namespace ash {

class CertificateProviderService;

// Factory to create CertificateProviderService.
class CertificateProviderServiceFactory
    : public BrowserContextKeyedServiceFactory {
 public:
  static CertificateProviderService* GetForBrowserContext(
      content::BrowserContext* context);

  static CertificateProviderServiceFactory* GetInstance();

 private:
  friend struct base::DefaultSingletonTraits<CertificateProviderServiceFactory>;

  CertificateProviderServiceFactory();

  // BrowserContextKeyedServiceFactory:
  content::BrowserContext* GetBrowserContextToUse(
      content::BrowserContext* context) const override;
  bool ServiceIsNULLWhileTesting() const override;
  KeyedService* BuildServiceInstanceFor(
      content::BrowserContext* context) const override;

  DISALLOW_COPY_AND_ASSIGN(CertificateProviderServiceFactory);
};

}  // namespace ash

// TODO(https://crbug.com/1164001): remove when Chrome OS code migration is
// done.
namespace chromeos {
using ::ash::CertificateProviderServiceFactory;
}

#endif  // CHROME_BROWSER_ASH_CERTIFICATE_PROVIDER_CERTIFICATE_PROVIDER_SERVICE_FACTORY_H_
