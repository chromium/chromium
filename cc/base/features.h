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

// Enables partial raster in ZeroCopyRasterBufferProvider when used with the GPU
// compositor.
CC_BASE_EXPORT BASE_DECLARE_FEATURE(kZeroCopyRBPPartialRasterWithGpuCompositor);

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

// When a LayerTreeHostImpl is not visible, clear its transferable resources
// that haven't been imported into viz.
CC_BASE_EXPORT BASE_DECLARE_FEATURE(kClearCanvasResourcesInBackground);

// Currently CC Metrics does a lot of calculations for UMA and Tracing. While
// Traces themselves won't run when we are not tracing, some of the calculation
// work is done regardless. When enabled this feature reduces extra calculation
// to when tracing is enabled.
CC_BASE_EXPORT BASE_DECLARE_FEATURE(kMetricsTracingCalculationReduction);

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

// Enables Viz service-side layer trees for content rendering.
CC_BASE_EXPORT BASE_DECLARE_FEATURE(kTreesInViz);

// Enables Viz service-side layer tree animations for content rendering.
CC_BASE_EXPORT BASE_DECLARE_FEATURE(kTreeAnimationsInViz);

// When enabled HTMLImageElement::decode() will initiate the decode task right
// away rather than piggy-backing on the next BeginMainFrame.
CC_BASE_EXPORT BASE_DECLARE_FEATURE(kSendExplicitDecodeRequestsImmediately);

// When enabled, the CC tree priority will be switched to
// NEW_CONTENT_TAKES_PRIORITY during long scroll that cause checkerboarding.
CC_BASE_EXPORT BASE_DECLARE_FEATURE(kNewContentForCheckerboardedScrolls);

// When enabled, LCD text is allowed with some filters and backdrop filters.
// Killswitch M135.
CC_BASE_EXPORT BASE_DECLARE_FEATURE(kAllowLCDTextWithFilter);

// When enabled, impl-only scroll animations may execute concurrently.
CC_BASE_EXPORT BASE_DECLARE_FEATURE(kMultipleImplOnlyScrollAnimations);
CC_BASE_EXPORT extern bool MultiImplOnlyScrollAnimationsSupported();

// When enabled, for a render surface with fractional translation, we'll try to
// align the texels in the render surface to screen pixels to avoid blurriness
// during compositing.
CC_BASE_EXPORT BASE_DECLARE_FEATURE(kRenderSurfacePixelAlignment);

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

// On devices with a high refresh rate, whether to throttle main (not impl)
// frame production to 60Hz.
CC_BASE_EXPORT BASE_DECLARE_FEATURE(kThrottleMainFrameTo60Hz);

// We only want to test the feature value if the client satisfies an eligibility
// criteria, as testing the value enters the client into an experimental group,
// and we only want the groups (including control) to only contain eligibilie
// clients. This is also used for other feature that want to select from the
// samt pool.
CC_BASE_EXPORT bool IsEligibleForThrottleMainFrameTo60Hz();
CC_BASE_EXPORT void SetIsEligibleForThrottleMainFrameTo60Hz(bool is_eligible);

// A mode of ViewTransition capture that does not display unstyled frame,
// instead displays the properly constructed frame while at the same doing
// capture.
CC_BASE_EXPORT BASE_DECLARE_FEATURE(kViewTransitionCaptureAndDisplay);

// When enabled, we save the `EventMetrics` for a scroll, even when the result
// is no damage. So that the termination can be per properly attributed to the
// end of frame production for the given VSync.
CC_BASE_EXPORT BASE_DECLARE_FEATURE(kZeroScrollMetricsUpdate);

// When enabled, the view transition capture transform is floored instead of
// rounded and we use the render surface pixel snapping to counteract the blurry
// effect.
CC_BASE_EXPORT BASE_DECLARE_FEATURE(kViewTransitionFloorTransform);

// Allow the main thread to throttle the main frame rate.
// Note that the composited animations will not be affected.
// Typically the throttle is triggered with the render-blocking API <link
// rel="expect" blocking="full-frame-rate"/>.
CC_BASE_EXPORT BASE_DECLARE_FEATURE(kRenderThrottleFrameRate);
// The throttled frame rate when the main thread requests a throttle.
CC_BASE_EXPORT extern const base::FeatureParam<int>
    kRenderThrottledFrameIntervalHz;

// Adds a fast path to avoid waking up the thread pool when there are no raster
// tasks.
CC_BASE_EXPORT BASE_DECLARE_FEATURE(kFastPathNoRaster);

// When enabled, moves the layer tree client's metric export call
// for from beginning of the subsequent frame to the end of the subsequent
// frame.
CC_BASE_EXPORT BASE_DECLARE_FEATURE(kExportFrameTimingAfterFrameDone);

// When enabled, internal begin frame source will be used in cc to reduce IPC
// between cc and viz when there were many "did not produce frame" recently,
// and SetAutoNeedsBeginFrame will be called on CompositorFrameSink.
CC_BASE_EXPORT BASE_DECLARE_FEATURE(
    kInternalBeginFrameSourceOnManyDidNotProduceFrame);
CC_BASE_EXPORT extern const base::FeatureParam<int>
    kNumDidNotProduceFrameBeforeInternalBeginFrameSource;

// When enabled, the LayerTreeHost will expect to use layer lists instead of
// layer trees by default; the caller can explicitly opt into enabled or
// disabled if need be to override this.
CC_BASE_EXPORT BASE_DECLARE_FEATURE(kUseLayerListsByDefault);

// When enabled, the default programmatic scroll animation curve can be
// overridden with extra params.
CC_BASE_EXPORT BASE_DECLARE_FEATURE(kProgrammaticScrollAnimationOverride);
// Extra params to override the programmatic scroll animation.
CC_BASE_EXPORT BASE_DECLARE_FEATURE_PARAM(double, kCubicBezierX1);
CC_BASE_EXPORT BASE_DECLARE_FEATURE_PARAM(double, kCubicBezierY1);
CC_BASE_EXPORT BASE_DECLARE_FEATURE_PARAM(double, kCubicBezierX2);
CC_BASE_EXPORT BASE_DECLARE_FEATURE_PARAM(double, kCubicBezierY2);
CC_BASE_EXPORT BASE_DECLARE_FEATURE_PARAM(base::TimeDelta,
                                          kMaxAnimtionDuration);

}  // namespace features

#endif  // CC_BASE_FEATURES_H_
