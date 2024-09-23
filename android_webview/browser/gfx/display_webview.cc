// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "android_webview/browser/gfx/display_webview.h"

#include "android_webview/browser/gfx/overlay_processor_webview.h"
#include "android_webview/browser/gfx/root_frame_sink.h"
#include "base/memory/ptr_util.h"
#include "components/viz/common/features.h"
#include "components/viz/service/display/overlay_processor_stub.h"
#include "components/viz/service/frame_sinks/frame_sink_manager_impl.h"
#include "gpu/config/gpu_finch_features.h"

namespace android_webview {

std::unique_ptr<DisplayWebView> DisplayWebView::Create(
    const viz::RendererSettings& settings,
    const viz::DebugRendererSettings* debug_settings,
    const viz::FrameSinkId& frame_sink_id,
    std::unique_ptr<viz::DisplayCompositorMemoryAndTaskController>
        gpu_dependency,
    std::unique_ptr<viz::OutputSurface> output_surface,
    viz::FrameSinkManagerImpl* frame_sink_manager,
    RootFrameSink* root_frame_sink) {
  std::unique_ptr<viz::OverlayProcessorInterface> overlay_processor;
  OverlayProcessorWebView* overlay_processor_webview_raw = nullptr;
  if (features::IsAndroidSurfaceControlEnabled()) {
    // TODO(crbug.com/40113791): This is to help triage bugs on pre-release
    // android. Remove this log once feature is controlled only by feature flag
    // or launched.
    LOG(WARNING) << "WebView overlays are enabled!";
    auto overlay_processor_webview = std::make_unique<OverlayProcessorWebView>(
        gpu_dependency.get(), frame_sink_manager);
    overlay_processor_webview_raw = overlay_processor_webview.get();
    overlay_processor = std::move(overlay_processor_webview);
  } else {
    overlay_processor = std::make_unique<viz::OverlayProcessorStub>();
  }

  auto scheduler = std::make_unique<DisplaySchedulerWebView>(
      root_frame_sink, overlay_processor_webview_raw);

  return base::WrapUnique(new DisplayWebView(
      settings, debug_settings, frame_sink_id, std::move(gpu_dependency),
      std::move(output_surface), std::move(overlay_processor),
      std::move(scheduler), overlay_processor_webview_raw, frame_sink_manager,
      root_frame_sink));
}

DisplayWebView::DisplayWebView(
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
    RootFrameSink* root_frame_sink)
    : viz::Display(/*bitmap_manager=*/nullptr,
                   /*shared_image_manager=*/nullptr,
                   /*sync_point_manager=*/nullptr,
                   /*gpu_scheduler=*/nullptr,
                   settings,
                   debug_settings,
                   frame_sink_id,
                   std::move(gpu_dependency),
                   std::move(output_surface),
                   std::move(overlay_processor),
                   std::move(scheduler),
                   /*current_task_runner=*/nullptr),
      overlay_processor_webview_(overlay_processor_webview),
      frame_sink_manager_(frame_sink_manager),
      root_frame_sink_(root_frame_sink),
      use_new_invalidate_heuristic_(
          features::UseWebViewNewInvalidateHeuristic()) {
  if (overlay_processor_webview_) {
    frame_sink_manager_observation_.Observe(frame_sink_manager);
  }
}

DisplayWebView::~DisplayWebView() = default;

void DisplayWebView::OnFrameSinkDidFinishFrame(
    const viz::FrameSinkId& frame_sink_id,
    const viz::BeginFrameArgs& args) {
  DCHECK(overlay_processor_webview_);
  auto surface_id =
      overlay_processor_webview_->GetOverlaySurfaceId(frame_sink_id);
  if (surface_id.is_valid()) {
    auto* surface =
        frame_sink_manager_->surface_manager()->GetSurfaceForId(surface_id);
    DCHECK(surface);

    if (use_new_invalidate_heuristic_) {
      // For overlays we are going to display this frame immediately, so commit
      // it.
      surface->CommitFramesRecursively(
          [](const viz::SurfaceId&, const viz::BeginFrameId&) { return true; });
    }

    // TODO(vasilyt): We don't need full aggregation here as we don't need
    // aggregated frame.
    aggregator_->Aggregate(current_surface_id_, base::TimeTicks::Now(),
                           gfx::OVERLAY_TRANSFORM_NONE, gfx::Rect(),
                           ++swapped_trace_id_);
    auto* resolved_data = aggregator_->GetLatestFrameData(surface_id);
    if (resolved_data) {
      if (!overlay_processor_webview_->ProcessForFrameSinkId(frame_sink_id,
                                                             resolved_data)) {
        // If we failed to update overlay buffer, we need to invalidate to make
        // sure full draw happens.
        root_frame_sink_->InvalidateForOverlays();
      }
    }
  }
}

const base::flat_set<viz::SurfaceId>& DisplayWebView::GetContainedSurfaceIds() {
  return aggregator_->previous_contained_surfaces();
}

}  // namespace android_webview
