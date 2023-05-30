// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_LONG_SCREENSHOTS_LONG_SCREENSHOTS_TAB_SERVICE_FACTORY_H_
#define CHROME_BROWSER_LONG_SCREENSHOTS_LONG_SCREENSHOTS_TAB_SERVICE_FACTORY_H_

#include <memory>

#include "base/no_destructor.h"
#include "components/keyed_service/core/keyed_service.h"
#include "components/keyed_service/core/simple_keyed_service_factory.h"

class SimpleFactoryKey;

namespace long_screenshots {
class LongScreenshotsTabService;

// Factory to create one LongScreenshotsTabService per profile key.
class LongScreenshotsTabServiceFactory : public SimpleKeyedServiceFactory {
 public:
  static LongScreenshotsTabServiceFactory* GetInstance();

  static long_screenshots::LongScreenshotsTabService* GetServiceInstance(
      SimpleFactoryKey* key);

  LongScreenshotsTabServiceFactory(const LongScreenshotsTabServiceFactory&) =
      delete;
  LongScreenshotsTabServiceFactory& operator=(
      const LongScreenshotsTabServiceFactory&) = delete;

 private:
  friend base::NoDestructor<LongScreenshotsTabServiceFactory>;

  LongScreenshotsTabServiceFactory();
  ~LongScreenshotsTabServiceFactory() override;

  // SimpleKeyedServiceFactory:
  std::unique_ptr<KeyedService> BuildServiceInstanceFor(
      SimpleFactoryKey* key) const override;
  SimpleFactoryKey* GetKeyToUse(SimpleFactoryKey* key) const override;
};

}  // namespace long_screenshots

#endif  // CHROME_BROWSER_LONG_SCREENSHOTS_LONG_SCREENSHOTS_TAB_SERVICE_FACTORY_H_
