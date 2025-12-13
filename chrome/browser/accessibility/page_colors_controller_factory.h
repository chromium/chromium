// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ACCESSIBILITY_PAGE_COLORS_CONTROLLER_FACTORY_H_
#define CHROME_BROWSER_ACCESSIBILITY_PAGE_COLORS_CONTROLLER_FACTORY_H_

#include "base/no_destructor.h"
#include "chrome/browser/profiles/profile_keyed_service_factory.h"
#include "components/pref_registry/pref_registry_syncable.h"

class Profile;
class PageColorsController;

// Factory to get or create an instance of PageColorsController from a Profile.
class PageColorsControllerFactory : public ProfileKeyedServiceFactory {
 public:
  static PageColorsControllerFactory* GetInstance();

  static PageColorsController* GetForProfile(Profile* profile);

 private:
  friend base::NoDestructor<PageColorsControllerFactory>;

  PageColorsControllerFactory();
  ~PageColorsControllerFactory() override;

  // BrowserContextKeyedServiceFactory:
  std::unique_ptr<KeyedService> BuildServiceInstanceForBrowserContext(
      content::BrowserContext* context) const override;
  bool ServiceIsCreatedWithBrowserContext() const override;
};

#endif  // CHROME_BROWSER_ACCESSIBILITY_PAGE_COLORS_CONTROLLER_FACTORY_H_
