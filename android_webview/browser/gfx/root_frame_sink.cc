// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "android_webview/browser/gfx/root_frame_sink.h"

#include "android_webview/browser/gfx/child_frame.h"
#include "android_webview/browser/gfx/display_scheduler_webview.h"
#include "android_webview/browser/gfx/viz_compositor_thread_runner_webview.h"
#include "base/memory/raw_ptr.h"
#include "base/trace_event/trace_event.h"
#include "components/viz/common/surfaces/frame_sink_id_allocator.h"
#include "components/viz/service/frame_sinks/compositor_frame_sink_support.h"
#include "components/viz/service/frame_sinks/frame_sink_manager_impl.h"

namespace android_webview {

namespace {

viz::FrameSinkId AllocateParentSinkId() {
  static viz::FrameSinkIdAllocator allocator(0u);
  return allocator.NextFrameSinkId();
}

}  // namespace

class RootFrameSink::ChildCompositorFrameSink
    : public viz::mojom::CompositorFrameSinkClient {
 public:
  ChildCompositorFrameSink(RootFrameSink* owner,
                           uint32_t layer_tree_frame_sink_id,
                           viz::FrameSinkId frame_sink_id)
      : owner_(owner),
        layer_tree_frame_sink_id_(layer_tree_frame_sink_id),
        frame_sink_id_(frame_sink_id),
        support_(std::make_unique<viz::CompositorFrameSinkSupport>(
            this,
            owner->GetFrameSinkManager(),
            frame_sink_id,
            false)) {
    support_->SetBeginFrameSource(nullptr);
  }

  void DidReceiveCompositorFrameAck(
      std::vector<viz::ReturnedResource> resources) override {
    ReclaimResources(std::move(resources));
  }
  void OnBeginFrame(const viz::BeginFrameArgs& args,
                    const viz::FrameTimingDetailsMap& feedbacks) override {}
  void OnBeginFramePausedChanged(bool paused) override {}
  void ReclaimResources(std::vector<viz::ReturnedResource> resources) override {
    owner_->ReturnResources(frame_sink_id_, layer_tree_frame_sink_id_,
                            std::move(resources));
  }
  void OnCompositorFrameTransitionDirectiveProcessed(
      uint32_t sequence_id) override {}

  const viz::FrameSinkId frame_sink_id() { return frame_sink_id_; }

  uint32_t layer_tree_frame_sink_id() { return layer_tree_frame_sink_id_; }

  viz::CompositorFrameSinkSupport* support() { return support_.get(); }
  gfx::Size size() { return size_; }

  void SubmitCompositorFrame(
      const viz::LocalSurfaceId& local_surface_id,
      viz::CompositorFrame frame,
      absl::optional<viz::HitTestRegionList> hit_test_region_list) {
    size_ = frame.size_in_pixels();
    support()->SubmitCompositorFrame(local_surface_id, std::move(frame),
                                     std::move(hit_test_region_list));
  }

  void EvictSurface(viz::SurfaceId surface_id) {
    if (surface_id.frame_sink_id() == frame_sink_id_)
      support_->EvictSurface(surface_id.local_surface_id());
  }

 private:
  const raw_ptr<RootFrameSink> owner_;
  const uint32_t layer_tree_frame_sink_id_;
  const viz::FrameSinkId frame_sink_id_;
  std::unique_ptr<viz::CompositorFrameSinkSupport> support_;
  gfx::Size size_;
};

RootFrameSink::RootFrameSink(RootFrameSinkClient* client)
    : root_frame_sink_id_(AllocateParentSinkId()), client_(client) {
  constexpr bool is_root = true;
  GetFrameSinkManager()->RegisterFrameSinkId(root_frame_sink_id_,
                                             false /* report_activationa */);
  support_ = std::make_unique<viz::CompositorFrameSinkSupport>(
      this, GetFrameSinkManager(), root_frame_sink_id_, is_root);
  begin_frame_source_ = std::make_unique<viz::ExternalBeginFrameSource>(this);
  GetFrameSinkManager()->RegisterBeginFrameSource(begin_frame_source_.get(),
                                                  root_frame_sink_id_);
}

RootFrameSink::~RootFrameSink() {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  GetFrameSinkManager()->UnregisterBeginFrameSource(begin_frame_source_.get());
  begin_frame_source_.reset();
  support_.reset();
  GetFrameSinkManager()->InvalidateFrameSinkId(root_frame_sink_id_);
}

viz::FrameSinkManagerImpl* RootFrameSink::GetFrameSinkManager() {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  // FrameSinkManagerImpl is global and not owned by this class, which is
  // per-AwContents.
  return VizCompositorThreadRunnerWebView::GetInstance()->GetFrameSinkManager();
}

const viz::LocalSurfaceId& RootFrameSink::SubmitRootCompositorFrame(
    viz::CompositorFrame frame) {
  frame.metadata.frame_token = ++next_root_frame_token_;

  if (!root_local_surface_id_allocator_.HasValidLocalSurfaceId() ||
      root_surface_size_ != frame.size_in_pixels() ||
      root_device_scale_factor_ != frame.device_scale_factor()) {
    root_local_surface_id_allocator_.GenerateId();
    root_surface_size_ = frame.size_in_pixels();
    root_device_scale_factor_ = frame.device_scale_factor();
  }

  const auto& local_surface_id =
      root_local_surface_id_allocator_.GetCurrentLocalSurfaceId();
  support_->SubmitCompositorFrame(local_surface_id, std::move(frame));
  return local_surface_id;
}

void RootFrameSink::EvictRootSurface(
    const viz::LocalSurfaceId& local_surface_id) {
  const auto& current_local_surface_id =
      root_local_surface_id_allocator_.GetCurrentLocalSurfaceId();

  DLOG_IF(FATAL, !current_local_surface_id.IsSameOrNewerThan(local_surface_id))
      << "Evicting newer surface: " << local_surface_id.ToString()
      << " old: " << current_local_surface_id.ToString();
  if (current_local_surface_id == local_surface_id) {
    root_surface_size_ = gfx::Size();
    root_device_scale_factor_ = 0.0f;
  }
  support_->EvictSurface(local_surface_id);
}

void RootFrameSink::DidReceiveCompositorFrameAck(
    std::vector<viz::ReturnedResource> resources) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  ReclaimResources(std::move(resources));
}

void RootFrameSink::ReclaimResources(
    std::vector<viz::ReturnedResource> resources) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  // Root surface should have no resources to return.
  CHECK(resources.empty());
}

void RootFrameSink::OnNeedsBeginFrames(bool needs_begin_frames) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  TRACE_EVENT_INSTANT1("android_webview", "RootFrameSink::OnNeedsBeginFrames",
                       TRACE_EVENT_SCOPE_THREAD, "needs_begin_frames",
                       needs_begin_frames);
  needs_begin_frames_ = needs_begin_frames;
  if (client_)
    client_->SetNeedsBeginFrames(needs_begin_frames);
}

void RootFrameSink::AddChildFrameSinkId(const viz::FrameSinkId& frame_sink_id) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  child_frame_sink_ids_.insert(frame_sink_id);
  GetFrameSinkManager()->RegisterFrameSinkHierarchy(root_frame_sink_id_,
                                                    frame_sink_id);
}

void RootFrameSink::RemoveChildFrameSinkId(
    const viz::FrameSinkId& frame_sink_id) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  child_frame_sink_ids_.erase(frame_sink_id);
  GetFrameSinkManager()->UnregisterFrameSinkHierarchy(root_frame_sink_id_,
                                                      frame_sink_id);
}

bool RootFrameSink::BeginFrame(const viz::BeginFrameArgs& args,
                               bool had_input_event) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);

  // This handles only invalidation of sub clients, root client invalidation is
  // handled by Invalidate() from cc to |SynchronousLayerTreeFrameSink|. So we
  // return false unless we already have damage.
  bool invalidate = had_input_event || needs_draw_;

  TRACE_EVENT_INSTANT1("android_webview", "RootFrameSink::BeginFrame",
                       TRACE_EVENT_SCOPE_THREAD, "invalidate", invalidate);

  if (needs_begin_frames_) {
    begin_frame_source_->OnBeginFrame(args);
  }

  return invalidate;
}

void RootFrameSink::SetBeginFrameSourcePaused(bool paused) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  begin_frame_source_->OnSetBeginFrameSourcePaused(paused);
}

void RootFrameSink::SetNeedsDraw(bool needs_draw) {
  needs_draw_ = needs_draw;

  // It's possible that client submitted last frame and unsubscribed from
  // BeginFrames, but we haven't draw it yet.
  if (!needs_begin_frames_ && needs_draw) {
    if (client_)
      client_->Invalidate();
  }
}

bool RootFrameSink::IsChildSurface(const viz::FrameSinkId& frame_sink_id) {
  return child_frame_sink_ids_.contains(frame_sink_id);
}

void RootFrameSink::ReturnResources(
    viz::FrameSinkId frame_sink_id,
    uint32_t layer_tree_frame_sink_id,
    std::vector<viz::ReturnedResource> resources) {
  if (client_)
    client_->ReturnResources(frame_sink_id, layer_tree_frame_sink_id,
                             std::move(resources));
}

void RootFrameSink::DettachClient() {
  client_ = nullptr;
}

void RootFrameSink::SubmitChildCompositorFrame(ChildFrame* child_frame) {
  DCHECK(child_frame->frame);
  DCHECK(child_frame->local_surface_id.is_valid());
  if (!child_sink_support_ ||
      child_sink_support_->frame_sink_id() != child_frame->frame_sink_id ||
      child_sink_support_->layer_tree_frame_sink_id() !=
          child_frame->layer_tree_frame_sink_id) {
    child_sink_support_.reset();

    child_sink_support_ = std::make_unique<ChildCompositorFrameSink>(
        this, child_frame->layer_tree_frame_sink_id,
        child_frame->frame_sink_id);
  }

  child_sink_support_->SubmitCompositorFrame(
      child_frame->local_surface_id, std::move(*child_frame->frame),
      std::move(child_frame->hit_test_region_list));
  child_frame->frame.reset();
}

viz::FrameTimingDetailsMap RootFrameSink::TakeChildFrameTimingDetailsMap() {
  if (child_sink_support_)
    return child_sink_support_->support()->TakeFrameTimingDetailsMap();
  return viz::FrameTimingDetailsMap();
}

gfx::Size RootFrameSink::GetChildFrameSize() {
  // TODO(vasilyt): This is not going to work with VizFrameSubmissionForWebView.
  if (child_sink_support_) {
    return child_sink_support_->size();
  }
  return gfx::Size();
}

void RootFrameSink::EvictChildSurface(const viz::SurfaceId& surface_id) {
  DCHECK(child_sink_support_);
  child_sink_support_->EvictSurface(surface_id);
}

}  // namespace android_webview
