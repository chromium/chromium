// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ANDROID_WEBVIEW_BROWSER_GFX_HARDWARE_RENDERER_H_
#define ANDROID_WEBVIEW_BROWSER_GFX_HARDWARE_RENDERER_H_

#include <memory>

#include "android_webview/browser/gfx/child_frame.h"
#include "android_webview/browser/gfx/output_surface_provider_webview.h"
#include "base/memory/raw_ptr.h"
#include "ui/gfx/android/android_surface_control_compat.h"
#include "ui/gfx/color_space.h"

namespace android_webview {

class RenderThreadManager;

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

  HardwareRenderer(const HardwareRenderer&) = delete;
  HardwareRenderer& operator=(const HardwareRenderer&) = delete;

  virtual ~HardwareRenderer();

  void Draw(const HardwareRendererDrawParams& params,
            const OverlaysParams& overlays_params);
  void CommitFrame();
  virtual void RemoveOverlays(
      OverlaysParams::MergeTransactionFn merge_transaction) = 0;
  virtual void AbandonContext() = 0;

  void SetChildFrameForTesting(std::unique_ptr<ChildFrame> child_frame);

 protected:
  explicit HardwareRenderer(RenderThreadManager* state);

  void ReturnChildFrame(std::unique_ptr<ChildFrame> child_frame);
  void ReturnResourcesToCompositor(std::vector<viz::ReturnedResource> resources,
                                   const viz::FrameSinkId& frame_sink_id,
                                   uint32_t layer_tree_frame_sink_id);

  void ReportDrawMetric(const HardwareRendererDrawParams& params);

  virtual void DrawAndSwap(const HardwareRendererDrawParams& params,
                           const OverlaysParams& overlays_params) = 0;

  const raw_ptr<RenderThreadManager> render_thread_manager_;

  typedef void* EGLContext;
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
};

}  // namespace android_webview

#endif  // ANDROID_WEBVIEW_BROWSER_GFX_HARDWARE_RENDERER_H_
