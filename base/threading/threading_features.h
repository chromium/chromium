// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef BASE_THREADING_THREADING_FEATURES_H_
#define BASE_THREADING_THREADING_FEATURES_H_

#include "base/base_export.h"
#include "base/feature_list.h"
#include "build/build_config.h"

#if BUILDFLAG(IS_APPLE)
#include "base/metrics/field_trial_params.h"
#endif

namespace base {

#if BUILDFLAG(IS_APPLE)
BASE_EXPORT BASE_DECLARE_FEATURE(kOptimizedRealtimeThreadingMac);
extern const BASE_EXPORT FeatureParam<bool>
    kOptimizedRealtimeThreadingMacPreemptible;
extern const BASE_EXPORT FeatureParam<double>
    kOptimizedRealtimeThreadingMacBusy;
extern const BASE_EXPORT FeatureParam<double>
    kOptimizedRealtimeThreadingMacBusyLimit;
extern const BASE_EXPORT Feature kUserInteractiveCompositingMac;
#endif

#if BUILDFLAG(IS_WIN)
BASE_EXPORT BASE_DECLARE_FEATURE(kAboveNormalCompositingBrowserWin);
#endif

BASE_EXPORT BASE_DECLARE_FEATURE(kEnableHangWatcher);
BASE_EXPORT BASE_DECLARE_FEATURE(kEnableHangWatcherInZygoteChildren);

}  // namespace base

#endif  // BASE_THREADING_THREADING_FEATURES_H_
