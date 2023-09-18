// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/base/features.h"

#include "base/feature_list.h"
#include "build/build_config.h"
#include "ui/base/ui_base_features.h"

namespace features {

// Uses the Resume method instead of the Catch-up method for animated images.
// - Catch-up behavior tries to keep animated images in pace with wall-clock
//   time. This might require decoding several animation frames if the
//   animation has fallen behind.
// - Resume behavior presents what would have been the next presented frame.
//   This means it might only decode one frame, resuming where it left off.
//   However, if the animation updates faster than the display's refresh rate,
//   it is possible to decode more than a single frame.
BASE_FEATURE(kAnimatedImageResume,
             "AnimatedImageResume",
             base::FEATURE_DISABLED_BY_DEFAULT);

bool IsImpulseScrollAnimationEnabled() {
  return base::FeatureList::IsEnabled(features::kWindowsScrollingPersonality);
}

// Whether the compositor should attempt to sync with the scroll handlers before
// submitting a frame.
BASE_FEATURE(kSynchronizedScrolling,
             "SynchronizedScrolling",
#if BUILDFLAG(IS_ANDROID) || BUILDFLAG(IS_IOS)
             base::FEATURE_DISABLED_BY_DEFAULT);
#else
             base::FEATURE_ENABLED_BY_DEFAULT);
#endif

BASE_FEATURE(kRemoveMobileViewportDoubleTap,
             "RemoveMobileViewportDoubleTap",
             base::FEATURE_ENABLED_BY_DEFAULT);

// Design doc: bit.ly/scrollunification
BASE_FEATURE(kScrollUnification,
             "ScrollUnification",
             base::FEATURE_ENABLED_BY_DEFAULT);

BASE_FEATURE(kScrollSnapCoveringAvoidNestedSnapAreas,
             "ScrollSnapCoveringAvoidNestedSnapAreas",
             base::FEATURE_ENABLED_BY_DEFAULT);

BASE_FEATURE(kScrollSnapCoveringUseNativeFling,
             "ScrollSnapCoveringUseNativeFling",
             base::FEATURE_ENABLED_BY_DEFAULT);

BASE_FEATURE(kScrollSnapPreferCloserCovering,
             "ScrollSnapPreferCloserCovering",
             base::FEATURE_ENABLED_BY_DEFAULT);

BASE_FEATURE(kHudDisplayForPerformanceMetrics,
             "HudDisplayForPerformanceMetrics",
             base::FEATURE_DISABLED_BY_DEFAULT);

BASE_FEATURE(kDurationEstimatesInCompositorTimingHistory,
             "DurationEstimatesInCompositorTimingHistory",
             base::FEATURE_DISABLED_BY_DEFAULT);

BASE_FEATURE(kNonBlockingCommit,
             "NonBlockingCommit",
             base::FEATURE_DISABLED_BY_DEFAULT);

BASE_FEATURE(kNoPreserveLastMutation,
             "NoPreserveLastMutation",
             base::FEATURE_ENABLED_BY_DEFAULT);

BASE_FEATURE(kSlidingWindowForDroppedFrameCounter,
             "SlidingWindowForDroppedFrameCounter",
             base::FEATURE_DISABLED_BY_DEFAULT);

BASE_FEATURE(kNormalPriorityImageDecoding,
             "NormalPriorityImageDecoding",
             base::FEATURE_ENABLED_BY_DEFAULT);

// Note that kUseDMSAAForTiles only controls vulkan launch on android. We will
// be using a separate flag to control the launch on GL.
BASE_FEATURE(kUseDMSAAForTiles,
             "UseDMSAAForTiles",
#if BUILDFLAG(IS_WIN) || BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_ANDROID)
             base::FEATURE_ENABLED_BY_DEFAULT
#else
             base::FEATURE_DISABLED_BY_DEFAULT
#endif
);

#if BUILDFLAG(IS_ANDROID)
// This flag controls the DMSAA for tile raster on Android GL backend whereas
// above flag UseDMSAAForTiles controls the launch on Vulkan backend.
BASE_FEATURE(kUseDMSAAForTilesAndroidGL,
             "UseDMSAAForTilesAndroidGL",
             base::FEATURE_DISABLED_BY_DEFAULT);
#endif

BASE_FEATURE(kUpdateBrowserControlsWithoutProxy,
             "UpdateBrowserControlsWithoutProxy",
             base::FEATURE_ENABLED_BY_DEFAULT);

BASE_FEATURE(kUIEnableSharedImageCacheForGpu,
             "UIEnableSharedImageCacheForGpu",
#if BUILDFLAG(IS_CHROMEOS_LACROS)
             base::FEATURE_ENABLED_BY_DEFAULT);
#else
             base::FEATURE_DISABLED_BY_DEFAULT);
#endif

BASE_FEATURE(kReclaimResourcesFlushInBackground,
             "ReclaimResourcesFlushInBackground",
             base::FEATURE_ENABLED_BY_DEFAULT);

BASE_FEATURE(kReclaimResourcesDelayedFlushInBackground,
             "ReclaimResourcesDelayedFlushInBackground",
             base::FEATURE_DISABLED_BY_DEFAULT);

BASE_FEATURE(kMoreAggressiveSolidColorDetection,
             "MoreAggressiveSolidColorDetection",
             base::FEATURE_ENABLED_BY_DEFAULT);

BASE_FEATURE(kReducedFrameRateEstimation,
             "kReducedFrameRateEstimation",
             base::FEATURE_ENABLED_BY_DEFAULT);

BASE_FEATURE(kDetectHiDpiForMsaa,
             "DetectHiDpiForMsaa",
             base::FEATURE_DISABLED_BY_DEFAULT);

BASE_FEATURE(kReclaimPrepaintTilesWhenIdle,
             "ReclaimPrepaintTilesWhenIdle",
             base::FEATURE_DISABLED_BY_DEFAULT);

BASE_FEATURE(kSmallerInterestArea,
             "SmallerInterestArea",
             base::FEATURE_DISABLED_BY_DEFAULT);

const base::FeatureParam<int> kInterestAreaSizeInPixels{
    &kSmallerInterestArea, "size_in_pixels", kDefaultInterestAreaSizeInPixels};

BASE_FEATURE(kImageCacheNoCache,
             "ImageCacheNoCache",
             base::FEATURE_DISABLED_BY_DEFAULT);

BASE_FEATURE(kReclaimOldPrepaintTiles,
             "ReclaimOldPrepaintTiles",
             base::FEATURE_DISABLED_BY_DEFAULT);

const base::FeatureParam<int> kReclaimDelayInSeconds{&kSmallerInterestArea,
                                                     "reclaim_delay_s", 30};

BASE_FEATURE(kUseMapRectForPixelMovement,
             "UseMapRectForPixelMovement",
             base::FEATURE_DISABLED_BY_DEFAULT);

}  // namespace features
