// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/base/features.h"

#include "base/feature_list.h"
#include "build/build_config.h"

namespace features {

// Uses the Resume method instead of the Catch-up method for animated images.
// - Catch-up behavior tries to keep animated images in pace with wall-clock
//   time. This might require decoding several animation frames if the
//   animation has fallen behind.
// - Resume behavior presents what would have been the next presented frame.
//   This means it might only decode one frame, resuming where it left off.
//   However, if the animation updates faster than the display's refresh rate,
//   it is possible to decode more than a single frame.
const base::Feature kAnimatedImageResume = {"AnimatedImageResume",
                                            base::FEATURE_DISABLED_BY_DEFAULT};

// Enables impulse-style scroll animations in place of the default ones.
//
// Note: Do not enable this on the Mac. The animation does not match the system
// scroll animation curve to such an extent that it makes Chromium stand out in
// a bad way.
const base::Feature kImpulseScrollAnimations = {
    "ImpulseScrollAnimations",
    base::FEATURE_DISABLED_BY_DEFAULT};

bool IsImpulseScrollAnimationEnabled() {
  return base::FeatureList::IsEnabled(features::kImpulseScrollAnimations);
}

// Whether the compositor should attempt to sync with the scroll handlers before
// submitting a frame.
const base::Feature kSynchronizedScrolling = {
    "SynchronizedScrolling",
#if BUILDFLAG(IS_ANDROID)
    base::FEATURE_DISABLED_BY_DEFAULT};
#else
    base::FEATURE_ENABLED_BY_DEFAULT};
#endif

const base::Feature kRemoveMobileViewportDoubleTap{
    "RemoveMobileViewportDoubleTap", base::FEATURE_ENABLED_BY_DEFAULT};

const base::Feature kScrollUnification{"ScrollUnification",
                                       base::FEATURE_DISABLED_BY_DEFAULT};

const base::Feature kSchedulerSmoothnessForAnimatedScrolls{
    "SmoothnessModeForAnimatedScrolls", base::FEATURE_DISABLED_BY_DEFAULT};

const base::Feature kHudDisplayForPerformanceMetrics{
    "HudDisplayForPerformanceMetrics", base::FEATURE_DISABLED_BY_DEFAULT};

const base::Feature kJankInjectionAblationFeature{
    "JankInjectionAblation", base::FEATURE_DISABLED_BY_DEFAULT};

const base::Feature kPreferNewContentForCheckerboardedScrolls{
    "PreferNewContentForCheckerboardedScrolls",
    base::FEATURE_ENABLED_BY_DEFAULT};

const base::Feature kDurationEstimatesInCompositorTimingHistory{
    "DurationEstimatesInCompositorTimingHistory",
    base::FEATURE_DISABLED_BY_DEFAULT};

const base::Feature kNonBlockingCommit{"NonBlockingCommit",
                                       base::FEATURE_DISABLED_BY_DEFAULT};

const base::Feature kSlidingWindowForDroppedFrameCounter{
    "SlidingWindowForDroppedFrameCounter", base::FEATURE_DISABLED_BY_DEFAULT};
}  // namespace features
