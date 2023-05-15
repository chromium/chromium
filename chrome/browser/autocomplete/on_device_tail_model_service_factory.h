// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_AUTOCOMPLETE_ON_DEVICE_TAIL_MODEL_SERVICE_FACTORY_H_
#define CHROME_BROWSER_AUTOCOMPLETE_ON_DEVICE_TAIL_MODEL_SERVICE_FACTORY_H_

#include <memory>

#include "base/no_destructor.h"
#include "chrome/browser/profiles/profile_keyed_service_factory.h"

namespace content {
class BrowserContext;
}  // namespace content

class OnDeviceTailModelService;
class Profile;

// A factory to create a unique `OnDeviceTailModelService` per profile.
// Has dependency on `OptimizationGuideKeyedServiceFactory`.
class OnDeviceTailModelServiceFactory : public ProfileKeyedServiceFactory {
 public:
  // Gets the no destructor instance of `OnDeviceTailModelServiceFactory`.
  static OnDeviceTailModelServiceFactory* GetInstance();

  // Gets the `OnDeviceTailModelService` for the profile.
  static OnDeviceTailModelService* GetForProfile(Profile* profile);

  // Disallow copy/assign.
  OnDeviceTailModelServiceFactory(const OnDeviceTailModelServiceFactory&) =
      delete;
  OnDeviceTailModelServiceFactory& operator=(
      const OnDeviceTailModelServiceFactory&) = delete;

 private:
  friend class base::NoDestructor<OnDeviceTailModelServiceFactory>;

  OnDeviceTailModelServiceFactory();
  ~OnDeviceTailModelServiceFactory() override;

  // `BrowserContextKeyedServiceFactory` overrides.
  //
  // Returns nullptr if `OptimizationGuideKeyedService` is null.
  std::unique_ptr<KeyedService> BuildServiceInstanceForBrowserContext(
      content::BrowserContext* context) const override;
  bool ServiceIsCreatedWithBrowserContext() const override;
  bool ServiceIsNULLWhileTesting() const override;
};

#endif  // CHROME_BROWSER_AUTOCOMPLETE_ON_DEVICE_TAIL_MODEL_SERVICE_FACTORY_H_
