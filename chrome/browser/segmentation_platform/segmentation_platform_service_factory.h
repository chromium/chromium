// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SEGMENTATION_PLATFORM_SEGMENTATION_PLATFORM_SERVICE_FACTORY_H_
#define CHROME_BROWSER_SEGMENTATION_PLATFORM_SEGMENTATION_PLATFORM_SERVICE_FACTORY_H_

#include <memory>

#include "base/macros.h"
#include "components/keyed_service/core/simple_keyed_service_factory.h"

namespace base {
template <typename T>
struct DefaultSingletonTraits;
}  // namespace base

namespace segmentation_platform {
class SegmentationPlatformService;

// A factory to create a unique SegmentationPlatformService.
class SegmentationPlatformServiceFactory : public SimpleKeyedServiceFactory {
 public:
  // Returns singleton instance of SegmentationPlatformServiceFactory.
  static SegmentationPlatformServiceFactory* GetInstance();
  static SegmentationPlatformService* GetForKey(SimpleFactoryKey* key);

  // Disallow copy/assign.
  SegmentationPlatformServiceFactory(
      const SegmentationPlatformServiceFactory&) = delete;
  SegmentationPlatformServiceFactory& operator=(
      const SegmentationPlatformServiceFactory&) = delete;

 private:
  friend struct base::DefaultSingletonTraits<
      SegmentationPlatformServiceFactory>;

  SegmentationPlatformServiceFactory();
  ~SegmentationPlatformServiceFactory() override;

  // SimpleKeyedServiceFactory overrides.
  std::unique_ptr<KeyedService> BuildServiceInstanceFor(
      SimpleFactoryKey* key) const override;
};

}  // namespace segmentation_platform

#endif  // CHROME_BROWSER_SEGMENTATION_PLATFORM_SEGMENTATION_PLATFORM_SERVICE_FACTORY_H_
