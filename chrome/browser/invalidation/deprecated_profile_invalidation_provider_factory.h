// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_INVALIDATION_DEPRECATED_PROFILE_INVALIDATION_PROVIDER_FACTORY_H_
#define CHROME_BROWSER_INVALIDATION_DEPRECATED_PROFILE_INVALIDATION_PROVIDER_FACTORY_H_

#include "base/macros.h"
#include "base/memory/singleton.h"
#include "components/keyed_service/content/browser_context_keyed_service_factory.h"

class Profile;

namespace policy {
class AffiliatedInvalidationServiceProviderImplTest;
}

namespace user_prefs {
class PrefRegistrySyncable;
}

namespace invalidation {

class ProfileInvalidationProvider;

// A BrowserContextKeyedServiceFactory to construct InvalidationServices wrapped
// in ProfileInvalidationProviders. The implementation of InvalidationService
// may be completely different on different platforms; this class should help to
// hide this complexity. It also exposes some factory methods that are useful
// for setting up tests that rely on invalidations.
class DeprecatedProfileInvalidationProviderFactory
    : public BrowserContextKeyedServiceFactory {
 public:
  // Returns the ProfileInvalidationProvider for the given |profile|, lazily
  // creating one first if required. If |profile| does not support invalidation
  // (Chrome OS login profile, Chrome OS guest), returns NULL instead.
  static ProfileInvalidationProvider* GetForProfile(Profile* profile);

  static DeprecatedProfileInvalidationProviderFactory* GetInstance();

  // Switches service creation to go through |testing_factory| for all browser
  // contexts.
  void RegisterTestingFactory(TestingFactory testing_factory);

 private:
  friend class DeprecatedProfileInvalidationProviderFactoryTestBase;
  friend class policy::AffiliatedInvalidationServiceProviderImplTest;
  friend struct base::DefaultSingletonTraits<
      DeprecatedProfileInvalidationProviderFactory>;

  DeprecatedProfileInvalidationProviderFactory();
  ~DeprecatedProfileInvalidationProviderFactory() override;

  // BrowserContextKeyedServiceFactory:
  KeyedService* BuildServiceInstanceFor(
      content::BrowserContext* context) const override;
  void RegisterProfilePrefs(
      user_prefs::PrefRegistrySyncable* registry) override;

  TestingFactory testing_factory_;

  DISALLOW_COPY_AND_ASSIGN(DeprecatedProfileInvalidationProviderFactory);
};

}  // namespace invalidation

#endif  // CHROME_BROWSER_INVALIDATION_DEPRECATED_PROFILE_INVALIDATION_PROVIDER_FACTORY_H_
