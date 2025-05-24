// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ON_DEVICE_TRANSLATION_SERVICE_CONTROLLER_MANAGER_FACTORY_H_
#define CHROME_BROWSER_ON_DEVICE_TRANSLATION_SERVICE_CONTROLLER_MANAGER_FACTORY_H_

#include "base/memory/singleton.h"
#include "chrome/browser/profiles/profile_keyed_service_factory.h"

namespace on_device_translation {

class ServiceControllerManager;

// Factory for ServiceControllerManager.
class ServiceControllerManagerFactory : public ProfileKeyedServiceFactory {
 public:
  static ServiceControllerManagerFactory* GetInstance();

  ServiceControllerManager* Get(content::BrowserContext* context);

 private:
  friend struct base::DefaultSingletonTraits<ServiceControllerManagerFactory>;

  ServiceControllerManagerFactory();

  // BrowserContextKeyedServiceFactory overrides.
  std::unique_ptr<KeyedService> BuildServiceInstanceForBrowserContext(
      content::BrowserContext* profile) const override;
};

}  // namespace on_device_translation

#endif  // CHROME_BROWSER_ON_DEVICE_TRANSLATION_SERVICE_CONTROLLER_MANAGER_FACTORY_H_
