// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ANDROID_WEBVIEW_BROWSER_GFX_RENDER_THREAD_MANAGER_H_
#define ANDROID_WEBVIEW_BROWSER_GFX_RENDER_THREAD_MANAGER_H_

#include <map>

#include "android_webview/browser/gfx/compositor_frame_consumer.h"
#include "android_webview/browser/gfx/hardware_renderer.h"
#include "android_webview/browser/gfx/parent_compositor_draw_constraints.h"
#include "android_webview/browser/gfx/root_frame_sink.h"
#include "base/logging.h"
#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "base/single_thread_task_runner.h"
#include "base/synchronization/lock.h"
#include "components/viz/common/surfaces/frame_sink_id.h"
#include "ui/gfx/geometry/vector2d.h"

namespace android_webview {

class ChildFrame;
class CompositorFrameProducer;

// This class is used to pass data between UI thread and RenderThread.
class RenderThreadManager : public CompositorFrameConsumer {
 public:
  explicit RenderThreadManager(
      const scoped_refptr<base::SingleThreadTaskRunner>& ui_loop);
  ~RenderThreadManager() override;

  // CompositorFrameConsumer methods.
  void SetCompositorFrameProducer(
      CompositorFrameProducer* compositor_frame_producer,
      RootFrameSinkGetter root_frame_sink_getter) override;
  void SetScrollOffsetOnUI(gfx::Vector2d scroll_offset) override;
  std::unique_ptr<ChildFrame> SetFrameOnUI(
      std::unique_ptr<ChildFrame> frame) override;
  void TakeParentDrawDataOnUI(ParentCompositorDrawConstraints* constraints,
                              viz::FrameSinkId* frame_sink_id,
                              viz::FrameTimingDetailsMap* timing_details,
                              uint32_t* frame_token) override;
  ChildFrameQueue PassUncommittedFrameOnUI() override;

  void RemoveFromCompositorFrameProducerOnUI();

  // Render thread methods.
  gfx::Vector2d GetScrollOffsetOnRT();
  ChildFrameQueue PassFramesOnRT();
  void PostParentDrawDataToChildCompositorOnRT(
      const ParentCompositorDrawConstraints& parent_draw_constraints,
      const viz::FrameSinkId& frame_sink_id,
      viz::FrameTimingDetailsMap timing_details,
      uint32_t frame_token);
  void InsertReturnedResourcesOnRT(
      const std::vector<viz::ReturnedResource>& resources,
      const viz::FrameSinkId& frame_sink_id,
      uint32_t layer_tree_frame_sink_id);

  void CommitFrameOnRT();
  void UpdateViewTreeForceDarkStateOnRT(bool view_tree_force_dark_state);
  void DrawOnRT(bool save_restore, HardwareRendererDrawParams* params);
  void DestroyHardwareRendererOnRT(bool save_restore);

  // May be created on either thread.
  class InsideHardwareReleaseReset {
   public:
    explicit InsideHardwareReleaseReset(
        RenderThreadManager* render_thread_manager);
    ~InsideHardwareReleaseReset();

   private:
    RenderThreadManager* render_thread_manager_;
  };

 private:
  static std::unique_ptr<ChildFrame> GetSynchronousCompositorFrame(
      scoped_refptr<content::SynchronousCompositor::FrameFuture> frame_future,
      std::unique_ptr<ChildFrame> child_frame);

  // RT thread method.
  bool HasFrameForHardwareRendererOnRT() const;
  bool IsInsideHardwareRelease() const;
  void SetInsideHardwareRelease(bool inside);

  // UI thread methods.
  void UpdateParentDrawConstraintsOnUI();
  void ViewTreeForceDarkStateChangedOnUI(bool view_tree_force_dark_state);
  void CheckUiCallsAllowed() const {
#if DCHECK_IS_ON()
    DCHECK(ui_calls_allowed_);
#endif  // DCHECK_IS_ON()
  }

  // Accessed by UI thread.
  scoped_refptr<base::SingleThreadTaskRunner> ui_loop_;
  base::WeakPtr<CompositorFrameProducer> producer_weak_ptr_;
  base::WeakPtr<RenderThreadManager> ui_thread_weak_ptr_;
#if DCHECK_IS_ON()
  // Becomes false after lifetime of this object transitions from being
  // shared by both UI and RT, to being owned by a single thread and only RT
  // methods and destructor are allowed after.
  bool ui_calls_allowed_ = true;
#endif  // DCHECK_IS_ON()

  // Accessed by RT thread.
  std::unique_ptr<HardwareRenderer> hardware_renderer_;
  bool view_tree_force_dark_state_ = false;

  // Accessed by both UI and RT thread.
  mutable base::Lock lock_;
  RootFrameSinkGetter root_frame_sink_getter_;
  gfx::Vector2d scroll_offset_;
  ChildFrameQueue child_frames_;
  bool mark_hardware_release_;
  ParentCompositorDrawConstraints parent_draw_constraints_;
  viz::FrameSinkId frame_sink_id_for_presentation_feedbacks_;
  viz::FrameTimingDetailsMap timing_details_;
  uint32_t presented_frame_token_ = 0u;

  base::WeakPtrFactory<RenderThreadManager> weak_factory_on_ui_thread_{this};

  DISALLOW_COPY_AND_ASSIGN(RenderThreadManager);
};

}  // namespace android_webview

#endif  // ANDROID_WEBVIEW_BROWSER_GFX_RENDER_THREAD_MANAGER_H_
