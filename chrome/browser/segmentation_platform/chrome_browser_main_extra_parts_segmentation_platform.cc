// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/segmentation_platform/chrome_browser_main_extra_parts_segmentation_platform.h"

#include "base/feature_list.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/segmentation_platform/segmentation_platform_service_factory.h"
#include "components/segmentation_platform/public/config.h"
#include "components/segmentation_platform/public/features.h"
#include "components/segmentation_platform/public/segmentation_platform_service.h"

void ChromeBrowserMainExtraPartsSegmentationPlatform::PostProfileInit() {
  Profile* last_used_profile =
      g_browser_process->profile_manager()->GetLastUsedProfileIfLoaded();
  if (!last_used_profile || last_used_profile->IsOffTheRecord())
    return;
  if (!base::FeatureList::IsEnabled(
          segmentation_platform::features::kSegmentationPlatformDummyFeature)) {
    return;
  }
  auto* service =
      segmentation_platform::SegmentationPlatformServiceFactory::GetForProfile(
          last_used_profile);
  service->GetSelectedSegment(segmentation_platform::kDummySegmentationKey,
                              base::DoNothing());
}
