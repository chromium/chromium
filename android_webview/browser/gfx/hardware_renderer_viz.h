// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ANDROID_WEBVIEW_BROWSER_GFX_HARDWARE_RENDERER_VIZ_H_
#define ANDROID_WEBVIEW_BROWSER_GFX_HARDWARE_RENDERER_VIZ_H_

#include <memory>

#include "android_webview/browser/gfx/hardware_renderer.h"
#include "android_webview/browser/gfx/output_surface_provider_webview.h"
#include "android_webview/browser/gfx/root_frame_sink.h"
#include "base/macros.h"
#include "base/threading/thread_checker.h"
#include "components/viz/common/surfaces/frame_sink_id.h"

namespace android_webview {

class HardwareRendererViz : public HardwareRenderer {
 public:
  HardwareRendererViz(RenderThreadManager* state,
                      RootFrameSinkGetter root_frame_sink_getter);
  ~HardwareRendererViz() override;

  // HardwareRenderer overrides.
  void DrawAndSwap(HardwareRendererDrawParams* params) override;

 private:
  class OnViz;

  void InitializeOnViz(RootFrameSinkGetter root_frame_sink_getter);
  void DestroyOnViz();
  bool IsUsingVulkan() const;

  // Information about last delegated frame.
  gfx::Size surface_size_;
  float device_scale_factor_ = 0;

  viz::SurfaceId surface_id_;

  // Used to create viz::OutputSurface and gl::GLSurface
  OutputSurfaceProviderWebview output_surface_provider_;

  // These are accessed on the viz thread.
  std::unique_ptr<OnViz> on_viz_;

  THREAD_CHECKER(render_thread_checker_);

  DISALLOW_COPY_AND_ASSIGN(HardwareRendererViz);
};

}  // namespace android_webview

#endif  // ANDROID_WEBVIEW_BROWSER_GFX_HARDWARE_RENDERER_VIZ_H_
