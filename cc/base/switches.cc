// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/base/switches.h"

#include "base/command_line.h"

namespace cc {
namespace switches {

const char kDisableThreadedAnimation[] = "disable-threaded-animation";

// Disables layer-edge anti-aliasing in the compositor.
const char kDisableCompositedAntialiasing[] =
    "disable-composited-antialiasing";

// Disables sending the next BeginMainFrame before the previous commit
// activates. Overrides the kEnableMainFrameBeforeActivation flag.
const char kDisableMainFrameBeforeActivation[] =
    "disable-main-frame-before-activation";

// Enables sending the next BeginMainFrame before the previous commit activates.
const char kEnableMainFrameBeforeActivation[] =
    "enable-main-frame-before-activation";

// Disabled defering all image decodes to the image decode service, ignoring
// DecodingMode preferences specified on PaintImage.
const char kDisableCheckerImaging[] = "disable-checker-imaging";

// Percentage of the browser controls need to be hidden before they will auto
// hide.
const char kBrowserControlsHideThreshold[] = "top-controls-hide-threshold";

// Percentage of the browser controls need to be shown before they will auto
// show.
const char kBrowserControlsShowThreshold[] = "top-controls-show-threshold";

// Re-rasters everything multiple times to simulate a much slower machine.
// Give a scale factor to cause raster to take that many times longer to
// complete, such as --slow-down-raster-scale-factor=25.
const char kSlowDownRasterScaleFactor[] = "slow-down-raster-scale-factor";

// Checks damage early and aborts the frame if no damage, so that clients like
// Android WebView don't invalidate unnecessarily.
const char kCheckDamageEarly[] = "check-damage-early";

// Enables the GPU benchmarking extension
const char kEnableGpuBenchmarking[] = "enable-gpu-benchmarking";

// Disables LayerTreeHost::OnMemoryPressure
const char kDisableLayerTreeHostMemoryPressure[] =
    "disable-layer-tree-host-memory-pressure";

// Controls the number of threads to use for raster tasks.
const char kNumRasterThreads[] = "num-raster-threads";

// Renders a border around compositor layers to help debug and study
// layer compositing.
const char kShowCompositedLayerBorders[] = "show-composited-layer-borders";
const char kUIShowCompositedLayerBorders[] = "ui-show-composited-layer-borders";
// Parameters for kUIShowCompositedLayerBorders.
const char kCompositedRenderPassBorders[] = "renderpass";
const char kCompositedSurfaceBorders[] = "surface";
const char kCompositedLayerBorders[] = "layer";

#if DCHECK_IS_ON()
// Checks and logs double background blur as an error if any.
const char kLogOnUIDoubleBackgroundBlur[] = "log-on-ui-double-background-blur";
#endif

// Draws a heads-up-display showing Frames Per Second as well as GPU memory
// usage. If you also use --enable-logging=stderr --vmodule="head*=1" then FPS
// will also be output to the console log.
const char kShowFPSCounter[] = "show-fps-counter";
const char kUIShowFPSCounter[] = "ui-show-fps-counter";

// Renders a border that represents the bounding box for the layer's animation.
const char kShowLayerAnimationBounds[] = "show-layer-animation-bounds";
const char kUIShowLayerAnimationBounds[] = "ui-show-layer-animation-bounds";

// Show rects in the HUD around layers whose properties have changed.
const char kShowPropertyChangedRects[] = "show-property-changed-rects";
const char kUIShowPropertyChangedRects[] = "ui-show-property-changed-rects";

// Show rects in the HUD around damage as it is recorded into each render
// surface.
const char kShowSurfaceDamageRects[] = "show-surface-damage-rects";
const char kUIShowSurfaceDamageRects[] = "ui-show-surface-damage-rects";

// Show rects in the HUD around the screen-space transformed bounds of every
// layer.
const char kShowScreenSpaceRects[] = "show-screenspace-rects";
const char kUIShowScreenSpaceRects[] = "ui-show-screenspace-rects";

// Highlights layers that can't use lcd text. Layers containing no text won't
// be highlighted. See DebugColors::NonLCDTextHighlightColor() for the colors.
const char kHighlightNonLCDTextLayers[] = "highlight-non-lcd-text-layers";

// Enables the resume method on animated images.
const char kAnimatedImageResume[] = "animated-image-resume";

// Allows scaling clipped images in GpuImageDecodeCache. Note that this may
// cause color-bleeding.
// TODO(crbug.com/40160880): Remove this workaround flag once the underlying
// cache problems are solved.
const char kEnableClippedImageScaling[] = "enable-scaling-clipped-images";

// Prevents the layer tree unit tests from timing out.
const char kCCLayerTreeTestNoTimeout[] = "cc-layer-tree-test-no-timeout";

// Increases timeout for memory checkers.
const char kCCLayerTreeTestLongTimeout[] = "cc-layer-tree-test-long-timeout";

// Controls the duration of the scroll animation curve.
const char kCCScrollAnimationDurationForTesting[] =
    "cc-scroll-animation-duration-in-seconds";

}  // namespace switches
}  // namespace cc
