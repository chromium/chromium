// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/features.h"
#include "base/feature_list.h"

namespace base::features {

// Alphabetical:

// Enforce that writeable file handles passed to untrusted processes are not
// backed by executable files.
BASE_FEATURE(kEnforceNoExecutableFileHandles,
             "EnforceNoExecutableFileHandles",
             FEATURE_ENABLED_BY_DEFAULT);

// TODO(crbug.com/851128): Roll out this to 100% before replacing existing
// NOTREACHED()s with NOTREACHED_NORETURN() as part of NOTREACHED() migration.
// Note that a prerequisite for rolling out this experiment is that existing
// NOTREACHED reports are at a very low rate. Once this rolls out we should
// monitor that crash rates for the experiment population is within a 1-5% or
// lower than the control group.
BASE_FEATURE(kNotReachedIsFatal,
             "NotReachedIsFatal",
             FEATURE_DISABLED_BY_DEFAULT);

// Optimizes parsing and loading of data: URLs.
BASE_FEATURE(kOptimizeDataUrls, "OptimizeDataUrls", FEATURE_ENABLED_BY_DEFAULT);

BASE_FEATURE(kUseRustJsonParser,
             "UseRustJsonParser",
             FEATURE_DISABLED_BY_DEFAULT);

BASE_FEATURE(kJsonNegativeZero, "JsonNegativeZero", FEATURE_ENABLED_BY_DEFAULT);

#if BUILDFLAG(IS_ANDROID) || BUILDFLAG(IS_CHROMEOS)
// Force to enable LowEndDeviceMode partially on Android 3Gb devices.
// (see PartialLowEndModeOnMidRangeDevices below)
BASE_FEATURE(kPartialLowEndModeOn3GbDevices,
             "PartialLowEndModeOn3GbDevices",
             base::FEATURE_DISABLED_BY_DEFAULT);

// Used to enable LowEndDeviceMode partially on Android and ChromeOS mid-range
// devices. Such devices aren't considered low-end, but we'd like experiment
// with a subset of low-end features to see if we get a good memory vs.
// performance tradeoff.
//
// TODO(crbug.com/1434873): |#if| out 32-bit before launching or going to
// high Stable %, because we will enable the feature only for <8GB 64-bit
// devices, where we didn't ship yet. However, we first need a larger
// population to collect data.
BASE_FEATURE(kPartialLowEndModeOnMidRangeDevices,
             "PartialLowEndModeOnMidRangeDevices",
#if BUILDFLAG(IS_ANDROID)
             base::FEATURE_ENABLED_BY_DEFAULT);
#elif BUILDFLAG(IS_CHROMEOS)
             base::FEATURE_DISABLED_BY_DEFAULT);
#endif

#endif  // BUILDFLAG(IS_ANDROID) || BUILDFLAG(IS_CHROMEOS)

#if BUILDFLAG(IS_ANDROID)
// Whether to report frame metrics to the Android.FrameTimeline.* histograms.
BASE_FEATURE(kCollectAndroidFrameTimelineMetrics,
             "CollectAndroidFrameTimelineMetrics",
             base::FEATURE_DISABLED_BY_DEFAULT);
#endif  // BUILDFLAG(IS_ANDROID)

}  // namespace base::features
