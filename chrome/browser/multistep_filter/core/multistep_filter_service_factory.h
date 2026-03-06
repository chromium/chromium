// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
#ifndef CHROME_BROWSER_MULTISTEP_FILTER_CORE_MULTISTEP_FILTER_SERVICE_FACTORY_H_
#define CHROME_BROWSER_MULTISTEP_FILTER_CORE_MULTISTEP_FILTER_SERVICE_FACTORY_H_
#include "base/no_destructor.h"
#include "chrome/browser/profiles/profile_keyed_service_factory.h"

class Profile;

namespace multistep_filter {

class MultistepFilterService;

// Singleton that owns all MultistepFilterServices and associates them with
// Profiles.
//
// Responsible for instantiating MultistepFilterService and its
// non-KeyedService, non-Singleton dependencies for each Profile.
class MultistepFilterServiceFactory : public ProfileKeyedServiceFactory {
 public:
  MultistepFilterServiceFactory(const MultistepFilterServiceFactory&) = delete;
  MultistepFilterServiceFactory& operator=(
      const MultistepFilterServiceFactory&) = delete;

  static MultistepFilterServiceFactory* GetInstance();
  static MultistepFilterService* GetForProfile(Profile* profile);

 private:
  friend base::NoDestructor<MultistepFilterServiceFactory>;
  MultistepFilterServiceFactory();
  ~MultistepFilterServiceFactory() override;
  std::unique_ptr<KeyedService> BuildServiceInstanceForBrowserContext(
      content::BrowserContext* context) const override;
};

}  // namespace multistep_filter
#endif  // CHROME_BROWSER_MULTISTEP_FILTER_CORE_MULTISTEP_FILTER_SERVICE_FACTORY_H_
