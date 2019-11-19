// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "android_webview/browser/gfx/render_thread_manager.h"

#include <utility>

#include "android_webview/browser/gfx/compositor_frame_producer.h"
#include "android_webview/browser/gfx/gpu_service_web_view.h"
#include "android_webview/browser/gfx/hardware_renderer_single_thread.h"
#include "android_webview/browser/gfx/hardware_renderer_viz.h"
#include "android_webview/browser/gfx/scoped_app_gl_state_restore.h"
#include "android_webview/browser/gfx/task_queue_web_view.h"
#include "android_webview/common/aw_features.h"
#include "android_webview/public/browser/draw_gl.h"
#include "base/bind.h"
#include "base/location.h"
#include "base/memory/ptr_util.h"
#include "base/time/time.h"
#include "base/trace_event/trace_event.h"
#include "base/trace_event/traced_value.h"
#include "components/viz/common/features.h"
#include "components/viz/common/quads/compositor_frame.h"

namespace android_webview {

RenderThreadManager::RenderThreadManager(
    const scoped_refptr<base::SingleThreadTaskRunner>& ui_loop)
    : ui_loop_(ui_loop), mark_hardware_release_(false) {
  DCHECK(ui_loop_->BelongsToCurrentThread());
  ui_thread_weak_ptr_ = weak_factory_on_ui_thread_.GetWeakPtr();
}

RenderThreadManager::~RenderThreadManager() {
  DCHECK(!hardware_renderer_.get());
  DCHECK(child_frames_.empty());
}

void RenderThreadManager::UpdateParentDrawConstraintsOnUI() {
  DCHECK(ui_loop_->BelongsToCurrentThread());
  CheckUiCallsAllowed();
  if (producer_weak_ptr_) {
    producer_weak_ptr_->OnParentDrawDataUpdated(this);
  }
}

void RenderThreadManager::ViewTreeForceDarkStateChangedOnUI(
    bool view_tree_force_dark_state) {
  DCHECK(ui_loop_->BelongsToCurrentThread());
  CheckUiCallsAllowed();
  if (producer_weak_ptr_) {
    producer_weak_ptr_->OnViewTreeForceDarkStateChanged(
        view_tree_force_dark_state);
  }
}

void RenderThreadManager::SetScrollOffsetOnUI(gfx::Vector2d scroll_offset) {
  DCHECK(ui_loop_->BelongsToCurrentThread());
  CheckUiCallsAllowed();
  base::AutoLock lock(lock_);
  scroll_offset_ = scroll_offset;
}

gfx::Vector2d RenderThreadManager::GetScrollOffsetOnRT() {
  base::AutoLock lock(lock_);
  return scroll_offset_;
}

std::unique_ptr<ChildFrame> RenderThreadManager::SetFrameOnUI(
    std::unique_ptr<ChildFrame> new_frame) {
  DCHECK(ui_loop_->BelongsToCurrentThread());
  CheckUiCallsAllowed();
  DCHECK(new_frame);

  base::AutoLock lock(lock_);
  if (child_frames_.empty()) {
    child_frames_.emplace_back(std::move(new_frame));
    return nullptr;
  }
  std::unique_ptr<ChildFrame> uncommitted_frame;
  DCHECK_LE(child_frames_.size(), 2u);
  ChildFrameQueue pruned_frames =
      HardwareRenderer::WaitAndPruneFrameQueue(&child_frames_);
  DCHECK_LE(pruned_frames.size(), 1u);
  if (pruned_frames.size())
    uncommitted_frame = std::move(pruned_frames.front());
  child_frames_.emplace_back(std::move(new_frame));
  return uncommitted_frame;
}

ChildFrameQueue RenderThreadManager::PassFramesOnRT() {
  base::AutoLock lock(lock_);
  ChildFrameQueue returned_frames;
  returned_frames.swap(child_frames_);
  return returned_frames;
}

ChildFrameQueue RenderThreadManager::PassUncommittedFrameOnUI() {
  DCHECK(ui_loop_->BelongsToCurrentThread());
  CheckUiCallsAllowed();
  base::AutoLock lock(lock_);
  for (auto& frame_ptr : child_frames_)
    frame_ptr->WaitOnFutureIfNeeded();
  ChildFrameQueue returned_frames;
  returned_frames.swap(child_frames_);
  return returned_frames;
}

void RenderThreadManager::PostParentDrawDataToChildCompositorOnRT(
    const ParentCompositorDrawConstraints& parent_draw_constraints,
    const viz::FrameSinkId& frame_sink_id,
    viz::FrameTimingDetailsMap timing_details,
    uint32_t frame_token) {
  {
    base::AutoLock lock(lock_);
    parent_draw_constraints_ = parent_draw_constraints;
    // FrameTimingDetails are a sequence and it's ok to drop something in
    // the middle of the sequence. This also means its ok to drop the details
    // from early returned frames from WaitAndPruneFrameQueue as well.
    timing_details_ = std::move(timing_details);
    presented_frame_token_ = frame_token;
    frame_sink_id_for_presentation_feedbacks_ = frame_sink_id;
  }

  // No need to hold the lock_ during the post task.
  ui_loop_->PostTask(
      FROM_HERE,
      base::BindOnce(&RenderThreadManager::UpdateParentDrawConstraintsOnUI,
                     ui_thread_weak_ptr_));
}

void RenderThreadManager::TakeParentDrawDataOnUI(
    ParentCompositorDrawConstraints* constraints,
    viz::FrameSinkId* frame_sink_id,
    viz::FrameTimingDetailsMap* timing_details,
    uint32_t* frame_token) {
  DCHECK(ui_loop_->BelongsToCurrentThread());
  DCHECK(timing_details->empty());
  CheckUiCallsAllowed();
  base::AutoLock lock(lock_);
  *constraints = parent_draw_constraints_;
  *frame_sink_id = frame_sink_id_for_presentation_feedbacks_;
  timing_details_.swap(*timing_details);
  *frame_token = presented_frame_token_;
}

void RenderThreadManager::SetInsideHardwareRelease(bool inside) {
  base::AutoLock lock(lock_);
  mark_hardware_release_ = inside;
}

bool RenderThreadManager::IsInsideHardwareRelease() const {
  base::AutoLock lock(lock_);
  return mark_hardware_release_;
}

void RenderThreadManager::InsertReturnedResourcesOnRT(
    const std::vector<viz::ReturnedResource>& resources,
    const viz::FrameSinkId& frame_sink_id,
    uint32_t layer_tree_frame_sink_id) {
  if (resources.empty())
    return;
  ui_loop_->PostTask(
      FROM_HERE, base::BindOnce(&CompositorFrameProducer::ReturnUsedResources,
                                producer_weak_ptr_, resources, frame_sink_id,
                                layer_tree_frame_sink_id));
}

void RenderThreadManager::CommitFrameOnRT() {
  if (hardware_renderer_)
    hardware_renderer_->CommitFrame();
}

void RenderThreadManager::UpdateViewTreeForceDarkStateOnRT(
    bool view_tree_force_dark_state) {
  if (view_tree_force_dark_state_ == view_tree_force_dark_state)
    return;
  view_tree_force_dark_state_ = view_tree_force_dark_state;
  ui_loop_->PostTask(
      FROM_HERE,
      base::BindOnce(&RenderThreadManager::ViewTreeForceDarkStateChangedOnUI,
                     ui_thread_weak_ptr_, view_tree_force_dark_state_));
}

void RenderThreadManager::DrawOnRT(bool save_restore,
                                   HardwareRendererDrawParams* params) {
  // Force GL binding init if it's not yet initialized.
  GpuServiceWebView::GetInstance();
  ScopedAppGLStateRestore state_restore(ScopedAppGLStateRestore::MODE_DRAW,
                                        save_restore);
  ScopedAllowGL allow_gl;
  if (!hardware_renderer_ && !IsInsideHardwareRelease() &&
      HasFrameForHardwareRendererOnRT()) {
    if (::features::IsUsingVizForWebView()) {
      RootFrameSinkGetter getter;
      {
        base::AutoLock lock(lock_);
        getter = root_frame_sink_getter_;
      }
      DCHECK(getter);
      hardware_renderer_.reset(
          new HardwareRendererViz(this, std::move(getter)));
    } else {
      hardware_renderer_.reset(new HardwareRendererSingleThread(this));
    }
    hardware_renderer_->CommitFrame();
  }

  if (hardware_renderer_)
    hardware_renderer_->Draw(params);
}

void RenderThreadManager::DestroyHardwareRendererOnRT(bool save_restore) {
  GpuServiceWebView::GetInstance();
  ScopedAppGLStateRestore state_restore(
      ScopedAppGLStateRestore::MODE_RESOURCE_MANAGEMENT, save_restore);
  ScopedAllowGL allow_gl;
  hardware_renderer_.reset();
}

void RenderThreadManager::RemoveFromCompositorFrameProducerOnUI() {
  DCHECK(ui_loop_->BelongsToCurrentThread());
  CheckUiCallsAllowed();
  if (producer_weak_ptr_)
    producer_weak_ptr_->RemoveCompositorFrameConsumer(this);
  weak_factory_on_ui_thread_.InvalidateWeakPtrs();
#if DCHECK_IS_ON()
  ui_calls_allowed_ = false;
#endif  // DCHECK_IS_ON()
}

void RenderThreadManager::SetCompositorFrameProducer(
    CompositorFrameProducer* compositor_frame_producer,
    RootFrameSinkGetter root_frame_sink_getter) {
  DCHECK(ui_loop_->BelongsToCurrentThread());
  CheckUiCallsAllowed();
  producer_weak_ptr_ = compositor_frame_producer->GetWeakPtr();

  base::AutoLock lock(lock_);
  root_frame_sink_getter_ = std::move(root_frame_sink_getter);
}

bool RenderThreadManager::HasFrameForHardwareRendererOnRT() const {
  base::AutoLock lock(lock_);
  return !child_frames_.empty();
}

RenderThreadManager::InsideHardwareReleaseReset::InsideHardwareReleaseReset(
    RenderThreadManager* render_thread_manager)
    : render_thread_manager_(render_thread_manager) {
  DCHECK(!render_thread_manager_->IsInsideHardwareRelease());
  render_thread_manager_->SetInsideHardwareRelease(true);
}

RenderThreadManager::InsideHardwareReleaseReset::~InsideHardwareReleaseReset() {
  render_thread_manager_->SetInsideHardwareRelease(false);
}

}  // namespace android_webview
