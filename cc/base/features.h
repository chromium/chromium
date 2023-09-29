// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CC_BASE_FEATURES_H_
#define CC_BASE_FEATURES_H_

#include "base/feature_list.h"
#include "base/metrics/field_trial_params.h"
#include "build/build_config.h"
#include "cc/base/base_export.h"

namespace features {

CC_BASE_EXPORT BASE_DECLARE_FEATURE(kAnimatedImageResume);
CC_BASE_EXPORT extern bool IsImpulseScrollAnimationEnabled();
CC_BASE_EXPORT BASE_DECLARE_FEATURE(kSynchronizedScrolling);

// When enabled, the double tap to zoom will be disabled when the viewport
// meta tag is properly set for mobile using content=width=device-width
// or content=initial-scale=1.0
CC_BASE_EXPORT BASE_DECLARE_FEATURE(kRemoveMobileViewportDoubleTap);

// When enabled, scrolling within a covering snap area avoids or snaps to inner
// nested areas, avoiding resting on positions which do not snap the inner area.
// E.g. when scrolling within snap area A, it will stop either before/after
// snap area B or with B snapped.
//   --------
//  | A      |
//  |        |
//  |  ---   |
//  | | B |  |
//  |  ---   |
//  |        |
//   --------
CC_BASE_EXPORT BASE_DECLARE_FEATURE(kScrollSnapCoveringAvoidNestedSnapAreas);

// When enabled, scrolling within a covering snap area uses the native fling,
// allowing much more natural scrolling within these areas.
CC_BASE_EXPORT BASE_DECLARE_FEATURE(kScrollSnapCoveringUseNativeFling);

// When enabled, if a snap container is snapping to a large snap area, it will
// snap to the closest covering position if it is closer than the aligned
// position. This avoid unnecessary jumps that attempt to honor the
// scroll-snap-align value.
CC_BASE_EXPORT BASE_DECLARE_FEATURE(kScrollSnapPreferCloserCovering);

// When enabled, cc will show blink's Web-Vital metrics inside its heads up
// display.
CC_BASE_EXPORT BASE_DECLARE_FEATURE(kHudDisplayForPerformanceMetrics);

// Whether RenderSurface::common_ancestor_clip_id() is used to clip to the
// common ancestor clip when any contributing layer escapes the clip of the
// render surface's owning effect.
CC_BASE_EXPORT BASE_DECLARE_FEATURE(kRenderSurfaceCommonAncestorClip);

// When enabled, CompositorTimingHistory will directly record the timing history
// that is used to calculate main thread timing estimates, and use the
// percentile of sum of different stages instead of the sum of percentiles.
CC_BASE_EXPORT BASE_DECLARE_FEATURE(
    kDurationEstimatesInCompositorTimingHistory);

// When enabled, the main thread does not block while commit is running on the
// impl thread.
// WARNING: This feature is not yet safe to enable. Work is needed to ensure
// that main thread cc data structures are not modified on the main thread while
// commit is running concurrently on the impl thread.
CC_BASE_EXPORT BASE_DECLARE_FEATURE(kNonBlockingCommit);

// When enabled, LayerTreeImpl will not preserve the last mutation. This map
// of the last mutated value should not be necessary as animations are always
// ticked after the commit which should restore their animated values. Removing
// this should improve performance and reduce technical complexity.
CC_BASE_EXPORT BASE_DECLARE_FEATURE(kNoPreserveLastMutation);

// When enabled, DroppedFrameCounter will use an adjusted sliding window
// interval specified by field trial params.
CC_BASE_EXPORT BASE_DECLARE_FEATURE(kSlidingWindowForDroppedFrameCounter);

// When enabled, SupportsBackgroundThreadPriority is set to kNo for
// GpuImageDecodeTaskImpl and SoftwareImageDecodeTaskImpl.
// Introduced to fix https://crbug.com/1116624
CC_BASE_EXPORT BASE_DECLARE_FEATURE(kNormalPriorityImageDecoding);

// Use DMSAA instead of MSAA for rastering tiles.
CC_BASE_EXPORT BASE_DECLARE_FEATURE(kUseDMSAAForTiles);

#if BUILDFLAG(IS_ANDROID)
// Use DMSAA instead of MSAA for rastering tiles on Android GL backend. Note
// that the above flag kUseDMSAAForTiles is used for Android Vulkan backend.
CC_BASE_EXPORT BASE_DECLARE_FEATURE(kUseDMSAAForTilesAndroidGL);
#endif

// Updating browser controls state will IPC directly from browser main to the
// compositor thread. Previously this proxied through the renderer main thread.
CC_BASE_EXPORT BASE_DECLARE_FEATURE(kUpdateBrowserControlsWithoutProxy);

// Enables shared image cache for gpu used by CC instances instantiated for UI.
// TODO(https://crbug.com/c/1378251): this shall also be possible to use by
// renderers.
CC_BASE_EXPORT BASE_DECLARE_FEATURE(kUIEnableSharedImageCacheForGpu);

// When LayerTreeHostImpl::ReclaimResources() is called in background, trigger a
// flush to actually reclaim resources.
CC_BASE_EXPORT BASE_DECLARE_FEATURE(kReclaimResourcesFlushInBackground);

// When LayerTreeHostImpl::ReclaimResources() is called in background, trigger a
// additional delayed flush to reclaim resources.
CC_BASE_EXPORT BASE_DECLARE_FEATURE(kReclaimResourcesDelayedFlushInBackground);

// Try to play a longer list of ops before giving up in solid color analysis for
// tiles.
CC_BASE_EXPORT BASE_DECLARE_FEATURE(kMoreAggressiveSolidColorDetection);

// Allow CC FrameRateEstimater to reduce the frame rate to half of the default
// if the condition meets the requirement.
CC_BASE_EXPORT BASE_DECLARE_FEATURE(kReducedFrameRateEstimation);

// Use 4x MSAA (vs 8) on High DPI screens.
CC_BASE_EXPORT BASE_DECLARE_FEATURE(kDetectHiDpiForMsaa);

// When no frames are produced in a certain time interval, reclaim prepaint
// tiles.
CC_BASE_EXPORT BASE_DECLARE_FEATURE(kReclaimPrepaintTilesWhenIdle);

// Feature to reduce the area in which invisible tiles are kept around.
CC_BASE_EXPORT BASE_DECLARE_FEATURE(kSmallerInterestArea);

constexpr static int kDefaultInterestAreaSizeInPixels = 3000;
CC_BASE_EXPORT extern const base::FeatureParam<int> kInterestAreaSizeInPixels;

// Whether images marked "no-cache" are cached. When disabled, they are.
CC_BASE_EXPORT BASE_DECLARE_FEATURE(kImageCacheNoCache);

// When enabled, old prepaint tiles in the "eventually" region get reclaimed
// after some time.
CC_BASE_EXPORT BASE_DECLARE_FEATURE(kReclaimOldPrepaintTiles);
CC_BASE_EXPORT extern const base::FeatureParam<int> kReclaimDelayInSeconds;

// Kill switch for using MapRect() to compute filter pixel movement.
CC_BASE_EXPORT BASE_DECLARE_FEATURE(kUseMapRectForPixelMovement);

}  // namespace features

#endif  // CC_BASE_FEATURES_H_
