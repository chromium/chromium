// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "android_webview/browser/render_thread_manager.h"

#include <utility>

#include "android_webview/browser/compositor_frame_producer.h"
#include "android_webview/browser/compositor_id.h"
#include "android_webview/browser/deferred_gpu_command_service.h"
#include "android_webview/browser/hardware_renderer.h"
#include "android_webview/browser/render_thread_manager_client.h"
#include "android_webview/browser/scoped_app_gl_state_restore.h"
#include "android_webview/public/browser/draw_gl.h"
#include "base/bind.h"
#include "base/lazy_instance.h"
#include "base/location.h"
#include "base/memory/ptr_util.h"
#include "base/time/time.h"
#include "base/trace_event/trace_event.h"
#include "base/trace_event/traced_value.h"
#include "components/viz/common/quads/compositor_frame.h"

namespace android_webview {

namespace internal {

class RequestInvokeGLTracker {
 public:
  RequestInvokeGLTracker();
  bool ShouldRequestOnNonUiThread(RenderThreadManager* state);
  bool ShouldRequestOnUiThread(RenderThreadManager* state);
  void ResetPending();
  void SetQueuedFunctorOnUi(RenderThreadManager* state);

 private:
  base::Lock lock_;
  RenderThreadManager* pending_ui_;
  RenderThreadManager* pending_non_ui_;
};

RequestInvokeGLTracker::RequestInvokeGLTracker()
    : pending_ui_(NULL), pending_non_ui_(NULL) {}

bool RequestInvokeGLTracker::ShouldRequestOnNonUiThread(
    RenderThreadManager* state) {
  base::AutoLock lock(lock_);
  if (pending_ui_ || pending_non_ui_)
    return false;
  pending_non_ui_ = state;
  return true;
}

bool RequestInvokeGLTracker::ShouldRequestOnUiThread(
    RenderThreadManager* state) {
  base::AutoLock lock(lock_);
  if (pending_non_ui_) {
    pending_non_ui_->ResetRequestInvokeGLCallback();
    pending_non_ui_ = NULL;
  }
  // At this time, we could have already called RequestInvokeGL on the UI
  // thread,
  // but the corresponding GL mode process hasn't happened yet. In this case,
  // don't schedule another requestInvokeGL on the UI thread.
  if (pending_ui_)
    return false;
  pending_ui_ = state;
  return true;
}

void RequestInvokeGLTracker::ResetPending() {
  base::AutoLock lock(lock_);
  pending_non_ui_ = NULL;
  pending_ui_ = NULL;
}

void RequestInvokeGLTracker::SetQueuedFunctorOnUi(RenderThreadManager* state) {
  base::AutoLock lock(lock_);
  DCHECK(state);
  pending_ui_ = state;
  pending_non_ui_ = NULL;
}

}  // namespace internal

namespace {

base::LazyInstance<internal::RequestInvokeGLTracker>::DestructorAtExit
    g_request_invoke_gl_tracker = LAZY_INSTANCE_INITIALIZER;

constexpr base::TimeDelta kSlightlyMoreThanOneFrame =
    base::TimeDelta::FromMilliseconds(17);
}

RenderThreadManager::RenderThreadManager(
    RenderThreadManagerClient* client,
    const scoped_refptr<base::SingleThreadTaskRunner>& ui_loop)
    : ui_loop_(ui_loop),
      client_(client),
      compositor_frame_producer_(nullptr),
      has_received_frame_(false),
      renderer_manager_key_(GLViewRendererManager::GetInstance()->NullKey()),
      inside_hardware_release_(false),
      weak_factory_on_ui_thread_(this) {
  DCHECK(ui_loop_->BelongsToCurrentThread());
  DCHECK(client_);
  ui_thread_weak_ptr_ = weak_factory_on_ui_thread_.GetWeakPtr();
  ResetRequestInvokeGLCallback();
}

RenderThreadManager::~RenderThreadManager() {
  DCHECK(ui_loop_->BelongsToCurrentThread());
  if (compositor_frame_producer_) {
    compositor_frame_producer_->RemoveCompositorFrameConsumer(this);
  }
  DCHECK(!hardware_renderer_.get());
}

void RenderThreadManager::ClientRequestInvokeGL(bool for_idle) {
  if (ui_loop_->BelongsToCurrentThread()) {
    if (!g_request_invoke_gl_tracker.Get().ShouldRequestOnUiThread(this))
      return;
    ClientRequestInvokeGLOnUI();
  } else {
    if (!g_request_invoke_gl_tracker.Get().ShouldRequestOnNonUiThread(this))
      return;
    base::OnceClosure callback;
    {
      base::AutoLock lock(lock_);
      callback = request_draw_gl_closure_;
    }
    // 17ms is slightly longer than a frame, hoping that it will come
    // after the next frame so that the idle work is taken care of by
    // the next frame instead.
    ui_loop_->PostDelayedTask(
        FROM_HERE, std::move(callback),
        for_idle ? kSlightlyMoreThanOneFrame : base::TimeDelta());
  }
}

void RenderThreadManager::DidInvokeGLProcess() {
  g_request_invoke_gl_tracker.Get().ResetPending();
}

void RenderThreadManager::ResetRequestInvokeGLCallback() {
  DCHECK(ui_loop_->BelongsToCurrentThread());
  base::AutoLock lock(lock_);
  request_draw_gl_cancelable_closure_.Reset(base::BindRepeating(
      &RenderThreadManager::ClientRequestInvokeGLOnUI, base::Unretained(this)));
  request_draw_gl_closure_ = request_draw_gl_cancelable_closure_.callback();
}

void RenderThreadManager::ClientRequestInvokeGLOnUI() {
  DCHECK(ui_loop_->BelongsToCurrentThread());
  ResetRequestInvokeGLCallback();
  g_request_invoke_gl_tracker.Get().SetQueuedFunctorOnUi(this);
  if (!client_->RequestInvokeGL(false)) {
    g_request_invoke_gl_tracker.Get().ResetPending();
    LOG(ERROR) << "Failed to request GL process. Deadlock likely";
  }
}

void RenderThreadManager::UpdateParentDrawConstraintsOnUI() {
  DCHECK(ui_loop_->BelongsToCurrentThread());
  if (compositor_frame_producer_) {
    compositor_frame_producer_->OnParentDrawConstraintsUpdated(this);
  }
}

void RenderThreadManager::SetScrollOffsetOnUI(gfx::Vector2d scroll_offset) {
  base::AutoLock lock(lock_);
  scroll_offset_ = scroll_offset;
}

gfx::Vector2d RenderThreadManager::GetScrollOffsetOnRT() {
  base::AutoLock lock(lock_);
  return scroll_offset_;
}

std::unique_ptr<ChildFrame> RenderThreadManager::SetFrameOnUI(
    std::unique_ptr<ChildFrame> new_frame) {
  DCHECK(new_frame);
  has_received_frame_ = true;

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
  base::AutoLock lock(lock_);
  for (auto& frame_ptr : child_frames_)
    frame_ptr->WaitOnFutureIfNeeded();
  ChildFrameQueue returned_frames;
  returned_frames.swap(child_frames_);
  return returned_frames;
}

void RenderThreadManager::PostExternalDrawConstraintsToChildCompositorOnRT(
    const ParentCompositorDrawConstraints& parent_draw_constraints) {
  {
    base::AutoLock lock(lock_);
    parent_draw_constraints_ = parent_draw_constraints;
  }

  // No need to hold the lock_ during the post task.
  ui_loop_->PostTask(
      FROM_HERE,
      base::BindOnce(&RenderThreadManager::UpdateParentDrawConstraintsOnUI,
                     ui_thread_weak_ptr_));
}

ParentCompositorDrawConstraints
RenderThreadManager::GetParentDrawConstraintsOnUI() const {
  base::AutoLock lock(lock_);
  return parent_draw_constraints_;
}

void RenderThreadManager::SetInsideHardwareRelease(bool inside) {
  base::AutoLock lock(lock_);
  inside_hardware_release_ = inside;
}

bool RenderThreadManager::IsInsideHardwareRelease() const {
  base::AutoLock lock(lock_);
  return inside_hardware_release_;
}

RenderThreadManager::ReturnedResources::ReturnedResources()
    : layer_tree_frame_sink_id(0u) {}

RenderThreadManager::ReturnedResources::~ReturnedResources() {}

void RenderThreadManager::InsertReturnedResourcesOnRT(
    const std::vector<viz::ReturnedResource>& resources,
    const CompositorID& compositor_id,
    uint32_t layer_tree_frame_sink_id) {
  if (resources.empty())
    return;

  base::AutoLock lock(lock_);
  ReturnedResources& returned_resources =
      returned_resources_map_[compositor_id];
  if (returned_resources.layer_tree_frame_sink_id != layer_tree_frame_sink_id) {
    returned_resources.resources.clear();
  }
  returned_resources.resources.insert(returned_resources.resources.end(),
                                      resources.begin(), resources.end());
  returned_resources.layer_tree_frame_sink_id = layer_tree_frame_sink_id;

  if (!returned_resource_available_pending_) {
    returned_resource_available_pending_ = true;
    ui_loop_->PostDelayedTask(
        FROM_HERE,
        base::BindOnce(&RenderThreadManager::ReturnedResourceAvailableOnUI,
                       ui_thread_weak_ptr_),
        kSlightlyMoreThanOneFrame * 2);
  }
}

void RenderThreadManager::ReturnedResourceAvailableOnUI() {
  bool empty = false;
  {
    base::AutoLock lock(lock_);
    DCHECK(returned_resource_available_pending_);
    returned_resource_available_pending_ = false;
    empty = returned_resources_map_.empty();
  }
  if (!empty && compositor_frame_producer_) {
    compositor_frame_producer_->ReturnedResourceAvailable(this);
  }
}

void RenderThreadManager::SwapReturnedResourcesOnUI(
    ReturnedResourcesMap* returned_resource_map) {
  DCHECK(returned_resource_map->empty());
  base::AutoLock lock(lock_);
  returned_resource_map->swap(returned_resources_map_);
}

bool RenderThreadManager::ReturnedResourcesEmptyOnUI() const {
  base::AutoLock lock(lock_);
  return returned_resources_map_.empty();
}

void RenderThreadManager::DrawGL(AwDrawGLInfo* draw_info) {
  TRACE_EVENT0("android_webview,toplevel", "DrawFunctor");
  if (draw_info->mode == AwDrawGLInfo::kModeSync) {
    TRACE_EVENT_INSTANT0("android_webview", "kModeSync",
                         TRACE_EVENT_SCOPE_THREAD);
    if (hardware_renderer_)
      hardware_renderer_->CommitFrame();
    return;
  }

  // Force GL binding init if it's not yet initialized.
  DeferredGpuCommandService::GetInstance();

  // kModeProcessNoContext should never happen because we tear down hardware
  // in onTrimMemory. However that guarantee is maintained outside of chromium
  // code. Not notifying shared state in kModeProcessNoContext can lead to
  // immediate deadlock, which is slightly more catastrophic than leaks or
  // corruption.
  if (draw_info->mode == AwDrawGLInfo::kModeProcess ||
      draw_info->mode == AwDrawGLInfo::kModeProcessNoContext) {
    DidInvokeGLProcess();
  }

  {
    GLViewRendererManager* manager = GLViewRendererManager::GetInstance();
    base::AutoLock lock(lock_);
    if (renderer_manager_key_ != manager->NullKey()) {
      manager->DidDrawGL(renderer_manager_key_);
    }
  }

  ScopedAppGLStateRestore state_restore(
      draw_info->mode == AwDrawGLInfo::kModeDraw
          ? ScopedAppGLStateRestore::MODE_DRAW
          : ScopedAppGLStateRestore::MODE_RESOURCE_MANAGEMENT,
      draw_info->version < 3 /* save_restore */);
  ScopedAllowGL allow_gl;
  if (!hardware_renderer_ && draw_info->mode == AwDrawGLInfo::kModeDraw &&
      !IsInsideHardwareRelease() && HasFrameForHardwareRendererOnRT()) {
    hardware_renderer_.reset(new HardwareRenderer(this));
    hardware_renderer_->CommitFrame();
  }

  if (draw_info->mode == AwDrawGLInfo::kModeProcessNoContext) {
    LOG(ERROR) << "Received unexpected kModeProcessNoContext";
  }

  if (IsInsideHardwareRelease()) {
    hardware_renderer_.reset();
    // Flush the idle queue in tear down.
    DeferredGpuCommandService::GetInstance()->PerformAllIdleWork();
    return;
  }

  if (draw_info->mode != AwDrawGLInfo::kModeDraw) {
    if (draw_info->mode == AwDrawGLInfo::kModeProcess) {
      DeferredGpuCommandService::GetInstance()->PerformIdleWork(true);
    }
    return;
  }

  if (hardware_renderer_)
    hardware_renderer_->DrawGL(draw_info);
  DeferredGpuCommandService::GetInstance()->PerformIdleWork(false);
}

void RenderThreadManager::DeleteHardwareRendererOnUI() {
  DCHECK(ui_loop_->BelongsToCurrentThread());

  InsideHardwareReleaseReset auto_inside_hardware_release_reset(this);

  client_->DetachFunctorFromView();

  // If the WebView gets onTrimMemory >= MODERATE twice in a row, the 2nd
  // onTrimMemory will result in an unnecessary Render Thread InvokeGL call.
  if (has_received_frame_) {
    // Receiving at least one frame is a precondition for
    // initialization (such as looing up GL bindings and constructing
    // hardware_renderer_).
    bool draw_functor_succeeded = client_->RequestInvokeGL(true);
    if (!draw_functor_succeeded) {
      LOG(ERROR) << "Unable to free GL resources. Has the Window leaked?";
      // Calling release on wrong thread intentionally.
      AwDrawGLInfo info;
      info.mode = AwDrawGLInfo::kModeProcess;
      DrawGL(&info);
    }
  }

  GLViewRendererManager* manager = GLViewRendererManager::GetInstance();

  {
    base::AutoLock lock(lock_);
    if (renderer_manager_key_ != manager->NullKey()) {
      manager->Remove(renderer_manager_key_);
      renderer_manager_key_ = manager->NullKey();
    }
  }

  if (has_received_frame_) {
    // Flush any invoke functors that's caused by ReleaseHardware.
    client_->RequestInvokeGL(true);
  }

  has_received_frame_ = false;
}

void RenderThreadManager::SetCompositorFrameProducer(
    CompositorFrameProducer* compositor_frame_producer) {
  DCHECK(compositor_frame_producer == compositor_frame_producer_ ||
         compositor_frame_producer_ == nullptr ||
         compositor_frame_producer == nullptr);
  compositor_frame_producer_ = compositor_frame_producer;
}

bool RenderThreadManager::HasFrameForHardwareRendererOnRT() const {
  base::AutoLock lock(lock_);
  return !child_frames_.empty();
}

void RenderThreadManager::InitializeHardwareDrawIfNeededOnUI() {
  DCHECK(ui_loop_->BelongsToCurrentThread());
  GLViewRendererManager* manager = GLViewRendererManager::GetInstance();

  base::AutoLock lock(lock_);
  if (renderer_manager_key_ == manager->NullKey()) {
    renderer_manager_key_ = manager->PushBack(this);
  }
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
