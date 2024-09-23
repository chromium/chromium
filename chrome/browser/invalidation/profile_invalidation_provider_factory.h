// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_INVALIDATION_PROFILE_INVALIDATION_PROVIDER_FACTORY_H_
#define CHROME_BROWSER_INVALIDATION_PROFILE_INVALIDATION_PROVIDER_FACTORY_H_

#include "base/no_destructor.h"
#include "chrome/browser/profiles/profile_keyed_service_factory.h"

class Profile;

namespace policy {
class AffiliatedInvalidationServiceProviderImplTest;
}

namespace invalidation {

class ProfileInvalidationProvider;

// A BrowserContextKeyedServiceFactory to construct InvalidationServices wrapped
// in ProfileInvalidationProviders. The implementation of InvalidationService
// may be completely different on different platforms; this class should help to
// hide this complexity. It also exposes some factory methods that are useful
// for setting up tests that rely on invalidations.
class ProfileInvalidationProviderFactory : public ProfileKeyedServiceFactory {
 public:
  // A repeating factory that can be installed globally for all `context`
  // objects (thus needs to be repeating factory).
  using GlobalTestingFactory =
      base::RepeatingCallback<std::unique_ptr<KeyedService>(
          content::BrowserContext*)>;

  // Returns the ProfileInvalidationProvider for the given |profile|, lazily
  // creating one first if required. If |profile| does not support invalidation
  // (Chrome OS login profile, Chrome OS guest), returns NULL instead.
  static ProfileInvalidationProvider* GetForProfile(Profile* profile);

  static ProfileInvalidationProviderFactory* GetInstance();

  ProfileInvalidationProviderFactory(
      const ProfileInvalidationProviderFactory&) = delete;
  ProfileInvalidationProviderFactory& operator=(
      const ProfileInvalidationProviderFactory&) = delete;

  // Switches service creation to go through |testing_factory| for all browser
  // contexts.
  void RegisterTestingFactory(GlobalTestingFactory testing_factory);

 private:
  friend class ProfileInvalidationProviderFactoryTestBase;
  friend class policy::AffiliatedInvalidationServiceProviderImplTest;
  friend base::NoDestructor<ProfileInvalidationProviderFactory>;

  ProfileInvalidationProviderFactory();
  ~ProfileInvalidationProviderFactory() override;

  // BrowserContextKeyedServiceFactory:
  std::unique_ptr<KeyedService> BuildServiceInstanceForBrowserContext(
      content::BrowserContext* context) const override;
  void RegisterProfilePrefs(
      user_prefs::PrefRegistrySyncable* registry) override;

  GlobalTestingFactory testing_factory_;
};

}  // namespace invalidation

#endif  // CHROME_BROWSER_INVALIDATION_PROFILE_INVALIDATION_PROVIDER_FACTORY_H_
