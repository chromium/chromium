// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_DRIVE_DRIVE_INTEGRATION_SERVICE_FACTORY_H_
#define CHROME_BROWSER_ASH_DRIVE_DRIVE_INTEGRATION_SERVICE_FACTORY_H_

#include <memory>

#include "base/functional/callback.h"
#include "base/memory/singleton.h"
#include "chrome/browser/profiles/profile_keyed_service_factory.h"

class Profile;

namespace drive {

class DriveIntegrationService;

// Singleton that owns all instances of DriveIntegrationService and
// associates them with Profiles.
class DriveIntegrationServiceFactory : public ProfileKeyedServiceFactory {
 public:
  // Factory function used by tests.
  using FactoryCallback =
      base::RepeatingCallback<DriveIntegrationService*(Profile* profile)>;

  // Sets and resets a factory function for tests. See below for why we can't
  // use BrowserContextKeyedServiceFactory::SetTestingFactory().
  class ScopedFactoryForTest {
   public:
    explicit ScopedFactoryForTest(FactoryCallback* factory_for_test);
    ~ScopedFactoryForTest();
  };

  // Returns the DriveIntegrationService for |profile|, creating it if it is
  // not yet created.
  static DriveIntegrationService* GetForProfile(Profile* profile);

  // Returns the DriveIntegrationService that is already associated with
  // |profile|, if it is not yet created it will return NULL.
  static DriveIntegrationService* FindForProfile(Profile* profile);

  // Returns the DriveIntegrationServiceFactory instance.
  static DriveIntegrationServiceFactory* GetInstance();

 private:
  friend struct base::DefaultSingletonTraits<DriveIntegrationServiceFactory>;

  DriveIntegrationServiceFactory();
  ~DriveIntegrationServiceFactory() override;

  // BrowserContextKeyedServiceFactory overrides.
  std::unique_ptr<KeyedService> BuildServiceInstanceForBrowserContext(
      content::BrowserContext* context) const override;

  // This is static so it can be set without instantiating the factory. This
  // allows factory creation to be delayed until it normally happens (on profile
  // creation) rather than when tests are set up. DriveIntegrationServiceFactory
  // transitively depends on ChromeExtensionSystemFactory which crashes if
  // created too soon (i.e. before the BrowserProcess exists).
  static FactoryCallback* factory_for_test_;
};

}  // namespace drive

#endif  // CHROME_BROWSER_ASH_DRIVE_DRIVE_INTEGRATION_SERVICE_FACTORY_H_
