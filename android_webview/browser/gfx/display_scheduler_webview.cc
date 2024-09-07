// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "android_webview/browser/gfx/display_scheduler_webview.h"

#include "android_webview/browser/gfx/root_frame_sink.h"
#include "android_webview/browser/gfx/viz_compositor_thread_runner_webview.h"
#include "base/trace_event/trace_event.h"
#include "components/viz/common/features.h"
#include "components/viz/service/frame_sinks/frame_sink_manager_impl.h"

namespace android_webview {
DisplaySchedulerWebView::DisplaySchedulerWebView(
    RootFrameSink* root_frame_sink,
    OverlaysInfoProvider* overlays_info_provider)
    : root_frame_sink_(root_frame_sink),
      overlays_info_provider_(overlays_info_provider),
      use_new_invalidate_heuristic_(
          features::UseWebViewNewInvalidateHeuristic()) {
  auto* frame_sink_manager =
      VizCompositorThreadRunnerWebView::GetInstance()->GetFrameSinkManager();

  surface_manager_observation_.Observe(frame_sink_manager->surface_manager());
  frame_sink_manager_observation_.Observe(frame_sink_manager);
}

DisplaySchedulerWebView::~DisplaySchedulerWebView() {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
}

void DisplaySchedulerWebView::ForceImmediateSwapIfPossible() {
  // We can't swap immediately
  NOTREACHED();
}
void DisplaySchedulerWebView::SetNeedsOneBeginFrame(bool needs_draw) {
  NOTREACHED();
}
void DisplaySchedulerWebView::DidSwapBuffers() {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);

  // Code below is part of old invalidation heuristic.
  if (use_new_invalidate_heuristic_)
    return;

  bool needs_draw = false;
  for (auto it = damaged_frames_.begin(); it != damaged_frames_.end();) {
    DCHECK_GT(it->second, 0);
    if (!--it->second) {
      it = damaged_frames_.erase(it);
    } else {
      if (!needs_draw) {
        TRACE_EVENT_INSTANT2(
            "android_webview",
            "DisplaySchedulerWebView::DidSwapBuffers first needs_draw",
            TRACE_EVENT_SCOPE_THREAD, "frame_sink_id", it->first.ToString(),
            "damage_count", it->second);
      }
      needs_draw = true;
      ++it;
    }
  }

  root_frame_sink_->SetNeedsDraw(needs_draw);
}
void DisplaySchedulerWebView::OutputSurfaceLost() {
}

bool DisplaySchedulerWebView::IsFrameSinkOverlayed(
    viz::FrameSinkId frame_sink_id) {
  return overlays_info_provider_ &&
         overlays_info_provider_->IsFrameSinkOverlayed(frame_sink_id);
}

void DisplaySchedulerWebView::OnDisplayDamaged(viz::SurfaceId surface_id) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);

  // Code below is part of old invalidation heuristic.
  if (use_new_invalidate_heuristic_)
    return;

  // We don't need to track damage of root frame sink as we submit frame to it
  // at DrawAndSwap and Root Renderer sink because Android View.Invalidation is
  // handled by SynchronousCompositorHost.

  if (surface_id.frame_sink_id() != root_frame_sink_->root_frame_sink_id() &&
      !root_frame_sink_->IsChildSurface(surface_id.frame_sink_id()) &&
      !IsFrameSinkOverlayed(surface_id.frame_sink_id())) {
    int count = damaged_frames_[surface_id.frame_sink_id()] + 1;

    TRACE_EVENT_INSTANT2(
        "android_webview", "DisplaySchedulerWebView::OnDisplayDamaged",
        TRACE_EVENT_SCOPE_THREAD, "frame_sink_id",
        surface_id.frame_sink_id().ToString(), "damage_count", count);

    // Clamp value to max two frames. Two is enough to keep invalidation
    // working, but will prevent number going too high in case if kModeDraw
    // doesn't happen for some time.
    damaged_frames_[surface_id.frame_sink_id()] = std::min(2, count);

    root_frame_sink_->SetNeedsDraw(true);
  }
}

void DisplaySchedulerWebView::OnSurfaceHasNewUncommittedFrame(
    const viz::SurfaceId& surface_id) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);

  // We don't need to track damage of root frame sink as we submit frame to it
  // at DrawAndSwap and Root Renderer sink because Android View.Invalidation is
  // handled by SynchronousCompositorHost.
  const bool is_root =
      surface_id.frame_sink_id() == root_frame_sink_->root_frame_sink_id();
  const bool is_root_renderer =
      root_frame_sink_->IsChildSurface(surface_id.frame_sink_id());
  const bool is_overlay = IsFrameSinkOverlayed(surface_id.frame_sink_id());

  if (!is_root && !is_root_renderer && !is_overlay &&
      damage_tracker_->CheckForDisplayDamage(surface_id)) {
    root_frame_sink_->OnNewUncommittedFrame(surface_id);
  }
}

void DisplaySchedulerWebView::OnCaptureStarted(
    const viz::FrameSinkId& frame_sink_id) {
  root_frame_sink_->OnCaptureStarted(frame_sink_id);
}

}  // namespace android_webview
