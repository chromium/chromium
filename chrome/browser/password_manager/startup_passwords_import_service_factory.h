// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_PASSWORD_MANAGER_STARTUP_PASSWORDS_IMPORT_SERVICE_FACTORY_H_
#define CHROME_BROWSER_PASSWORD_MANAGER_STARTUP_PASSWORDS_IMPORT_SERVICE_FACTORY_H_

#include "base/no_destructor.h"
#include "chrome/browser/profiles/profile_keyed_service_factory.h"

class StartupPasswordsImportService;
class Profile;

class StartupPasswordsImportServiceFactory : public ProfileKeyedServiceFactory {
 public:
  static StartupPasswordsImportService* GetForProfile(Profile* profile);

  static StartupPasswordsImportServiceFactory* GetInstance();

  // Not copyable or movable.
  StartupPasswordsImportServiceFactory(
      const StartupPasswordsImportServiceFactory&) = delete;
  StartupPasswordsImportServiceFactory& operator=(
      const StartupPasswordsImportServiceFactory&) = delete;

 private:
  friend class base::NoDestructor<StartupPasswordsImportServiceFactory>;

  StartupPasswordsImportServiceFactory();
  ~StartupPasswordsImportServiceFactory() override;

  // BrowserContextKeyedServiceFactory:
  std::unique_ptr<KeyedService> BuildServiceInstanceForBrowserContext(
      content::BrowserContext* context) const override;

  bool ServiceIsCreatedWithBrowserContext() const override;
};

#endif  // CHROME_BROWSER_PASSWORD_MANAGER_STARTUP_PASSWORDS_IMPORT_SERVICE_FACTORY_H_
