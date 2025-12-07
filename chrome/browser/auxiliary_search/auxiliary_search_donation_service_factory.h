// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_AUXILIARY_SEARCH_AUXILIARY_SEARCH_DONATION_SERVICE_FACTORY_H_
#define CHROME_BROWSER_AUXILIARY_SEARCH_AUXILIARY_SEARCH_DONATION_SERVICE_FACTORY_H_

#include "chrome/browser/profiles/profile_keyed_service_factory.h"

namespace base {
template <typename T>
class NoDestructor;
}  // namespace base

class KeyedService;
class Profile;

class AuxiliarySearchDonationService;

// Singleton that owns all `AuxiliarySearchDonationService` instances, each
// mapped to one profile. Listens for profile destructions and clean up the
// associated AuxiliarySearchDonationServices.
class AuxiliarySearchDonationServiceFactory
    : public ProfileKeyedServiceFactory {
 public:
  // Returns the `AuxiliarySearchDonationService` instance for `profile`. Create
  // it if there is no instance.
  static AuxiliarySearchDonationService* GetForProfile(Profile* profile);

  // Gets the singleton instance of this factory class.
  static AuxiliarySearchDonationServiceFactory* GetInstance();

 private:
  friend base::NoDestructor<AuxiliarySearchDonationServiceFactory>;

  AuxiliarySearchDonationServiceFactory();
  ~AuxiliarySearchDonationServiceFactory() override;

  // ProfileKeyedServiceFactory:
  std::unique_ptr<KeyedService> BuildServiceInstanceForBrowserContext(
      content::BrowserContext* profile) const override;
  bool ServiceIsCreatedWithBrowserContext() const override;
  bool ServiceIsNULLWhileTesting() const override;
};

#endif  // CHROME_BROWSER_AUXILIARY_SEARCH_AUXILIARY_SEARCH_DONATION_SERVICE_FACTORY_H_
