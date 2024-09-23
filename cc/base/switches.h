// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Defines all the "cc" command-line switches.

#ifndef CC_BASE_SWITCHES_H_
#define CC_BASE_SWITCHES_H_

#include "base/check.h"
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

// Switches for LayerTreeHost.
CC_BASE_EXPORT extern const char kDisableLayerTreeHostMemoryPressure[];

// Switches for raster.
CC_BASE_EXPORT extern const char kNumRasterThreads[];

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
CC_BASE_EXPORT extern const char kHighlightNonLCDTextLayers[];
#if DCHECK_IS_ON()
CC_BASE_EXPORT extern const char kLogOnUIDoubleBackgroundBlur[];
#endif

// Parameters for kUIShowCompositedLayerBorders.
CC_BASE_EXPORT extern const char kCompositedRenderPassBorders[];
CC_BASE_EXPORT extern const char kCompositedSurfaceBorders[];
CC_BASE_EXPORT extern const char kCompositedLayerBorders[];

CC_BASE_EXPORT extern const char kEnableClippedImageScaling[];

CC_BASE_EXPORT extern const char kAnimatedImageResume[];

// Test related.
CC_BASE_EXPORT extern const char kCCLayerTreeTestNoTimeout[];
CC_BASE_EXPORT extern const char kCCLayerTreeTestLongTimeout[];
CC_BASE_EXPORT extern const char kCCScrollAnimationDurationForTesting[];

}  // namespace switches
}  // namespace cc

#endif  // CC_BASE_SWITCHES_H_
