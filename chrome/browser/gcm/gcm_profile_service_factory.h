// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_GCM_GCM_PROFILE_SERVICE_FACTORY_H_
#define CHROME_BROWSER_GCM_GCM_PROFILE_SERVICE_FACTORY_H_

#include "base/no_destructor.h"
#include "chrome/browser/profiles/profile_keyed_service_factory.h"
#include "components/gcm_driver/system_encryptor.h"

namespace gcm {

class GCMProfileService;

// Singleton that owns all GCMProfileService and associates them with
// Profiles.
class GCMProfileServiceFactory : public ProfileKeyedServiceFactory {
 public:
  // A repeating factory that can be installed globally for all `context`
  // objects (thus needs to be repeating factory).
  using GlobalTestingFactory =
      base::RepeatingCallback<std::unique_ptr<KeyedService>(
          content::BrowserContext*)>;

  static GCMProfileService* GetForProfile(content::BrowserContext* profile);
  static GCMProfileServiceFactory* GetInstance();

  // Helper registering a testing factory. Needs to be instantiated before the
  // factory is accessed in your test, and deallocated after the last access.
  // Usually this is achieved by putting this object as the first member in
  // your test fixture.
  class ScopedTestingFactoryInstaller {
   public:
    explicit ScopedTestingFactoryInstaller(
        GlobalTestingFactory testing_factory);

    ScopedTestingFactoryInstaller(const ScopedTestingFactoryInstaller&) =
        delete;
    ScopedTestingFactoryInstaller& operator=(
        const ScopedTestingFactoryInstaller&) = delete;

    ~ScopedTestingFactoryInstaller();
  };

  GCMProfileServiceFactory(const GCMProfileServiceFactory&) = delete;
  GCMProfileServiceFactory& operator=(const GCMProfileServiceFactory&) = delete;

 private:
  friend base::NoDestructor<GCMProfileServiceFactory>;

  GCMProfileServiceFactory();
  ~GCMProfileServiceFactory() override;

  // BrowserContextKeyedServiceFactory:
  KeyedService* BuildServiceInstanceFor(
      content::BrowserContext* profile) const override;
};

}  // namespace gcm

#endif  // CHROME_BROWSER_GCM_GCM_PROFILE_SERVICE_FACTORY_H_
