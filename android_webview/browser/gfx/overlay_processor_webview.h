// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ANDROID_WEBVIEW_BROWSER_GFX_OVERLAY_PROCESSOR_WEBVIEW_H_
#define ANDROID_WEBVIEW_BROWSER_GFX_OVERLAY_PROCESSOR_WEBVIEW_H_

#include "android_webview/browser/gfx/display_scheduler_webview.h"
#include "base/synchronization/waitable_event.h"
#include "components/viz/common/quads/compositor_frame.h"
#include "components/viz/service/display/display_resource_provider.h"
#include "components/viz/service/display/overlay_processor_interface.h"
#include "components/viz/service/display/overlay_processor_surface_control.h"
#include "ui/gfx/android/android_surface_control_compat.h"

namespace gpu {
class DisplayCompositorMemoryAndTaskControllerOnGpu;
}

namespace android_webview {
class OverlayProcessorWebView : public viz::OverlayProcessorSurfaceControl {
 public:
  typedef ASurfaceControl* (*GetSurfaceControlFn)();
  class ScopedSurfaceControlAvailable {
   public:
    ScopedSurfaceControlAvailable(OverlayProcessorWebView* processor,
                                  GetSurfaceControlFn surface_getter);
    ~ScopedSurfaceControlAvailable();

   private:
    OverlayProcessorWebView* processor_;
  };

  OverlayProcessorWebView(
      viz::DisplayCompositorMemoryAndTaskController* display_controller);
  ~OverlayProcessorWebView() override;

  void SetOverlaysEnabledByHWUI(bool enabled);
  void RemoveOverlays();
  absl::optional<gfx::SurfaceControl::Transaction> TakeSurfaceTransactionOnRT();

  // viz::OverlayProcessorSurfaceControl overrides:
  void TakeOverlayCandidates(
      viz::OverlayCandidateList* candidate_list) override;
  void ScheduleOverlays(
      viz::DisplayResourceProvider* resource_provider) override;
  void AdjustOutputSurfaceOverlay(absl::optional<OutputSurfaceOverlayPlane>*
                                      output_surface_plane) override {}
  void CheckOverlaySupport(
      const viz::OverlayProcessorInterface::OutputSurfaceOverlayPlane*
          primary_plane,
      viz::OverlayCandidateList* candidates) override;

 private:
  class Manager;

  void CreateManagerOnRT(
      gpu::DisplayCompositorMemoryAndTaskControllerOnGpu* controller_on_gpu,
      gpu::CommandBufferId command_buffer_id,
      gpu::SequenceId sequence_id,
      base::WaitableEvent* event);

  // Overlay candidates for the current frame.
  viz::OverlayCandidateList overlay_candidates_;

  // Command buffer id for SyncTokens on RenderThread sequence.
  const gpu::CommandBufferId command_buffer_id_;

  gpu::GpuTaskSchedulerHelper* render_thread_sequence_;
  std::unique_ptr<gpu::SingleTaskSequence> gpu_thread_sequence_;

  viz::DisplayResourceProvider* resource_provider_ = nullptr;

  scoped_refptr<Manager> manager_;

  bool overlays_enabled_by_hwui_ = false;
};

}  // namespace android_webview

#endif  // ANDROID_WEBVIEW_BROWSER_GFX_OVERLAY_PROCESSOR_WEBVIEW_H_
