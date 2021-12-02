// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_FEATURE_GUIDE_NOTIFICATIONS_FEATURE_NOTIFICATION_GUIDE_SERVICE_FACTORY_H_
#define CHROME_BROWSER_FEATURE_GUIDE_NOTIFICATIONS_FEATURE_NOTIFICATION_GUIDE_SERVICE_FACTORY_H_

#include <memory>

#include "components/keyed_service/core/simple_factory_key.h"
#include "components/keyed_service/core/simple_keyed_service_factory.h"

namespace base {
template <typename T>
struct DefaultSingletonTraits;
}  // namespace base

namespace feature_guide {
class FeatureNotificationGuideService;

// A factory to create one unique FeatureNotificationGuideService.
class FeatureNotificationGuideServiceFactory
    : public SimpleKeyedServiceFactory {
 public:
  static FeatureNotificationGuideServiceFactory* GetInstance();
  static FeatureNotificationGuideService* GetForKey(SimpleFactoryKey* key);

 private:
  friend struct base::DefaultSingletonTraits<
      FeatureNotificationGuideServiceFactory>;

  FeatureNotificationGuideServiceFactory();
  ~FeatureNotificationGuideServiceFactory() override = default;

  std::unique_ptr<KeyedService> BuildServiceInstanceFor(
      SimpleFactoryKey* key) const override;
};

}  // namespace feature_guide

#endif  // CHROME_BROWSER_FEATURE_GUIDE_NOTIFICATIONS_FEATURE_NOTIFICATION_GUIDE_SERVICE_FACTORY_H_
