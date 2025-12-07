// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ANDROID_WEBVIEW_BROWSER_GFX_DISPLAY_WEBVIEW_H_
#define ANDROID_WEBVIEW_BROWSER_GFX_DISPLAY_WEBVIEW_H_

#include "base/containers/flat_set.h"
#include "base/memory/raw_ptr.h"
#include "base/scoped_observation.h"
#include "components/viz/service/display/display.h"
#include "components/viz/service/frame_sinks/frame_sink_manager_impl.h"
#include "components/viz/service/frame_sinks/frame_sink_observer.h"

namespace android_webview {
class OverlayProcessorWebView;
class RootFrameSink;

// Lifetime: WebView
class DisplayWebView : public viz::Display, public viz::FrameSinkObserver {
 public:
  static std::unique_ptr<DisplayWebView> Create(
      const viz::RendererSettings& settings,
      const viz::DebugRendererSettings* debug_settings,
      const viz::FrameSinkId& frame_sink_id,
      std::unique_ptr<viz::DisplayCompositorMemoryAndTaskController>
          gpu_dependency,
      std::unique_ptr<viz::OutputSurface> output_surface,
      viz::FrameSinkManagerImpl* frame_sink_manager,
      RootFrameSink* root_frame_sink);

  ~DisplayWebView() override;

  OverlayProcessorWebView* overlay_processor() const {
    return overlay_processor_webview_;
  }

  const base::flat_set<viz::SurfaceId>& GetContainedSurfaceIds();

  // viz::FrameSinkObserver implenentation:
  void OnFrameSinkDidFinishFrame(const viz::FrameSinkId& frame_sink_id,
                                 const viz::BeginFrameArgs& args) override;

 private:
  DisplayWebView(
      const viz::RendererSettings& settings,
      const viz::DebugRendererSettings* debug_settings,
      const viz::FrameSinkId& frame_sink_id,
      std::unique_ptr<viz::DisplayCompositorMemoryAndTaskController>
          gpu_dependency,
      std::unique_ptr<viz::OutputSurface> output_surface,
      std::unique_ptr<viz::OverlayProcessorInterface> overlay_processor,
      std::unique_ptr<viz::DisplaySchedulerBase> scheduler,
      OverlayProcessorWebView* overlay_processor_webview,
      viz::FrameSinkManagerImpl* frame_sink_manager,
      RootFrameSink* root_frame_sink);

  const raw_ptr<OverlayProcessorWebView> overlay_processor_webview_;
  const raw_ptr<viz::FrameSinkManagerImpl> frame_sink_manager_;
  const raw_ptr<RootFrameSink> root_frame_sink_;

  base::ScopedObservation<viz::FrameSinkManagerImpl, viz::FrameSinkObserver>
      frame_sink_manager_observation_{this};

  const bool use_new_invalidate_heuristic_;
};

}  // namespace android_webview

#endif  // ANDROID_WEBVIEW_BROWSER_GFX_DISPLAY_WEBVIEW_H_
