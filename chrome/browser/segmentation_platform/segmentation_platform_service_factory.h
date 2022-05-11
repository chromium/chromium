// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SEGMENTATION_PLATFORM_SEGMENTATION_PLATFORM_SERVICE_FACTORY_H_
#define CHROME_BROWSER_SEGMENTATION_PLATFORM_SEGMENTATION_PLATFORM_SERVICE_FACTORY_H_

#include "base/memory/raw_ptr.h"
#include "components/keyed_service/content/browser_context_keyed_service_factory.h"

namespace base {
template <typename T>
struct DefaultSingletonTraits;
}  // namespace base

namespace content {
class BrowserContext;
}  // namespace content

class PrefService;
class Profile;

namespace segmentation_platform {
class SegmentationPlatformService;

// A factory to create a unique SegmentationPlatformService.
class SegmentationPlatformServiceFactory
    : public BrowserContextKeyedServiceFactory {
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

  void set_local_state_for_testing(PrefService* local_state) {
    local_state_to_use_ = local_state;
  }

 private:
  friend struct base::DefaultSingletonTraits<
      SegmentationPlatformServiceFactory>;

  SegmentationPlatformServiceFactory();
  ~SegmentationPlatformServiceFactory() override;

  // BrowserContextKeyedServiceFactory overrides.
  KeyedService* BuildServiceInstanceFor(
      content::BrowserContext* context) const override;

  raw_ptr<PrefService> local_state_to_use_;
};

}  // namespace segmentation_platform

#endif  // CHROME_BROWSER_SEGMENTATION_PLATFORM_SEGMENTATION_PLATFORM_SERVICE_FACTORY_H_
