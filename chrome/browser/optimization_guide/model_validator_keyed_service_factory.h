// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_OPTIMIZATION_GUIDE_MODEL_VALIDATOR_KEYED_SERVICE_FACTORY_H_
#define CHROME_BROWSER_OPTIMIZATION_GUIDE_MODEL_VALIDATOR_KEYED_SERVICE_FACTORY_H_

#include "base/no_destructor.h"
#include "chrome/browser/profiles/profile_keyed_service_factory.h"

namespace content {
class BrowserContext;
}  // namespace content

namespace optimization_guide {

class ModelValidatorKeyedService;

// LazyInstance that owns all ModelValidatorKeyedServices and associates them
// with Profiles.
class ModelValidatorKeyedServiceFactory : public ProfileKeyedServiceFactory {
 public:
  // Gets the LazyInstance that owns all ModelValidatorKeyedService(s).
  // Returns null if the model validation command-line flag is not specified.
  static ModelValidatorKeyedServiceFactory* GetInstance();

 private:
  friend base::NoDestructor<ModelValidatorKeyedServiceFactory>;

  ModelValidatorKeyedServiceFactory();
  ~ModelValidatorKeyedServiceFactory() override;

  // BrowserContextKeyedServiceFactory:
  std::unique_ptr<KeyedService> BuildServiceInstanceForBrowserContext(
      content::BrowserContext* context) const override;
  bool ServiceIsCreatedWithBrowserContext() const override;
};

}  // namespace optimization_guide

#endif  //  CHROME_BROWSER_OPTIMIZATION_GUIDE_MODEL_VALIDATOR_KEYED_SERVICE_FACTORY_H_
