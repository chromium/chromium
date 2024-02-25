// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_CROS_APPS_CROS_APPS_KEY_EVENT_HANDLER_FACTORY_H_
#define CHROME_BROWSER_CHROMEOS_CROS_APPS_CROS_APPS_KEY_EVENT_HANDLER_FACTORY_H_

#include "base/no_destructor.h"
#include "chrome/browser/profiles/profile_keyed_service_factory.h"

// Class to retrieve the KeyEventHandler associated with a profile.
class CrosAppsKeyEventHandlerFactory : public ProfileKeyedServiceFactory {
 public:
  static CrosAppsKeyEventHandlerFactory& GetInstance();

 private:
  friend base::NoDestructor<CrosAppsKeyEventHandlerFactory>;

  CrosAppsKeyEventHandlerFactory();
  ~CrosAppsKeyEventHandlerFactory() override;

  // BrowserContextKeyedServiceFactory:
  std::unique_ptr<KeyedService> BuildServiceInstanceForBrowserContext(
      content::BrowserContext* context) const override;
  bool ServiceIsCreatedWithBrowserContext() const override;
};

#endif  // CHROME_BROWSER_CHROMEOS_CROS_APPS_CROS_APPS_KEY_EVENT_HANDLER_FACTORY_H_
