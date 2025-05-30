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

// When enabled, this forces composited textures for SurfaceLayerImpls to be
// aligned to the pixel grid. Lack of alignment can lead to blur, noticeably so
// in text. https://crbug.com/359279545
BASE_FEATURE(kAlignSurfaceLayerImplToPixelGrid,
             "AlignSurfaceLayerImplToPixelGrid",
             base::FEATURE_ENABLED_BY_DEFAULT);

// Whether the compositor should attempt to sync with the scroll handlers before
// submitting a frame.
BASE_FEATURE(kSynchronizedScrolling,
             "SynchronizedScrolling",
#if BUILDFLAG(IS_ANDROID) || BUILDFLAG(IS_IOS)
             base::FEATURE_DISABLED_BY_DEFAULT);
#else
             base::FEATURE_ENABLED_BY_DEFAULT);
#endif

BASE_FEATURE(kZeroCopyRBPPartialRasterWithGpuCompositor,
             "ZeroCopyRBPPartialRasterWithGpuCompositor",
             base::FEATURE_ENABLED_BY_DEFAULT);

BASE_FEATURE(kMainRepaintScrollPrefersNewContent,
             "MainRepaintScrollPrefersNewContent",
             base::FEATURE_ENABLED_BY_DEFAULT);

BASE_FEATURE(kDeferImplInvalidation,
             "DeferImplInvalidation",
             base::FEATURE_DISABLED_BY_DEFAULT);

const base::FeatureParam<int> kDeferImplInvalidationFrames{
    &kDeferImplInvalidation, "frames", 1};

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

BASE_FEATURE(kReclaimResourcesDelayedFlushInBackground,
             "ReclaimResourcesDelayedFlushInBackground",
             base::FEATURE_ENABLED_BY_DEFAULT);

BASE_FEATURE(kDetectHiDpiForMsaa,
             "DetectHiDpiForMsaa",
             base::FEATURE_DISABLED_BY_DEFAULT);

BASE_FEATURE(kReclaimPrepaintTilesWhenIdle,
             "ReclaimPrepaintTilesWhenIdle",
             base::FEATURE_DISABLED_BY_DEFAULT);

// This saves memory on all platforms, but while on Android savings are
// significant (~10MiB or more of foreground memory), on desktop they were
// small, so only enable on Android.
//
// Disabled 04/2024 as it regresses checkerboarding metrics. Feature kept around
// to find a better balance between checkerboarding and memory.
BASE_FEATURE(kSmallerInterestArea,
             "SmallerInterestArea",
             base::FEATURE_DISABLED_BY_DEFAULT
);

const base::FeatureParam<int> kInterestAreaSizeInPixels{
    &kSmallerInterestArea, "size_in_pixels", kDefaultInterestAreaSizeInPixels};

BASE_FEATURE(kReclaimOldPrepaintTiles,
             "ReclaimOldPrepaintTiles",
             base::FEATURE_DISABLED_BY_DEFAULT);

const base::FeatureParam<int> kReclaimDelayInSeconds{&kSmallerInterestArea,
                                                     "reclaim_delay_s", 30};

// This feature can be removed once M136 hits stable as long as no issues are
// reported that require it to be disabled in finch.
BASE_FEATURE(kUseMapRectForPixelMovement,
             "UseMapRectForPixelMovement",
             base::FEATURE_ENABLED_BY_DEFAULT);

BASE_FEATURE(kEvictionThrottlesDraw,
             "EvictionThrottlesDraw",
             base::FEATURE_ENABLED_BY_DEFAULT);

BASE_FEATURE(kClearCanvasResourcesInBackground,
             "ClearCanvasResourcesInBackground",
             base::FEATURE_DISABLED_BY_DEFAULT);

BASE_FEATURE(kMetricsTracingCalculationReduction,
             "MetricsTracingCalculationReduction",
             base::FEATURE_ENABLED_BY_DEFAULT);

BASE_FEATURE(kWaitForLateScrollEvents,
             "WaitForLateScrollEvents",
             base::FEATURE_ENABLED_BY_DEFAULT);

const base::FeatureParam<double> kWaitForLateScrollEventsDeadlineRatio{
    &kWaitForLateScrollEvents, "deadline_ratio", 0.333};

BASE_FEATURE(kDontAlwaysPushPictureLayerImpls,
             "DontAlwaysPushPictureLayerImpls",
             base::FEATURE_ENABLED_BY_DEFAULT);

BASE_FEATURE(kPreserveDiscardableImageMapQuality,
             "PreserveDiscardableImageMapQuality",
             base::FEATURE_ENABLED_BY_DEFAULT);

BASE_FEATURE(kCCSlimming, "CCSlimming", base::FEATURE_ENABLED_BY_DEFAULT);

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

BASE_FEATURE(kTreesInViz, "TreesInViz", base::FEATURE_DISABLED_BY_DEFAULT);

BASE_FEATURE(kTreeAnimationsInViz,
             "kTreeAnimationsInViz",
             base::FEATURE_DISABLED_BY_DEFAULT);

BASE_FEATURE(kSendExplicitDecodeRequestsImmediately,
             "SendExplicitDecodeRequestsImmediately",
             base::FEATURE_DISABLED_BY_DEFAULT);

BASE_FEATURE(kNewContentForCheckerboardedScrolls,
             "NewContentForCheckerboardedScrolls",
             base::FEATURE_ENABLED_BY_DEFAULT);

BASE_FEATURE(kAllowLCDTextWithFilter,
             "AllowLCDTextWithFilter",
             base::FEATURE_ENABLED_BY_DEFAULT);

BASE_FEATURE(kMultipleImplOnlyScrollAnimations,
             "MultipleImplOnlyScrollAnimations",
             base::FEATURE_ENABLED_BY_DEFAULT);
bool MultiImplOnlyScrollAnimationsSupported() {
  return base::FeatureList::IsEnabled(
      features::kMultipleImplOnlyScrollAnimations);
}

BASE_FEATURE(kRenderSurfacePixelAlignment,
             "RenderSurfacePixelAlignment",
             base::FEATURE_ENABLED_BY_DEFAULT);

BASE_FEATURE(kPreventDuplicateImageDecodes,
             "PreventDuplicateImageDecodes",
             base::FEATURE_DISABLED_BY_DEFAULT);

BASE_FEATURE(kInitImageDecodeLastUseTime,
             "InitImageDecodeLastUseTime",
             base::FEATURE_ENABLED_BY_DEFAULT);

BASE_FEATURE(kDynamicSafeAreaInsetsSupportedByCC,
             "DynamicSafeAreaInsetsSupportedByCC",
             base::FEATURE_ENABLED_BY_DEFAULT);

BASE_FEATURE(kThrottleMainFrameTo60Hz,
             "ThrottleMainFrameTo60Hz",
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
             "ViewTransitionCaptureAndDisplay",
             base::FEATURE_ENABLED_BY_DEFAULT);

BASE_FEATURE(kZeroScrollMetricsUpdate,
             "ZeroScrollMetricsUpdate",
             base::FEATURE_DISABLED_BY_DEFAULT);

BASE_FEATURE(kViewTransitionFloorTransform,
             "ViewTransitionFloorTransform",
             base::FEATURE_ENABLED_BY_DEFAULT);

// The feature is the enabled for the cc infrastructure to set the frame rate
// throttles from the main thread.
// The experiment will be controlled by the feature flag
// RenderBlockingFullFrameRate. Enabling the feature will not introduce any
// behavioral change by itself.
BASE_FEATURE(kRenderThrottleFrameRate,
             "RenderThrottleFrameRate",
             base::FEATURE_ENABLED_BY_DEFAULT);
const base::FeatureParam<int> kRenderThrottledFrameIntervalHz{
    &kRenderThrottleFrameRate, "render-throttled-frame-interval-hz", 30};

BASE_FEATURE(kFastPathNoRaster,
             "FastPathNoRaster",
             base::FEATURE_DISABLED_BY_DEFAULT);

BASE_FEATURE(kExportFrameTimingAfterFrameDone,
             "ExportFrameTimingAfterFrameDone",
             base::FEATURE_DISABLED_BY_DEFAULT);

BASE_FEATURE(kInternalBeginFrameSourceOnManyDidNotProduceFrame,
             "InternalBeginFrameSourceOnManyDidNotProduceFrame",
             base::FEATURE_DISABLED_BY_DEFAULT);

// By default, internal begin frame source will be used when 4 consecutive
// "did not produce frame" are observed. It stops using internal begin frame
// source when there's a submitted compositor frame.
const base::FeatureParam<int>
    kNumDidNotProduceFrameBeforeInternalBeginFrameSource{
        &kInternalBeginFrameSourceOnManyDidNotProduceFrame,
        "num_did_not_produce_frame_before_internal_begin_frame_source", 4};

BASE_FEATURE(kUseLayerListsByDefault,
             "UseLayerListsByDefault",
             base::FEATURE_DISABLED_BY_DEFAULT);

BASE_FEATURE(kProgrammaticScrollAnimationOverride,
             "ProgrammaticScrollAnimationOverride",
             base::FEATURE_DISABLED_BY_DEFAULT);

// Default to `gfx::CubicBezierTimingFunction::EaseType::EASE_IN_OUT`.
BASE_FEATURE_PARAM(double,
                   kCubicBezierX1,
                   &kProgrammaticScrollAnimationOverride,
                   "cubic_bezier_x1",
                   0.42);
BASE_FEATURE_PARAM(double,
                   kCubicBezierY1,
                   &kProgrammaticScrollAnimationOverride,
                   "cubic_bezier_y1",
                   0.0);
BASE_FEATURE_PARAM(double,
                   kCubicBezierX2,
                   &kProgrammaticScrollAnimationOverride,
                   "cubic_bezier_x2",
                   0.58);
BASE_FEATURE_PARAM(double,
                   kCubicBezierY2,
                   &kProgrammaticScrollAnimationOverride,
                   "cubic_bezier_y2",
                   1.0);

BASE_FEATURE_PARAM(base::TimeDelta,
                   kMaxAnimtionDuration,
                   &kProgrammaticScrollAnimationOverride,
                   "max_animation_duration",
                   base::Milliseconds(700));

}  // namespace features
