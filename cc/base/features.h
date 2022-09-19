// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CC_BASE_FEATURES_H_
#define CC_BASE_FEATURES_H_

#include "base/feature_list.h"
#include "build/build_config.h"
#include "cc/base/base_export.h"

namespace features {

CC_BASE_EXPORT extern const base::Feature kAnimatedImageResume;
CC_BASE_EXPORT extern bool IsImpulseScrollAnimationEnabled();
CC_BASE_EXPORT extern const base::Feature kSynchronizedScrolling;

// When enabled, the double tap to zoom will be disabled when the viewport
// meta tag is properly set for mobile using content=width=device-width
// or content=initial-scale=1.0
CC_BASE_EXPORT extern const base::Feature kRemoveMobileViewportDoubleTap;

// When enabled, all scrolling is performed on the compositor thread -
// delegating only the hit test to Blink. This causes Blink to send additional
// information in the scroll property tree. When a scroll can't be hit tested
// on the compositor, it will post a hit test task to Blink and continue the
// scroll when that resolves. For details, see:
// https://docs.google.com/document/d/1smLAXs-DSLLmkEt4FIPP7PVglJXOcwRc7A5G0SEwxaY/edit
CC_BASE_EXPORT extern const base::Feature kScrollUnification;

// When enabled, wheel scrolls trigger smoothness mode. When disabled,
// smoothness mode is limited to non-animated (precision) scrolls, such as
// touch scrolling.
CC_BASE_EXPORT extern const base::Feature
    kSchedulerSmoothnessForAnimatedScrolls;

// When enabled, cc will show blink's Web-Vital metrics inside its heads up
// display.
CC_BASE_EXPORT extern const base::Feature kHudDisplayForPerformanceMetrics;

// When enabled, some jank is injected to the animation/scrolling pipeline.
CC_BASE_EXPORT extern const base::Feature kJankInjectionAblationFeature;

// When enabled, scheduler tree priority will change to
// NEW_CONTENT_TAKES_PRIORITY if during a scrollbar scroll, CC has to
// checkerboard.
CC_BASE_EXPORT extern const base::Feature
    kPreferNewContentForCheckerboardedScrolls;

// When enabled, CompositorTimingHistory will directly record the timing history
// that is used to calculate main thread timing estimates, and use the
// percentile of sum of different stages instead of the sum of percentiles.
CC_BASE_EXPORT extern const base::Feature
    kDurationEstimatesInCompositorTimingHistory;

// When enabled, the main thread does not block while commit is running on the
// impl thread.
// WARNING: This feature is not yet safe to enable. Work is needed to ensure
// that main thread cc data structures are not modified on the main thread while
// commit is running concurrently on the impl thread.
CC_BASE_EXPORT extern const base::Feature kNonBlockingCommit;

// When enabled, DroppedFrameCounter will use an adjusted sliding window
// interval specified by field trial params.
CC_BASE_EXPORT extern const base::Feature kSlidingWindowForDroppedFrameCounter;

// When enabled, SupportsBackgroundThreadPriority is set to kNo for
// GpuImageDecodeTaskImpl and SoftwareImageDecodeTaskImpl.
// Introduced to fix https://crbug.com/1116624
CC_BASE_EXPORT extern const base::Feature kNormalPriorityImageDecoding;

// When enabled commits are aborted if scroll and viewport state from CC could
// not be synchronized at the beginning of the frame because main frames were
// being deferred.
CC_BASE_EXPORT extern const base::Feature
    kSkipCommitsIfNotSynchronizingCompositorState;
}  // namespace features

#endif  // CC_BASE_FEATURES_H_
