// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_PRINTING_PRINT_MANAGEMENT_PRINTING_MANAGER_FACTORY_H_
#define CHROME_BROWSER_CHROMEOS_PRINTING_PRINT_MANAGEMENT_PRINTING_MANAGER_FACTORY_H_

#include "base/memory/singleton.h"
#include "components/keyed_service/content/browser_context_keyed_service_factory.h"

class PrefRegistrySimple;
class Profile;

namespace chromeos {
namespace printing {
namespace print_management {

class PrintingManager;

// Factory for PrintingManager.
class PrintingManagerFactory : public BrowserContextKeyedServiceFactory {
 public:
  static PrintingManager* GetForProfile(Profile* profile);
  static PrintingManagerFactory* GetInstance();
  static KeyedService* BuildInstanceFor(content::BrowserContext* profile);

  // Register the delete print job history preferences with the |registry|.
  static void RegisterProfilePrefs(PrefRegistrySimple* registry);

 private:
  friend struct base::DefaultSingletonTraits<PrintingManagerFactory>;

  PrintingManagerFactory();
  ~PrintingManagerFactory() override;

  PrintingManagerFactory(const PrintingManagerFactory&) = delete;
  PrintingManagerFactory& operator=(const PrintingManagerFactory&) = delete;

  // BrowserContextKeyedServiceFactory:
  KeyedService* BuildServiceInstanceFor(
      content::BrowserContext* context) const override;
  content::BrowserContext* GetBrowserContextToUse(
      content::BrowserContext* context) const override;
  bool ServiceIsCreatedWithBrowserContext() const override;
  bool ServiceIsNULLWhileTesting() const override;
};

}  // namespace print_management
}  // namespace printing
}  // namespace chromeos

#endif  // CHROME_BROWSER_CHROMEOS_PRINTING_PRINT_MANAGEMENT_PRINTING_MANAGER_FACTORY_H_
