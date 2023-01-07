// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_PRINTING_PRINT_MANAGEMENT_PRINTING_MANAGER_FACTORY_H_
#define CHROME_BROWSER_ASH_PRINTING_PRINT_MANAGEMENT_PRINTING_MANAGER_FACTORY_H_

#include "base/memory/singleton.h"
#include "chrome/browser/profiles/profile_keyed_service_factory.h"

namespace user_prefs {
class PrefRegistrySyncable;
}  // namespace user_prefs

class Profile;

namespace ash {
namespace printing {
namespace print_management {

class PrintingManager;

// Factory for PrintingManager.
class PrintingManagerFactory : public ProfileKeyedServiceFactory {
 public:
  static PrintingManager* GetForProfile(Profile* profile);
  static PrintingManagerFactory* GetInstance();
  static KeyedService* BuildInstanceFor(content::BrowserContext* profile);

 private:
  friend struct base::DefaultSingletonTraits<PrintingManagerFactory>;

  PrintingManagerFactory();
  ~PrintingManagerFactory() override;

  PrintingManagerFactory(const PrintingManagerFactory&) = delete;
  PrintingManagerFactory& operator=(const PrintingManagerFactory&) = delete;

  // BrowserContextKeyedServiceFactory:
  KeyedService* BuildServiceInstanceFor(
      content::BrowserContext* context) const override;
  void RegisterProfilePrefs(
      user_prefs::PrefRegistrySyncable* registry) override;
  bool ServiceIsCreatedWithBrowserContext() const override;
  bool ServiceIsNULLWhileTesting() const override;
};

}  // namespace print_management
}  // namespace printing
}  // namespace ash

#endif  // CHROME_BROWSER_ASH_PRINTING_PRINT_MANAGEMENT_PRINTING_MANAGER_FACTORY_H_
