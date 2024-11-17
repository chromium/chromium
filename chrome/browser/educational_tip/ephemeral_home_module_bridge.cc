// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <jni.h>

#include "base/android/jni_array.h"
#include "base/android/jni_string.h"
#include "base/android/jni_weak_ref.h"
#include "base/memory/raw_ptr.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/segmentation_platform/segmentation_platform_service_factory.h"
#include "components/segmentation_platform/embedder/home_modules/constants.h"
#include "components/segmentation_platform/embedder/home_modules/home_modules_card_registry.h"
#include "components/segmentation_platform/public/segmentation_platform_service.h"

// Must come after all headers that specialize FromJniType() / ToJniType().
#include "chrome/browser/educational_tip/jni_headers/EducationalTipModuleMediator_jni.h"

using base::android::JavaParamRef;

enum EducationalTipCardType {
  kDefaultBrowserPromo = 0,
  kTabGroups = 1,
  kTabGroupSync = 2,
  kQuickDelete = 3,
};

static void JNI_EducationalTipModuleMediator_NotifyCardShown(JNIEnv* env,
                                                             Profile* profile,
                                                             jint card_type) {
  DCHECK(profile);
  segmentation_platform::home_modules::HomeModulesCardRegistry* registry =
      segmentation_platform::SegmentationPlatformServiceFactory::
          GetHomeModulesCardRegistry(profile);
  switch (static_cast<EducationalTipCardType>(card_type)) {
    case EducationalTipCardType::kDefaultBrowserPromo:
      registry->NotifyCardShown(segmentation_platform::kDefaultBrowserPromo);
      return;
    case EducationalTipCardType::kTabGroups:
      // TODO(crbug.com/355015904): add notify process for tab groups promo
      // card.
      return;
    case EducationalTipCardType::kTabGroupSync:
      // TODO(crbug.com/355015904): add notify process for tab group sync promo
      // card.
      return;
    case EducationalTipCardType::kQuickDelete:
      // TODO(crbug.com/355015904): add notify process for quick delete promo
      // card.
      return;
  }
}
