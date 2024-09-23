// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "android_webview/browser/gfx/root_frame_sink.h"

#include "android_webview/browser/gfx/child_frame.h"
#include "android_webview/browser/gfx/display_scheduler_webview.h"
#include "android_webview/browser/gfx/viz_compositor_thread_runner_webview.h"
#include "base/containers/contains.h"
#include "base/memory/raw_ptr.h"
#include "base/trace_event/trace_event.h"
#include "components/viz/common/features.h"
#include "components/viz/common/surfaces/frame_sink_id_allocator.h"
#include "components/viz/service/frame_sinks/compositor_frame_sink_support.h"
#include "components/viz/service/frame_sinks/frame_sink_manager_impl.h"
#include "components/viz/service/surfaces/frame_index_constants.h"
#include "components/viz/service/surfaces/surface.h"

namespace android_webview {

namespace {

viz::FrameSinkId AllocateParentSinkId() {
  static viz::FrameSinkIdAllocator allocator(0u);
  return allocator.NextFrameSinkId();
}

}  // namespace

// Lifetime: WebView
// Instance owned by RootFrameSink
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
                    const viz::FrameTimingDetailsMap& feedbacks,
                    bool frame_ack,
                    std::vector<viz::ReturnedResource> resources) override {}
  void OnBeginFramePausedChanged(bool paused) override {}
  void ReclaimResources(std::vector<viz::ReturnedResource> resources) override {
    owner_->ReturnResources(frame_sink_id_, layer_tree_frame_sink_id_,
                            std::move(resources));
  }
  void OnCompositorFrameTransitionDirectiveProcessed(
      uint32_t sequence_id) override {
    owner_->OnCompositorFrameTransitionDirectiveProcessed(
        frame_sink_id_, layer_tree_frame_sink_id_, sequence_id);
  }
  void OnSurfaceEvicted(const viz::LocalSurfaceId& local_surface_id) override {}

  const viz::FrameSinkId frame_sink_id() { return frame_sink_id_; }

  uint32_t layer_tree_frame_sink_id() { return layer_tree_frame_sink_id_; }

  viz::CompositorFrameSinkSupport* support() { return support_.get(); }
  gfx::Size size() { return size_; }

  void SubmitCompositorFrame(
      const viz::LocalSurfaceId& local_surface_id,
      viz::CompositorFrame frame,
      std::optional<viz::HitTestRegionList> hit_test_region_list) {
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
    : root_frame_sink_id_(AllocateParentSinkId()),
      client_(client),
      use_new_invalidate_heuristic_(
          features::UseWebViewNewInvalidateHeuristic()) {
  constexpr bool is_root = true;
  GetFrameSinkManager()->RegisterFrameSinkId(root_frame_sink_id_,
                                             false /* report_activationa */);
  support_ = std::make_unique<viz::CompositorFrameSinkSupport>(
      this, GetFrameSinkManager(), root_frame_sink_id_, is_root);
  begin_frame_source_ = std::make_unique<viz::ExternalBeginFrameSource>(this);
  GetFrameSinkManager()->RegisterBeginFrameSource(begin_frame_source_.get(),
                                                  root_frame_sink_id_);

  // Note, that this technically not part of the new heuristic. Without this
  // line root CF will "request" BeginFrames for delivery of presentation
  // feedback that we don't care about which leads to more begin frame requested
  // than necessary. But to avoid any side effects on invalidation, fixing this
  // is gates under same feature flag.
  if (use_new_invalidate_heuristic_)
    support_->SetBeginFrameSource(nullptr);
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
  clients_need_begin_frames_ = needs_begin_frames;

  // Old heuristic doesn't need extra begin frames, so just forward client
  // needs.
  if (!use_new_invalidate_heuristic_) {
    UpdateNeedsBeginFrames(clients_need_begin_frames_);
    return;
  }

  // Make sure that we subscribed to BF if client needs them. We don't
  // unsubscribe from BF here to make sure that we invalidated for the latest
  // frames in necessary. We will stop observing them later in BeginFrame()
  // once we are done.
  if (clients_need_begin_frames_)
    UpdateNeedsBeginFrames(true);
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

void RootFrameSink::SetContainedSurfaces(
    const base::flat_set<viz::SurfaceId>& ids) {
  contained_surfaces_ = ids;
  for (auto it = last_invalidated_frame_index_.begin();
       it != last_invalidated_frame_index_.end();) {
    if (!contained_surfaces_.contains(it->first))
      it = last_invalidated_frame_index_.erase(it);
    else
      ++it;
  }
}

void RootFrameSink::UpdateNeedsBeginFrames(bool needs_begin_frames) {
  if (needs_begin_frames_ != needs_begin_frames) {
    needs_begin_frames_ = needs_begin_frames;
    if (client_)
      client_->SetNeedsBeginFrames(needs_begin_frames_);
  }
}

bool RootFrameSink::HasPendingDependency(const viz::SurfaceId& surface_id) {
  auto* surface =
      GetFrameSinkManager()->surface_manager()->GetSurfaceForId(surface_id);

  if (!surface || !surface->HasActiveFrame())
    return true;

  for (auto& range : surface->GetActiveFrame().metadata.referenced_surfaces) {
    if (HasPendingDependency(range.end()))
      return true;
  }
  return false;
}

bool RootFrameSink::ProcessVisibleSurfacesInvalidation() {
  if (!use_new_invalidate_heuristic_) {
    // This handles only invalidation of sub clients, root client invalidation
    // is handled by Invalidate() from cc to |SynchronousLayerTreeFrameSink|. So
    // we return false unless we already have damage.
    return needs_draw_;
  }

  bool invalidate = false;

  // There are few possible cases:
  // * viz::Surface is visible (i.e was embedded last frame and any scheduled
  // draws don't change that). In this case surface is in `contained_surfaces`
  // and we need to invalidate for any CompositorFrame that we haven't
  // invalidated yet. This is a steady state.
  // * viz::Surface is visible, but there are scheduled draws that remove it. In
  // this case surface is in `contained_surfaces`, but technically there is no
  // need to invalidate it. We can't know that it will disappear, so we
  // invalidate anyway.
  // * viz::Surface is visible, but has pending dependencies (embedded surfaces
  // without active frame). In this case surface is in `contained_surfaces`, but
  // the dependents aren't. Invalidate in this case pessimistically assuming
  // there are uncommitted frames that can be activated on commit in dependent
  // frames.
  // * viz::Surface is not visible yet, but there is a pending draw that will
  // embed it. In this case the surface is not in `contained_surfaces` yet, so
  // we can't process it here. After the draw will happen it's possible that
  // there are uncommitted frames that are already scheduled to draw, but have
  // not been processed here. This can cause extra invalidation.
  // * viz::Surface is not visible and there is no pending draw. This shouldn't
  // be possible because the only way to embed a child surface is for the root
  // renderer to submit a compositor frame and invalidation of it is handled
  // separately.

  // If there is pending dependency, invalidate.
  if (root_local_surface_id_allocator_.HasValidLocalSurfaceId()) {
    auto surface_id = viz::SurfaceId(
        root_frame_sink_id(),
        root_local_surface_id_allocator_.GetCurrentLocalSurfaceId());
    invalidate = invalidate || HasPendingDependency(surface_id);
  }

  for (auto& surface_id : contained_surfaces_) {
    auto* surface =
        GetFrameSinkManager()->surface_manager()->GetSurfaceForId(surface_id);
    if (surface) {
      // Track last frame_index that we invalidated for. Note, that this doesn't
      // take into account what current frame is or what display compositor will
      // draw. The intent here is to invalidate once for each CompositorFrame in
      // the Surface we see.
      auto& last_invalidated_index = last_invalidated_frame_index_[surface_id];
      auto uncommited_frame_index =
          last_invalidated_index > viz::kInvalidFrameIndex
              ? surface->GetUncommitedFrameIndexNewerThan(
                    last_invalidated_index)
              : surface->GetFirstUncommitedFrameIndex();
      if (uncommited_frame_index.has_value()) {
        invalidate = true;
        last_invalidated_index = uncommited_frame_index.value();
      }
    }
  }

  return invalidate;
}

bool RootFrameSink::BeginFrame(const viz::BeginFrameArgs& args,
                               bool had_input_event) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);

  // We call ProcessVisibleSurfacesInvalidation() to make sure heuristic updated
  // it's state (e.g last invalidated begin frame args).
  bool invalidate = ProcessVisibleSurfacesInvalidation() || had_input_event;

  TRACE_EVENT_INSTANT1("android_webview", "RootFrameSink::BeginFrame",
                       TRACE_EVENT_SCOPE_THREAD, "invalidate", invalidate);

  if (clients_need_begin_frames_) {
    begin_frame_source_->OnBeginFrame(args);
  } else if (!invalidate) {
    if (use_new_invalidate_heuristic_) {
      // Client don't need begin frames and we didn't invalidate, so we don't
      // need them either.
      UpdateNeedsBeginFrames(false);
    }
  }

  return invalidate;
}

void RootFrameSink::SetBeginFrameSourcePaused(bool paused) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  begin_frame_source_->OnSetBeginFrameSourcePaused(paused);
}

void RootFrameSink::SetNeedsDraw(bool needs_draw) {
  // Only old heuristic needs this.
  DCHECK(!use_new_invalidate_heuristic_);

  needs_draw_ = needs_draw;

  // It's possible that client submitted last frame and unsubscribed from
  // BeginFrames, but we haven't draw it yet.
  if (!needs_begin_frames_ && needs_draw) {
    if (client_)
      client_->Invalidate();
  }
}

void RootFrameSink::OnNewUncommittedFrame(const viz::SurfaceId& surface_id) {
  // Only new heurstic needs this.
  if (!use_new_invalidate_heuristic_)
    return;

  // If there is new uncommitted frame in the surface that affects display, make
  // sure we request a begin frame to check if we need to invalidate next frame.
  UpdateNeedsBeginFrames(true);
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

void RootFrameSink::OnCompositorFrameTransitionDirectiveProcessed(
    viz::FrameSinkId frame_sink_id,
    uint32_t layer_tree_frame_sink_id,
    uint32_t sequence_id) {
  if (client_) {
    client_->OnCompositorFrameTransitionDirectiveProcessed(
        frame_sink_id, layer_tree_frame_sink_id, sequence_id);
  }
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
    child_frame_renderer_thread_ids_ = {};
  }
  if (child_frame_renderer_thread_ids_ != child_frame->renderer_thread_ids) {
    child_frame_renderer_thread_ids_ = child_frame->renderer_thread_ids;
    // Thread IDs from a sandboxed renderer process, thus untrusted and
    // require verification.
    // child_frame_renderer_thread_ids_ are used only to avoid unnessary
    // reverification, they shouldn't be used a source of truth in
    // GetChildFrameRendererThreadIds.
    child_sink_support_->support()->SetThreadIds(
        /*from_untrusted_client=*/true, child_frame->renderer_thread_ids);
  }

  // Root renderer frame MUST be presented synchronously with UI, so we can't
  // delay activation. Note, it's not part of invalidation heuristic, but for
  // safety we update deadline only on the new path, on the old path there are
  // almost no embedded surfaces anyway.
  if (use_new_invalidate_heuristic_) {
    child_frame->frame->metadata.deadline = viz::FrameDeadline::MakeZero();
  }

  child_sink_support_->SubmitCompositorFrame(
      child_frame->local_surface_id, std::move(*child_frame->frame),
      std::move(child_frame->hit_test_region_list));
  child_frame->frame.reset();
}

viz::FrameTimingDetailsMap RootFrameSink::TakeChildFrameTimingDetailsMap() {
  // Take timing for root CompositorFrameSinkSupport to avoid them accumulating.
  // We don't use them anyhow.
  std::ignore = support_->TakeFrameTimingDetailsMap();

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

base::flat_set<base::PlatformThreadId>
RootFrameSink::GetChildFrameRendererThreadIds() {
  if (child_sink_support_) {
    return child_sink_support_->support()->GetThreadIds();
  }
  return {};
}

void RootFrameSink::EvictChildSurface(const viz::SurfaceId& surface_id) {
  DCHECK(child_sink_support_);
  child_sink_support_->EvictSurface(surface_id);
}

void RootFrameSink::OnCaptureStarted(const viz::FrameSinkId& frame_sink_id) {
  if (!base::Contains(contained_surfaces_, frame_sink_id,
                      &viz::SurfaceId::frame_sink_id)) {
    return;
  }
  // When a capture is started we need to force an invalidate.
  if (client_)
    client_->Invalidate();
}

void RootFrameSink::InvalidateForOverlays() {
  if (client_) {
    client_->Invalidate();
  }
}

}  // namespace android_webview
