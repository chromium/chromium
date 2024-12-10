// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CC_BASE_FEATURES_H_
#define CC_BASE_FEATURES_H_

#include <string>

#include "base/feature_list.h"
#include "base/metrics/field_trial_params.h"
#include "build/build_config.h"
#include "cc/base/base_export.h"

namespace features {

CC_BASE_EXPORT BASE_DECLARE_FEATURE(kAlignSurfaceLayerImplToPixelGrid);
CC_BASE_EXPORT BASE_DECLARE_FEATURE(kSynchronizedScrolling);

// Sets raster tree priority to NEW_CONTENT_TAKES_PRIORITY when performing a
// unified scroll with main-thread repaint reasons.
CC_BASE_EXPORT BASE_DECLARE_FEATURE(kMainRepaintScrollPrefersNewContent);

// When enabled, the scheduler will allow deferring impl invalidation frames
// for N frames (default 1) to reduce contention with main frames, allowing
// main a chance to commit.
CC_BASE_EXPORT BASE_DECLARE_FEATURE(kDeferImplInvalidation);
CC_BASE_EXPORT extern const base::FeatureParam<int>
    kDeferImplInvalidationFrames;

// Use DMSAA instead of MSAA for rastering tiles.
CC_BASE_EXPORT BASE_DECLARE_FEATURE(kUseDMSAAForTiles);

// Enables shared image cache for gpu used by CC instances instantiated for UI.
// TODO(https://crbug.com/c/1378251): this shall also be possible to use by
// renderers.
CC_BASE_EXPORT BASE_DECLARE_FEATURE(kUIEnableSharedImageCacheForGpu);

// When LayerTreeHostImpl::ReclaimResources() is called in background, trigger a
// additional delayed flush to reclaim resources.
//
// Enabled 03/2024, kept to run a holdback experiment.
CC_BASE_EXPORT BASE_DECLARE_FEATURE(kReclaimResourcesDelayedFlushInBackground);

// Use 4x MSAA (vs 8) on High DPI screens.
CC_BASE_EXPORT BASE_DECLARE_FEATURE(kDetectHiDpiForMsaa);

// When no frames are produced in a certain time interval, reclaim prepaint
// tiles.
CC_BASE_EXPORT BASE_DECLARE_FEATURE(kReclaimPrepaintTilesWhenIdle);

// Feature to reduce the area in which invisible tiles are kept around.
CC_BASE_EXPORT BASE_DECLARE_FEATURE(kSmallerInterestArea);

constexpr static int kDefaultInterestAreaSizeInPixels = 3000;
constexpr static int kDefaultInterestAreaSizeInPixelsWhenEnabled = 500;
CC_BASE_EXPORT extern const base::FeatureParam<int> kInterestAreaSizeInPixels;

// When enabled, old prepaint tiles in the "eventually" region get reclaimed
// after some time.
CC_BASE_EXPORT BASE_DECLARE_FEATURE(kReclaimOldPrepaintTiles);
CC_BASE_EXPORT extern const base::FeatureParam<int> kReclaimDelayInSeconds;

// Kill switch for using MapRect() to compute filter pixel movement.
CC_BASE_EXPORT BASE_DECLARE_FEATURE(kUseMapRectForPixelMovement);

// When enabled, we will not schedule drawing for viz::Surfaces that have been
// evicted. Instead waiting for an ActiveTree that is defining a newer
// viz::Surface.
CC_BASE_EXPORT BASE_DECLARE_FEATURE(kEvictionThrottlesDraw);

// Permits adjusting the threshold we use for determining if main thread updates
// are fast. Specifically, via a scalar on the range [0,1] that we multiply with
// the existing threshold. I.e., |new_threshold| = |scalar| * |old_threshold|.
CC_BASE_EXPORT BASE_DECLARE_FEATURE(kAdjustFastMainThreadThreshold);

// When a LayerTreeHostImpl is not visible, clear its transferable resources
// that haven't been imported into viz.
CC_BASE_EXPORT BASE_DECLARE_FEATURE(kClearCanvasResourcesInBackground);

// Currently CC Metrics does a lot of calculations for UMA and Tracing. While
// Traces themselves won't run when we are not tracing, some of the calculation
// work is done regardless. When enabled this feature reduces extra calculation
// to when tracing is enabled.
CC_BASE_EXPORT BASE_DECLARE_FEATURE(kMetricsTracingCalculationReduction);

// When enabled we will restore older FrameSequenceTracker sequence order
// enforcing that can miss backfilled frames.
CC_BASE_EXPORT BASE_DECLARE_FEATURE(kMetricsBackfillAdjustmentHoldback);

// When enabled we will submit the 'CopySharedImage' in one call and not batch
// it up into 4MiB increments.
CC_BASE_EXPORT BASE_DECLARE_FEATURE(kNonBatchedCopySharedImage);

// Currently there is a race between OnBeginFrames from the GPU process and
// input arriving from the Browser process. Due to this we can start to produce
// a frame while scrolling without any input events. Late arriving events are
// then enqueued for the next VSync.
//
// When this feature is enabled we will use the corresponding mode definted by
// `kScrollEventDispatchModeParamName`.
CC_BASE_EXPORT BASE_DECLARE_FEATURE(kWaitForLateScrollEvents);
CC_BASE_EXPORT extern const base::FeatureParam<double>
    kWaitForLateScrollEventsDeadlineRatio;

// When enabled we stop always pushing PictureLayerImpl properties on
// tree Activation. See crbug.com/40335690.
CC_BASE_EXPORT BASE_DECLARE_FEATURE(kDontAlwaysPushPictureLayerImpls);

// When enabled, image quality settings will be preserved in the discardable
// image map.
CC_BASE_EXPORT BASE_DECLARE_FEATURE(kPreserveDiscardableImageMapQuality);

// When enabled, the renderer asks the compositor to request warming up and
// create FrameSink speculatively even if invisible. Currently, this is intended
// to be used when prerender initial navigation is happening in background.
// Please see crbug.com/41496019 for more details.
CC_BASE_EXPORT BASE_DECLARE_FEATURE(kWarmUpCompositor);

// Kill switch for a bunch of optimizations for cc-slimming project.
// Please see crbug.com/335450599 for more details.
CC_BASE_EXPORT BASE_DECLARE_FEATURE(kCCSlimming);
// Check if the above feature is enabled. For performance purpose.
CC_BASE_EXPORT bool IsCCSlimmingEnabled();

// Modes for `kWaitForLateScrollEvents` changing event dispatch. Where the
// default is to just always enqueue scroll events.
//
// The ideal goal for both
// `kScrollEventDispatchModeNameDispatchScrollEventsImmediately` and
// `kScrollEventDispatchModeDispatchScrollEventsUntilDeadline` is that they will
// wait for `kWaitForLateScrollEventsDeadlineRatio` of the frame interval for
// input. During this time the first scroll event will be dispatched
// immediately. Subsequent scroll events will be enqueued. At the deadline we
// will resume frame production and enqueuing input.
//
// `kScrollEventDispatchModeNameDispatchScrollEventsImmediately` relies on
// `cc::Scheduler` to control the deadline. However this is overridden if we are
// waiting for Main-thread content. There are also fragile bugs which currently
// prevent enforcing the deadline if frame production is no longer required.
//
// `kScrollEventDispatchModeNameUseScrollPredictorForEmptyQueue` checks when
// we begin frame production, if the event queue is empty, we will generate a
// new prediction and dispatch a synthetic scroll event.
//
// `kScrollEventDispatchModeUseScrollPredictorForDeadline` will perform the
// same as `kScrollEventDispatchModeDispatchScrollEventsImmediately` until
// the deadline is encountered. Instead of immediately resuming frame
// production, we will first attempt to generate a new prediction to dispatch.
// As in `kScrollEventDispatchModeUseScrollPredictorForEmptyQueue`. After
// which we will resume frame production and enqueuing input.
//
// `kScrollEventDispatchModeDispatchScrollEventsUntilDeadline` relies on
// `blink::InputHandlerProxy` to directly enforce the deadline. This isolates us
// from cc scheduling bugs. Allowing us to no longer dispatch events, even if
// frame production has yet to complete.
CC_BASE_EXPORT extern const base::FeatureParam<std::string>
    kScrollEventDispatchMode;
CC_BASE_EXPORT extern const char
    kScrollEventDispatchModeDispatchScrollEventsImmediately[];
CC_BASE_EXPORT extern const char
    kScrollEventDispatchModeUseScrollPredictorForEmptyQueue[];
CC_BASE_EXPORT extern const char
    kScrollEventDispatchModeUseScrollPredictorForDeadline[];
CC_BASE_EXPORT extern const char
    kScrollEventDispatchModeDispatchScrollEventsUntilDeadline[];

// Enables GPU-side layer trees for content rendering.
CC_BASE_EXPORT BASE_DECLARE_FEATURE(kVizLayers);

// When enabled HTMLImageElement::decode() will initiate the decode task right
// away rather than piggy-backing on the next BeginMainFrame.
CC_BASE_EXPORT BASE_DECLARE_FEATURE(kSendExplicitDecodeRequestsImmediately);

// Whether frame rate should be throttled when there were many "did not produce
// frame" recently.
CC_BASE_EXPORT BASE_DECLARE_FEATURE(kThrottleFrameRateOnManyDidNotProduceFrame);
CC_BASE_EXPORT extern const base::FeatureParam<int>
    kNumDidNotProduceFrameBeforeThrottle;

// When enabled, the CC tree priority will be switched to
// NEW_CONTENT_TAKES_PRIORITY during long scroll that cause checkerboarding.
CC_BASE_EXPORT BASE_DECLARE_FEATURE(kNewContentForCheckerboardedScrolls);

// When enabled, impl-only scroll animations may execute concurrently.
CC_BASE_EXPORT BASE_DECLARE_FEATURE(kMultipleImplOnlyScrollAnimations);
CC_BASE_EXPORT extern bool MultiImplOnlyScrollAnimationsSupported();

// When enabled, and an image decode is requested by both a tile task and
// explicitly via img.decode(), it will be decoded only once.
CC_BASE_EXPORT BASE_DECLARE_FEATURE(kPreventDuplicateImageDecodes);

// When enabled, fix bug where an image decode cache entry last use timestamp is
// initialized to 0 instead of now.
CC_BASE_EXPORT BASE_DECLARE_FEATURE(kInitImageDecodeLastUseTime);

// The position affected by the safe area inset bottom will be handled by CC in
// the Render Compositor Thread. The transform metrix y is adjusted for all
// affected nodes.
CC_BASE_EXPORT BASE_DECLARE_FEATURE(kDynamicSafeAreaInsetsSupportedByCC);

}  // namespace features

#endif  // CC_BASE_FEATURES_H_
