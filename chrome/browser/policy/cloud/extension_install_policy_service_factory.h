
// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_POLICY_CLOUD_EXTENSION_INSTALL_POLICY_SERVICE_FACTORY_H_
#define CHROME_BROWSER_POLICY_CLOUD_EXTENSION_INSTALL_POLICY_SERVICE_FACTORY_H_

#include "base/no_destructor.h"
#include "chrome/browser/profiles/profile_keyed_service_factory.h"

namespace content {
class BrowserContext;
}

namespace policy {

class ExtensionInstallPolicyService;

class ExtensionInstallPolicyServiceFactory : public ProfileKeyedServiceFactory {
 public:
  static ExtensionInstallPolicyService* GetForBrowserContext(
      content::BrowserContext* context);

  static ExtensionInstallPolicyServiceFactory* GetInstance();

 private:
  friend base::NoDestructor<ExtensionInstallPolicyServiceFactory>;

  ExtensionInstallPolicyServiceFactory();
  ~ExtensionInstallPolicyServiceFactory() override;

  // BrowserContextKeyedServiceFactory implementation.
  std::unique_ptr<KeyedService> BuildServiceInstanceForBrowserContext(
      content::BrowserContext* context) const override;
};

}  // namespace policy

#endif  // CHROME_BROWSER_POLICY_CLOUD_EXTENSION_INSTALL_POLICY_SERVICE_FACTORY_H_
