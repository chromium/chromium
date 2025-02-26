// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ENTERPRISE_SIGNIN_ENTERPRISE_IDENTITY_SERVICE_FACTORY_H_
#define CHROME_BROWSER_ENTERPRISE_SIGNIN_ENTERPRISE_IDENTITY_SERVICE_FACTORY_H_

#include <memory>

#include "base/no_destructor.h"
#include "chrome/browser/profiles/profile_keyed_service_factory.h"

namespace enterprise {

class EnterpriseIdentityService;

class EnterpriseIdentityServiceFactory : public ProfileKeyedServiceFactory {
 public:
  static EnterpriseIdentityServiceFactory* GetInstance();
  static EnterpriseIdentityService* GetForBrowserContext(
      content::BrowserContext* browser_context);

  EnterpriseIdentityServiceFactory(const EnterpriseIdentityServiceFactory&) =
      delete;
  EnterpriseIdentityServiceFactory& operator=(
      const EnterpriseIdentityServiceFactory&) = delete;

 private:
  friend base::NoDestructor<EnterpriseIdentityServiceFactory>;

  EnterpriseIdentityServiceFactory();
  ~EnterpriseIdentityServiceFactory() override;

  // BrowserContextKeyedServiceFactory:
  std::unique_ptr<KeyedService> BuildServiceInstanceForBrowserContext(
      content::BrowserContext* context) const override;
};

}  // namespace enterprise

#endif  // CHROME_BROWSER_ENTERPRISE_SIGNIN_ENTERPRISE_IDENTITY_SERVICE_FACTORY_H_
