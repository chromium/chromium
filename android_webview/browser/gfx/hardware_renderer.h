// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ANDROID_WEBVIEW_BROWSER_GFX_HARDWARE_RENDERER_H_
#define ANDROID_WEBVIEW_BROWSER_GFX_HARDWARE_RENDERER_H_

#include <memory>

#include "android_webview/browser/gfx/child_frame.h"
#include "android_webview/browser/gfx/output_surface_provider_webview.h"
#include "android_webview/browser/gfx/root_frame_sink.h"
#include "base/memory/raw_ptr.h"
#include "base/threading/thread_checker.h"
#include "components/viz/common/surfaces/frame_sink_id.h"
#include "ui/gfx/android/android_surface_control_compat.h"
#include "ui/gfx/color_space.h"

namespace android_webview {

class AwVulkanContextProvider;
class RenderThreadManager;

// Lifetime: Temporary
struct OverlaysParams {
  enum class Mode {
    Disabled,
    Enabled,
  };

  typedef ASurfaceControl* (*GetSurfaceControlFn)();
  typedef void (*MergeTransactionFn)(ASurfaceTransaction*);

  Mode overlays_mode = Mode::Disabled;
  GetSurfaceControlFn get_surface_control = nullptr;
  MergeTransactionFn merge_transaction = nullptr;
};

// Lifetime: Temporary
struct HardwareRendererDrawParams {
  bool operator==(const HardwareRendererDrawParams& other) const;
  bool operator!=(const HardwareRendererDrawParams& other) const;

  int clip_left;
  int clip_top;
  int clip_right;
  int clip_bottom;
  int width;
  int height;
  float transform[16];
  gfx::ColorSpace color_space;
};

using ReportRenderingThreadsCallback =
    base::OnceCallback<void(const pid_t*, size_t)>;

// Lifetime: WebView
class HardwareRenderer {
 public:
  // Two rules:
  // 1) Never wait on |new_frame| on the UI thread, or in kModeSync. Otherwise
  //    this defeats the purpose of having a future.
  // 2) Never replace a non-empty frames with an empty frame.
  // The only way to do both is to hold up to two frames. This helper function
  // will wait on all frames in the queue, then only keep the last non-empty
  // frame and return the rest (non-empty) frames. It takes care to not drop
  // other data such as readback requests.
  // A common pattern for appending a new frame is:
  // * WaitAndPrune the existing frame, after which there is at most one frame
  //   is left in queue.
  // * Append new frame without waiting on it.
  static ChildFrameQueue WaitAndPruneFrameQueue(ChildFrameQueue* child_frames);

  HardwareRenderer(RenderThreadManager* state,
                   RootFrameSinkGetter root_frame_sink_getter,
                   AwVulkanContextProvider* context_provider);

  HardwareRenderer(const HardwareRenderer&) = delete;
  HardwareRenderer& operator=(const HardwareRenderer&) = delete;

  ~HardwareRenderer();

  void Draw(const HardwareRendererDrawParams& params,
            const OverlaysParams& overlays_params,
            ReportRenderingThreadsCallback report_rendering_threads_callback);
  void CommitFrame();
  void SetChildFrameForTesting(std::unique_ptr<ChildFrame> child_frame);
  void RemoveOverlays(OverlaysParams::MergeTransactionFn merge_transaction);
  void AbandonContext();

 private:
  class OnViz;

  void InitializeOnViz(RootFrameSinkGetter root_frame_sink_getter);
  bool IsUsingVulkan() const;
  bool IsUsingANGLEOverGL() const;
  void MergeTransactionIfNeeded(
      OverlaysParams::MergeTransactionFn merge_transaction);
  void ReturnChildFrame(std::unique_ptr<ChildFrame> child_frame);
  void ReturnResourcesToCompositor(std::vector<viz::ReturnedResource> resources,
                                   const viz::FrameSinkId& frame_sink_id,
                                   uint32_t layer_tree_frame_sink_id);

  void ReportDrawMetric(const HardwareRendererDrawParams& params);
  void DrawAndSwap(
      const HardwareRendererDrawParams& params,
      const OverlaysParams& overlays_params,
      ReportRenderingThreadsCallback report_rendering_threads_callback);
  void MarkAllowContextLoss();

  THREAD_CHECKER(render_thread_checker_);

  const raw_ptr<RenderThreadManager> render_thread_manager_;

  typedef void* EGLContext;

  // NOTE: This must be initialized before |output_surface_provider_|: this
  // field is expected to be initialized with the current EGL context just
  // *before* creation of this HardwareRenderer instance, whereas this
  // HardwareRenderer instance's creation of |output_surface_provider_| actually
  // *causes* an EGL context to be created.
  EGLContext last_egl_context_;

  ChildFrameQueue child_frame_queue_;

  // This holds the last ChildFrame received. Contains the frame info of the
  // last frame. The |frame| member is always null since frame has already
  // been submitted.
  std::unique_ptr<ChildFrame> child_frame_;
  // Used in metrics. Indicates if we invalidated/submitted for the ChildFrame
  // in |child_frame_|
  bool did_invalidate_ = false;
  bool did_submit_compositor_frame_ = false;

  // Information from UI on last commit.
  gfx::Point scroll_offset_;

  // HardwareRendererSingleThread guarantees resources are returned in the order
  // of layer_tree_frame_sink_id, and resources for old output surfaces are
  // dropped.
  uint32_t last_committed_layer_tree_frame_sink_id_ = 0u;

  // Draw params that was used in previous draw. Used in reporting draw metric.
  HardwareRendererDrawParams last_draw_params_ = {};

  // Information about last delegated frame.
  float device_scale_factor_ = 0;

  viz::SurfaceId surface_id_;

  // Used to create viz::OutputSurface and gl::GLSurface
  OutputSurfaceProviderWebView output_surface_provider_;

  // These are accessed on the viz thread.
  std::unique_ptr<OnViz> on_viz_;

  const bool report_rendering_threads_;

  base::TimeDelta preferred_frame_interval_;
};

}  // namespace android_webview

#endif  // ANDROID_WEBVIEW_BROWSER_GFX_HARDWARE_RENDERER_H_
