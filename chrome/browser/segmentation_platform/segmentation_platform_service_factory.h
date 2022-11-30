// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SEGMENTATION_PLATFORM_SEGMENTATION_PLATFORM_SERVICE_FACTORY_H_
#define CHROME_BROWSER_SEGMENTATION_PLATFORM_SEGMENTATION_PLATFORM_SERVICE_FACTORY_H_

#include "chrome/browser/profiles/profile_keyed_service_factory.h"

namespace base {
template <typename T>
struct DefaultSingletonTraits;
}  // namespace base

namespace content {
class BrowserContext;
}  // namespace content

class Profile;

namespace segmentation_platform {
class SegmentationPlatformService;

// A factory to create a unique SegmentationPlatformService.
class SegmentationPlatformServiceFactory : public ProfileKeyedServiceFactory {
 public:
  // Gets the SegmentationPlatformService for the profile. Returns a dummy one
  // if the feature isn't enabled.
  static SegmentationPlatformService* GetForProfile(Profile* profile);

  // Gets the lazy singleton instance of SegmentationPlatformService.
  static SegmentationPlatformServiceFactory* GetInstance();

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

  // BrowserContextKeyedServiceFactory overrides.
  KeyedService* BuildServiceInstanceFor(
      content::BrowserContext* context) const override;
};

}  // namespace segmentation_platform

#endif  // CHROME_BROWSER_SEGMENTATION_PLATFORM_SEGMENTATION_PLATFORM_SERVICE_FACTORY_H_
