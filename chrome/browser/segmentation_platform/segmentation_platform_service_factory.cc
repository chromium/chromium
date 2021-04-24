// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/segmentation_platform/segmentation_platform_service_factory.h"

#include "base/memory/singleton.h"
#include "components/keyed_service/core/simple_dependency_manager.h"
#include "components/segmentation_platform/internal/segmentation_platform_service_impl.h"

namespace segmentation_platform {

// static
SegmentationPlatformServiceFactory*
SegmentationPlatformServiceFactory::GetInstance() {
  return base::Singleton<SegmentationPlatformServiceFactory>::get();
}

// static
SegmentationPlatformService* SegmentationPlatformServiceFactory::GetForKey(
    SimpleFactoryKey* key) {
  return static_cast<SegmentationPlatformService*>(
      GetInstance()->GetServiceForKey(key, /*create=*/true));
}

SegmentationPlatformServiceFactory::SegmentationPlatformServiceFactory()
    : SimpleKeyedServiceFactory("SegmentationPlatformService",
                                SimpleDependencyManager::GetInstance()) {}

SegmentationPlatformServiceFactory::~SegmentationPlatformServiceFactory() =
    default;

std::unique_ptr<KeyedService>
SegmentationPlatformServiceFactory::BuildServiceInstanceFor(
    SimpleFactoryKey* key) const {
  // TODO(shaktisahu): Create the service object.
  return nullptr;
}

}  // namespace segmentation_platform
