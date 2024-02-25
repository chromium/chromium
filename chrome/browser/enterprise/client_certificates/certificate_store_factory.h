// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ENTERPRISE_CLIENT_CERTIFICATES_CERTIFICATE_STORE_FACTORY_H_
#define CHROME_BROWSER_ENTERPRISE_CLIENT_CERTIFICATES_CERTIFICATE_STORE_FACTORY_H_

#include "chrome/browser/profiles/profile_keyed_service_factory.h"

class Profile;

namespace base {
template <typename T>
class NoDestructor;
}

namespace client_certificates {

class CertificateStore;

class CertificateStoreFactory : public ProfileKeyedServiceFactory {
 public:
  static CertificateStoreFactory* GetInstance();
  static CertificateStore* GetForProfile(Profile* profile);

 private:
  friend base::NoDestructor<CertificateStoreFactory>;

  CertificateStoreFactory();
  ~CertificateStoreFactory() override;

  // BrowserContextKeyedServiceFactory implementation:
  std::unique_ptr<KeyedService> BuildServiceInstanceForBrowserContext(
      content::BrowserContext* context) const override;
};

}  // namespace client_certificates

#endif  // CHROME_BROWSER_ENTERPRISE_CLIENT_CERTIFICATES_CERTIFICATE_STORE_FACTORY_H_
