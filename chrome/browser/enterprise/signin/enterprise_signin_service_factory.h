// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ENTERPRISE_SIGNIN_ENTERPRISE_SIGNIN_SERVICE_FACTORY_H_
#define CHROME_BROWSER_ENTERPRISE_SIGNIN_ENTERPRISE_SIGNIN_SERVICE_FACTORY_H_

#include <memory>

#include "base/no_destructor.h"
#include "chrome/browser/enterprise/signin/enterprise_signin_service.h"
#include "chrome/browser/profiles/profile_keyed_service_factory.h"

namespace content {
class BrowserContext;
}  // namespace content

namespace enterprise_signin {

class EnterpriseSigninServiceFactory : public ProfileKeyedServiceFactory {
 public:
  static EnterpriseSigninServiceFactory* GetInstance();
  static EnterpriseSigninService* GetForBrowserContext(
      content::BrowserContext* browser_context);

  EnterpriseSigninServiceFactory(const EnterpriseSigninServiceFactory&) =
      delete;
  EnterpriseSigninServiceFactory& operator=(
      const EnterpriseSigninServiceFactory&) = delete;

 private:
  friend base::NoDestructor<EnterpriseSigninServiceFactory>;

  EnterpriseSigninServiceFactory();
  ~EnterpriseSigninServiceFactory() override;

  // BrowserContextKeyedServiceFactory overrides.
  std::unique_ptr<KeyedService> BuildServiceInstanceForBrowserContext(
      content::BrowserContext* context) const override;
  bool ServiceIsCreatedWithBrowserContext() const override;
};

}  // namespace enterprise_signin

#endif  // CHROME_BROWSER_ENTERPRISE_SIGNIN_ENTERPRISE_SIGNIN_SERVICE_FACTORY_H_
