// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_EXTENSIONS_DESK_API_DESK_API_EXTENSION_MANAGER_FACTORY_H_
#define CHROME_BROWSER_CHROMEOS_EXTENSIONS_DESK_API_DESK_API_EXTENSION_MANAGER_FACTORY_H_

#include "base/no_destructor.h"
#include "chrome/browser/profiles/profile_keyed_service_factory.h"

namespace chromeos {

class DeskApiExtensionManager;

class DeskApiExtensionManagerFactory : public ProfileKeyedServiceFactory {
 public:
  // Returns the instance of `DeskApiExtensionManager` for the given profile.
  static DeskApiExtensionManager* GetForProfile(Profile* profile);

  // Returns the singleton instance of the factory.
  static DeskApiExtensionManagerFactory* GetInstance();

  DeskApiExtensionManagerFactory(const DeskApiExtensionManagerFactory&) =
      delete;
  DeskApiExtensionManagerFactory& operator=(
      const DeskApiExtensionManagerFactory&) = delete;

 private:
  friend base::NoDestructor<DeskApiExtensionManagerFactory>;

  DeskApiExtensionManagerFactory();
  ~DeskApiExtensionManagerFactory() override;

  KeyedService* BuildServiceInstanceFor(
      content::BrowserContext* context) const override;
};

}  // namespace chromeos

#endif  // CHROME_BROWSER_CHROMEOS_EXTENSIONS_DESK_API_DESK_API_EXTENSION_MANAGER_FACTORY_H_
