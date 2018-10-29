// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Defines all the "cc" command-line switches.

#ifndef CC_BASE_SWITCHES_H_
#define CC_BASE_SWITCHES_H_

#include "cc/base/base_export.h"

// Since cc is used from the render process, anything that goes here also needs
// to be added to render_process_host_impl.cc.

namespace cc {
namespace switches {

// Switches for the renderer compositor only.
CC_BASE_EXPORT extern const char kDisableThreadedAnimation[];
CC_BASE_EXPORT extern const char kDisableCompositedAntialiasing[];
CC_BASE_EXPORT extern const char kDisableMainFrameBeforeActivation[];
CC_BASE_EXPORT extern const char kEnableMainFrameBeforeActivation[];
CC_BASE_EXPORT extern const char kDisableCheckerImaging[];
CC_BASE_EXPORT extern const char kBrowserControlsHideThreshold[];
CC_BASE_EXPORT extern const char kBrowserControlsShowThreshold[];
CC_BASE_EXPORT extern const char kSlowDownRasterScaleFactor[];
CC_BASE_EXPORT extern const char kStrictLayerPropertyChangeChecking[];
CC_BASE_EXPORT extern const char kCheckDamageEarly[];

// Switches for both the renderer and ui compositors.
CC_BASE_EXPORT extern const char kEnableGpuBenchmarking[];
CC_BASE_EXPORT extern const char kAlwaysRequestPresentationTime[];

// Debug visualizations.
CC_BASE_EXPORT extern const char kShowCompositedLayerBorders[];
CC_BASE_EXPORT extern const char kUIShowCompositedLayerBorders[];
CC_BASE_EXPORT extern const char kShowFPSCounter[];
CC_BASE_EXPORT extern const char kUIShowFPSCounter[];
CC_BASE_EXPORT extern const char kShowLayerAnimationBounds[];
CC_BASE_EXPORT extern const char kUIShowLayerAnimationBounds[];
CC_BASE_EXPORT extern const char kShowPropertyChangedRects[];
CC_BASE_EXPORT extern const char kUIShowPropertyChangedRects[];
CC_BASE_EXPORT extern const char kShowSurfaceDamageRects[];
CC_BASE_EXPORT extern const char kUIShowSurfaceDamageRects[];
CC_BASE_EXPORT extern const char kShowScreenSpaceRects[];
CC_BASE_EXPORT extern const char kUIShowScreenSpaceRects[];
CC_BASE_EXPORT extern const char kUIEnableLayerLists[];
CC_BASE_EXPORT extern const char kCompositedRenderPassBorders[];
CC_BASE_EXPORT extern const char kCompositedSurfaceBorders[];
CC_BASE_EXPORT extern const char kCompositedLayerBorders[];

// Test related.
CC_BASE_EXPORT extern const char kCCLayerTreeTestNoTimeout[];
CC_BASE_EXPORT extern const char kCCLayerTreeTestLongTimeout[];
CC_BASE_EXPORT extern const char kCCRebaselinePixeltests[];

}  // namespace switches
}  // namespace cc

#endif  // CC_BASE_SWITCHES_H_
