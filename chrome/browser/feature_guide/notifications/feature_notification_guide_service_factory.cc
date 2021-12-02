// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/feature_guide/notifications/feature_notification_guide_service_factory.h"

#include "base/memory/singleton.h"
#include "chrome/browser/feature_guide/notifications/feature_notification_guide_service.h"
#include "chrome/browser/feature_guide/notifications/internal/feature_notification_guide_service_impl.h"
#include "components/keyed_service/core/simple_dependency_manager.h"

namespace feature_guide {

// static
FeatureNotificationGuideServiceFactory*
FeatureNotificationGuideServiceFactory::GetInstance() {
  return base::Singleton<FeatureNotificationGuideServiceFactory>::get();
}

// static
FeatureNotificationGuideService*
FeatureNotificationGuideServiceFactory::GetForKey(SimpleFactoryKey* key) {
  return static_cast<FeatureNotificationGuideService*>(
      GetInstance()->GetServiceForKey(key, /*create=*/true));
}

FeatureNotificationGuideServiceFactory::FeatureNotificationGuideServiceFactory()
    : SimpleKeyedServiceFactory("FeatureNotificationGuideService",
                                SimpleDependencyManager::GetInstance()) {}

std::unique_ptr<KeyedService>
FeatureNotificationGuideServiceFactory::BuildServiceInstanceFor(
    SimpleFactoryKey* key) const {
  auto service = std::make_unique<FeatureNotificationGuideServiceImpl>();
  return std::move(service);
}

}  // namespace feature_guide
