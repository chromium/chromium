// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_EXTENSIONS_LOGIN_SCREEN_LOGIN_CLEANUP_CLEANUP_MANAGER_LACROS_FACTORY_H_
#define CHROME_BROWSER_CHROMEOS_EXTENSIONS_LOGIN_SCREEN_LOGIN_CLEANUP_CLEANUP_MANAGER_LACROS_FACTORY_H_

#include "base/no_destructor.h"
#include "chrome/browser/profiles/profile_keyed_service_factory.h"

namespace chromeos {

class CleanupManagerLacros;

// Factory for the `CleanupManagerLacros` KeyedService.
class CleanupManagerLacrosFactory : public ProfileKeyedServiceFactory {
 public:
  static CleanupManagerLacros* GetForBrowserContext(
      content::BrowserContext* browser_context);

  static CleanupManagerLacrosFactory* GetInstance();

  CleanupManagerLacrosFactory(const CleanupManagerLacrosFactory&) = delete;
  CleanupManagerLacrosFactory& operator=(const CleanupManagerLacrosFactory&) =
      delete;

 private:
  friend class base::NoDestructor<CleanupManagerLacrosFactory>;

  CleanupManagerLacrosFactory();
  ~CleanupManagerLacrosFactory() override;

  // BrowserContextKeyedServiceFactory:
  KeyedService* BuildServiceInstanceFor(
      content::BrowserContext* browser_context) const override;
  bool ServiceIsNULLWhileTesting() const override;
  bool ServiceIsCreatedWithBrowserContext() const override;
};

}  // namespace chromeos

#endif  // CHROME_BROWSER_CHROMEOS_EXTENSIONS_LOGIN_SCREEN_LOGIN_CLEANUP_CLEANUP_MANAGER_LACROS_FACTORY_H_
