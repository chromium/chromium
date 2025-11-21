// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/base/features.h"

#include <atomic>
#include <string>

#include "base/feature_list.h"
#include "build/build_config.h"

namespace features {

namespace {
std::atomic<bool> s_is_eligible_for_throttle_main_frame_to_60hz = false;
}  // namespace

// When enabled, this forces raster translation to be computed using screen
// space and draw transforms scaled by external page scale factor.
// Whithout this, text in OOPIFs that isn't aligned to the pixel grid may appear
// blurry. https://crbug.com/399478935
BASE_FEATURE(kComputeRasterTranslateForExternalScale,
             base::FEATURE_ENABLED_BY_DEFAULT);

// Whether the compositor should attempt to sync with the scroll handlers before
// submitting a frame.
BASE_FEATURE(kSynchronizedScrolling,
#if BUILDFLAG(IS_ANDROID) || BUILDFLAG(IS_IOS)
             base::FEATURE_DISABLED_BY_DEFAULT);
#else
             base::FEATURE_ENABLED_BY_DEFAULT);
#endif

BASE_FEATURE(kDeferImplInvalidation, base::FEATURE_ENABLED_BY_DEFAULT);

// TODO(crbug.com/446920991): Reduce back to 1 frame delay once we have a
// separate delay for handling latency sensitive input.
const base::FeatureParam<int> kDeferImplInvalidationFrames{
    &kDeferImplInvalidation, "frames", 4};

// Note that kUseDMSAAForTiles only controls vulkan launch on android. We will
// be using a separate flag to control the launch on GL.
BASE_FEATURE(kUseDMSAAForTiles,
#if BUILDFLAG(IS_WIN) || BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_ANDROID)
             base::FEATURE_ENABLED_BY_DEFAULT
#else
             base::FEATURE_DISABLED_BY_DEFAULT
#endif
);

BASE_FEATURE(kReclaimPrepaintTilesWhenIdle, base::FEATURE_DISABLED_BY_DEFAULT);

// This saves memory on all platforms, but while on Android savings are
// significant (~10MiB or more of foreground memory), on desktop they were
// small, so only enable on Android.
//
// Disabled 04/2024 as it regresses checkerboarding metrics. Feature kept around
// to find a better balance between checkerboarding and memory.
BASE_FEATURE(kSmallerInterestArea, base::FEATURE_DISABLED_BY_DEFAULT);

const base::FeatureParam<int> kInterestAreaSizeInPixels{
    &kSmallerInterestArea, "size_in_pixels", kDefaultInterestAreaSizeInPixels};

BASE_FEATURE(kReclaimOldPrepaintTiles, base::FEATURE_DISABLED_BY_DEFAULT);

const base::FeatureParam<int> kReclaimDelayInSeconds{&kSmallerInterestArea,
                                                     "reclaim_delay_s", 30};

BASE_FEATURE(kTileOOMFreezeMitigation, base::FEATURE_ENABLED_BY_DEFAULT);

BASE_FEATURE(kClearCanvasResourcesInBackground,
             base::FEATURE_DISABLED_BY_DEFAULT);

BASE_FEATURE(kWaitForLateScrollEvents, base::FEATURE_ENABLED_BY_DEFAULT);

const base::FeatureParam<double> kWaitForLateScrollEventsDeadlineRatio{
    &kWaitForLateScrollEvents, "deadline_ratio", 0.333};

BASE_FEATURE(kPreserveDiscardableImageMapQuality,
             base::FEATURE_ENABLED_BY_DEFAULT);

BASE_FEATURE(kCCSlimming, base::FEATURE_ENABLED_BY_DEFAULT);

bool IsCCSlimmingEnabled() {
  static const bool enabled = base::FeatureList::IsEnabled(kCCSlimming);
  return enabled;
}

constexpr const char kScrollEventDispatchModeDispatchScrollEventsImmediately[] =
    "DispatchScrollEventsImmediately";
constexpr const char kScrollEventDispatchModeUseScrollPredictorForEmptyQueue[] =
    "UseScrollPredictorForEmptyQueue";
constexpr const char kScrollEventDispatchModeUseScrollPredictorForDeadline[] =
    "UseScrollPredictorForDeadline";
constexpr const char
    kScrollEventDispatchModeDispatchScrollEventsUntilDeadline[] =
        "DispatchScrollEventsUntilDeadline";
const base::FeatureParam<std::string> kScrollEventDispatchMode(
    &kWaitForLateScrollEvents,
    "mode",
    kScrollEventDispatchModeDispatchScrollEventsUntilDeadline);

BASE_FEATURE(kTreesInViz, base::FEATURE_DISABLED_BY_DEFAULT);

BASE_FEATURE(kTreeAnimationsInViz,
             "kTreeAnimationsInViz",
             base::FEATURE_DISABLED_BY_DEFAULT);

BASE_FEATURE(kSendExplicitDecodeRequestsImmediately,
             base::FEATURE_ENABLED_BY_DEFAULT);

BASE_FEATURE(kNewContentForCheckerboardedScrolls,
             base::FEATURE_ENABLED_BY_DEFAULT);
constexpr const char kNewContentForCheckerboardedScrollsPerScroll[] =
    "per_scroll";
constexpr const char kNewContentForCheckerboardedScrollsPerFrame[] =
    "per_frame";
const base::FeatureParam<std::string> kNewContentForCheckerboardedScrollsParam(
    &kNewContentForCheckerboardedScrolls,
    "mode",
    kNewContentForCheckerboardedScrollsPerScroll);

BASE_FEATURE(kAllowLCDTextWithFilter, base::FEATURE_ENABLED_BY_DEFAULT);

BASE_FEATURE(kPreventDuplicateImageDecodes, base::FEATURE_DISABLED_BY_DEFAULT);

BASE_FEATURE(kInitImageDecodeLastUseTime, base::FEATURE_ENABLED_BY_DEFAULT);

BASE_FEATURE(kThrottleMainFrameTo60Hz, base::FEATURE_DISABLED_BY_DEFAULT);

BASE_FEATURE(kBoostFrameRateForUrgentMainFrame,
             base::FEATURE_DISABLED_BY_DEFAULT);

void SetIsEligibleForThrottleMainFrameTo60Hz(bool is_eligible) {
  s_is_eligible_for_throttle_main_frame_to_60hz.store(
      true, std::memory_order_relaxed);
}

bool IsEligibleForThrottleMainFrameTo60Hz() {
  return s_is_eligible_for_throttle_main_frame_to_60hz.load(
      std::memory_order_relaxed);
}

BASE_FEATURE(kViewTransitionCaptureAndDisplay,
             base::FEATURE_ENABLED_BY_DEFAULT);

BASE_FEATURE(kViewTransitionFloorTransform, base::FEATURE_ENABLED_BY_DEFAULT);

// The feature is the enabled for the cc infrastructure to set the frame rate
// throttles from the main thread.
// The experiment will be controlled by the feature flag
// RenderBlockingFullFrameRate. Enabling the feature will not introduce any
// behavioral change by itself.
BASE_FEATURE(kRenderThrottleFrameRate, base::FEATURE_ENABLED_BY_DEFAULT);
const base::FeatureParam<int> kRenderThrottledFrameIntervalHz{
    &kRenderThrottleFrameRate, "render-throttled-frame-interval-hz", 30};

BASE_FEATURE(kFastPathNoRaster, base::FEATURE_ENABLED_BY_DEFAULT);

BASE_FEATURE(kInternalBeginFrameSourceOnManyDidNotProduceFrame,
             base::FEATURE_DISABLED_BY_DEFAULT);

// By default, internal begin frame source will be used when 4 consecutive
// "did not produce frame" are observed. It stops using internal begin frame
// source when there's a submitted compositor frame.
const base::FeatureParam<int>
    kNumDidNotProduceFrameBeforeInternalBeginFrameSource{
        &kInternalBeginFrameSourceOnManyDidNotProduceFrame,
        "num_did_not_produce_frame_before_internal_begin_frame_source", 4};

BASE_FEATURE(kUseLayerListsByDefault, base::FEATURE_DISABLED_BY_DEFAULT);

BASE_FEATURE(kProgrammaticScrollAnimationOverride,
             base::FEATURE_ENABLED_BY_DEFAULT);

#define PROGRAMMATIC_SCROLL_ANIMATION_CURVE(x1, y1, x2, y2, duration_ms)       \
  BASE_FEATURE_PARAM(double, kCubicBezierX1,                                   \
                     &kProgrammaticScrollAnimationOverride, "cubic_bezier_x1", \
                     x1);                                                      \
  BASE_FEATURE_PARAM(double, kCubicBezierY1,                                   \
                     &kProgrammaticScrollAnimationOverride, "cubic_bezier_y1", \
                     y1);                                                      \
  BASE_FEATURE_PARAM(double, kCubicBezierX2,                                   \
                     &kProgrammaticScrollAnimationOverride, "cubic_bezier_x2", \
                     x2);                                                      \
  BASE_FEATURE_PARAM(double, kCubicBezierY2,                                   \
                     &kProgrammaticScrollAnimationOverride, "cubic_bezier_y2", \
                     y2);                                                      \
  BASE_FEATURE_PARAM(base::TimeDelta, kMaxAnimationDuration,                   \
                     &kProgrammaticScrollAnimationOverride,                    \
                     "max_animation_duration",                                 \
                     base::Milliseconds(duration_ms))
// Default to `gfx::CubicBezierTimingFunction::EaseType::EASE_IN_OUT` on
// Android. On other platforms, use the tweaked cubic bezier curve.
#if BUILDFLAG(IS_ANDROID)
PROGRAMMATIC_SCROLL_ANIMATION_CURVE(0.42, 0.0, 0.58, 1.0, 700);
#else
PROGRAMMATIC_SCROLL_ANIMATION_CURVE(0.4, 0.0, 0.0, 1.0, 1500);
#endif
#undef PROGRAMMATIC_SCROLL_ANIMATION_CURVE

BASE_FEATURE(kSlimDirectReceiverIpc, base::FEATURE_ENABLED_BY_DEFAULT);

BASE_FEATURE(kOverscrollBehaviorRespectedOnAllScrollContainers,
             base::FEATURE_ENABLED_BY_DEFAULT);

BASE_FEATURE(kOverscrollEffectOnNonRootScrollers,
             base::FEATURE_DISABLED_BY_DEFAULT);

BASE_FEATURE(kSkipFinishDuringReleaseLayerTreeFrameSink,
             base::FEATURE_ENABLED_BY_DEFAULT);

BASE_FEATURE(kScrollJankV4Metric, base::FEATURE_ENABLED_BY_DEFAULT);

BASE_FEATURE_PARAM(double,
                   kScrollJankV4MetricStabilityCorrection,
                   &kScrollJankV4Metric,
                   "stability_correction",
                   0.05);

BASE_FEATURE_PARAM(double,
                   kScrollJankV4MetricDiscountFactor,
                   &kScrollJankV4Metric,
                   "discount_factor",
                   0.01);

BASE_FEATURE_PARAM(double,
                   kScrollJankV4MetricFastScrollContinuityThreshold,
                   &kScrollJankV4Metric,
                   "fast_scroll_continuity_threshold_pixels",
                   3.0);

BASE_FEATURE_PARAM(double,
                   kScrollJankV4MetricFlingContinuityThreshold,
                   &kScrollJankV4Metric,
                   "fling_continuity_threshold_pixels",
                   0.2);

BASE_FEATURE(kHandleNonDamagingInputsInScrollJankV4Metric,
             base::FEATURE_DISABLED_BY_DEFAULT);

BASE_FEATURE_PARAM(bool,
                   kCountNonDamagingFramesTowardsHistogramFrameCount,
                   &kHandleNonDamagingInputsInScrollJankV4Metric,
                   "count_non_damaging_frames_towards_histogram_frame_count",
                   false);

BASE_FEATURE(kManualBeginFrame, base::FEATURE_DISABLED_BY_DEFAULT);

BASE_FEATURE(kDropMetricsFromNonProducedFramesOnlyIfTheyHadNoDamage,
             base::FEATURE_DISABLED_BY_DEFAULT);

BASE_FEATURE(kUnlockDuringGpuImageOperations,
             base::FEATURE_DISABLED_BY_DEFAULT);

}  // namespace features
