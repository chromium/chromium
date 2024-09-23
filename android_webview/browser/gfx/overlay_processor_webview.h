// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ANDROID_WEBVIEW_BROWSER_GFX_OVERLAY_PROCESSOR_WEBVIEW_H_
#define ANDROID_WEBVIEW_BROWSER_GFX_OVERLAY_PROCESSOR_WEBVIEW_H_

#include "android_webview/browser/gfx/display_scheduler_webview.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/synchronization/waitable_event.h"
#include "base/threading/thread_checker.h"
#include "components/viz/common/quads/compositor_frame.h"
#include "components/viz/common/surfaces/frame_sink_id.h"
#include "components/viz/service/display/display_resource_provider_skia.h"
#include "components/viz/service/display/overlay_processor_interface.h"
#include "components/viz/service/display/overlay_processor_surface_control.h"
#include "components/viz/service/frame_sinks/frame_sink_observer.h"
#include "gpu/command_buffer/common/mailbox.h"
#include "gpu/command_buffer/common/sync_token.h"
#include "ui/gfx/android/android_surface_control_compat.h"

namespace viz {
class FrameSinkManagerImpl;
class ResolvedFrameData;
}  // namespace viz

namespace android_webview {
// Lifetime: WebView
class OverlayProcessorWebView : public viz::OverlayProcessorSurfaceControl,
                                public OverlaysInfoProvider {
 public:
  typedef ASurfaceControl* (*GetSurfaceControlFn)();
  class ScopedSurfaceControlAvailable {
   public:
    ScopedSurfaceControlAvailable(OverlayProcessorWebView* processor,
                                  GetSurfaceControlFn surface_getter);
    ~ScopedSurfaceControlAvailable();

   private:
    raw_ptr<OverlayProcessorWebView> processor_;
  };

  OverlayProcessorWebView(
      viz::DisplayCompositorMemoryAndTaskController* display_controller,
      viz::FrameSinkManagerImpl* frame_sink_manager);
  ~OverlayProcessorWebView() override;

  // returns false if it failed to update overlays.
  bool ProcessForFrameSinkId(const viz::FrameSinkId& frame_sink_id,
                             const viz::ResolvedFrameData* frame_data);
  void SetOverlaysEnabledByHWUI(bool enabled);
  void RemoveOverlays();
  std::optional<gfx::SurfaceControl::Transaction> TakeSurfaceTransactionOnRT();
  viz::SurfaceId GetOverlaySurfaceId(const viz::FrameSinkId& frame_sink_id);

  // viz::OverlayProcessorSurfaceControl overrides:
  void TakeOverlayCandidates(
      viz::OverlayCandidateList* candidate_list) override;
  void ScheduleOverlays(
      viz::DisplayResourceProvider* resource_provider) override;
  void AdjustOutputSurfaceOverlay(
      std::optional<OutputSurfaceOverlayPlane>* output_surface_plane) override {
  }
  void CheckOverlaySupportImpl(
      const viz::OverlayProcessorInterface::OutputSurfaceOverlayPlane*
          primary_plane,
      viz::OverlayCandidateList* candidates) override;

  // OverlaysInfoProvider implenentation:
  bool IsFrameSinkOverlayed(viz::FrameSinkId frame_sink_id) override;

 private:
  class Manager;

  struct Overlay {
    Overlay(uint64_t id, viz::ResourceId resource_id, int child_id);
    Overlay(const Overlay&);
    ~Overlay();

    uint64_t id;
    gpu::SyncToken create_sync_token;
    viz::ResourceId resource_id;
    int child_id;
    viz::SurfaceId surface_id;
  };

  struct LockResult {
    gpu::SyncToken sync_token;
    base::ScopedClosureRunner unlock_cb;
    gpu::Mailbox mailbox;
  };

  LockResult LockResource(Overlay& overlay);
  void ReturnResource(viz::ResourceId resource_id, viz::SurfaceId surface_id);

  void CreateManagerOnRT(
      gpu::CommandBufferId command_buffer_id,
      gpu::SequenceId sequence_id,
      base::WaitableEvent* event);

  void UpdateOverlayResource(viz::FrameSinkId frame_sink_id,
                             viz::ResourceId new_resource_id,
                             const gfx::RectF& uv_rect);

  using OverlayResourceLock =
      viz::DisplayResourceProviderSkia::ScopedExclusiveReadLockSharedImage;

  base::flat_map<viz::FrameSinkId, Overlay> overlays_;
  uint64_t next_overlay_id_ = 1;
  std::multimap<viz::ResourceId, OverlayResourceLock> locked_resources_;

  base::flat_map<viz::FrameSinkId, int> resource_lock_count_;

  // Overlay candidates for the current frame.
  viz::OverlayCandidateList overlay_candidates_;

  // Command buffer id for SyncTokens on RenderThread sequence.
  const gpu::CommandBufferId command_buffer_id_;
  uint64_t sync_fence_release_ = 0;

  raw_ptr<gpu::GpuTaskSchedulerHelper> render_thread_sequence_;
  std::unique_ptr<gpu::SingleTaskSequence> gpu_thread_sequence_;

  raw_ptr<viz::DisplayResourceProvider> resource_provider_ = nullptr;
  const raw_ptr<viz::FrameSinkManagerImpl> frame_sink_manager_;

  scoped_refptr<Manager> manager_;

  bool overlays_enabled_by_hwui_ = false;

  float frame_rate_ = 0.f;

  THREAD_CHECKER(thread_checker_);
  base::WeakPtrFactory<OverlayProcessorWebView> weak_ptr_factory_{this};
};

}  // namespace android_webview

#endif  // ANDROID_WEBVIEW_BROWSER_GFX_OVERLAY_PROCESSOR_WEBVIEW_H_
