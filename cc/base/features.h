// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CC_BASE_FEATURES_H_
#define CC_BASE_FEATURES_H_

#include "base/feature_list.h"
#include "build/build_config.h"
#include "cc/base/base_export.h"

namespace features {

CC_BASE_EXPORT extern const base::Feature kAnimatedImageResume;
CC_BASE_EXPORT extern const base::Feature kImpulseScrollAnimations;
CC_BASE_EXPORT extern const base::Feature kSynchronizedScrolling;

#if !defined(OS_ANDROID)
CC_BASE_EXPORT extern const base::Feature kImplLatencyRecovery;
CC_BASE_EXPORT extern const base::Feature kMainLatencyRecovery;
#endif  // !defined(OS_ANDROID)

CC_BASE_EXPORT bool IsImplLatencyRecoveryEnabled();
CC_BASE_EXPORT bool IsMainLatencyRecoveryEnabled();

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

// When enabled, cc's layers support region-based wheel event hit-testing rather
// than only supporting flagging all layers, or no layers, as having blocking
// wheel event listeners.
// https://docs.google.com/document/d/1ar4WhVnLA-fmw6atgP-23iq-ys_NfFoGb3LA5AgaylA/edit?usp=sharing
CC_BASE_EXPORT extern const base::Feature kWheelEventRegions;

// When enabled, cc will show blink's Web-Vital metrics inside its heads up
// display.
CC_BASE_EXPORT extern const base::Feature kHudDisplayForPerformanceMetrics;

// When enabled, some jank is injected to the animation/scrolling pipeline.
CC_BASE_EXPORT extern const base::Feature kJankInjectionAblationFeature;

}  // namespace features

#endif  // CC_BASE_FEATURES_H_
