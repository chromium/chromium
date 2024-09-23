// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/frame_sink/frame_sink_holder.h"

#include <algorithm>
#include <memory>
#include <utility>

#include "ash/frame_sink/frame_sink_host.h"
#include "ash/frame_sink/ui_resource_manager.h"
#include "base/check.h"
#include "base/task/single_thread_task_runner.h"
#include "cc/scheduler/scheduler.h"
#include "cc/trees/layer_tree_frame_sink.h"
#include "components/viz/common/frame_sinks/begin_frame_source.h"
#include "components/viz/common/frame_timing_details.h"
#include "components/viz/common/hit_test/hit_test_region_list.h"
#include "components/viz/common/quads/compositor_frame.h"
#include "ui/aura/window.h"

namespace ash {
namespace {

constexpr int32_t kPauseBeginFrameThreshold = 5;

}  // namespace

FrameSinkHolder::FrameSinkHolder(
    std::unique_ptr<cc::LayerTreeFrameSink> frame_sink,
    const GetCompositorFrameCallback get_compositor_frame_callback,
    const OnFirstFrameRequestedCallback on_first_frame_requested_callback)
    : frame_sink_(std::move(frame_sink)),
      get_compositor_frame_callback_(std::move(get_compositor_frame_callback)),
      on_first_frame_requested_callback_(
          std::move(on_first_frame_requested_callback)) {
  frame_sink_->BindToClient(this);
}

FrameSinkHolder::~FrameSinkHolder() {
  if (frame_sink_) {
    frame_sink_->DetachFromClient();
  }
}

// static.
bool FrameSinkHolder::DeleteWhenLastResourceHasBeenReclaimed(
    std::unique_ptr<FrameSinkHolder> frame_sink_holder,
    aura::Window* host_window) {
  UiResourceManager& resource_manager = frame_sink_holder->resource_manager();
  if (frame_sink_holder->last_frame_size_in_pixels_.IsEmpty()) {
    // Delete sink holder immediately if no frame has been submitted.
    DCHECK(resource_manager.exported_resources_count() == 0);
    return true;
  }

  // Submit an empty frame to ensure that pending release callbacks will be
  // processed in a finite amount of time.
  frame_sink_holder->frame_sink_->SubmitCompositorFrame(
      frame_sink_holder->CreateEmptyFrame(),
      /*hit_test_data_changed=*/true);

  // Delete sink holder immediately if not waiting for exported resources to
  // be reclaimed.
  if (resource_manager.exported_resources_count() == 0) {
    return true;
  }

  // If we have exported resources to reclaim then extend the lifetime of
  // holder by deleting it later.
  DCHECK(host_window);
  aura::Window* root_window = host_window->GetRootWindow();

  // This can be null during shutdown.
  if (!root_window) {
    // Since we are in shutdown process, we will not be able to recover the
    // exported resources so just let the resources be marked as lost.
    resource_manager.LostExportedResources();
    return true;
  }

  // If we have exported resources to reclaim then extend the lifetime of
  // holder.
  frame_sink_holder.release()->SetRootWindowForDeletion(root_window);

  return false;
}

viz::CompositorFrame FrameSinkHolder::CreateEmptyFrame() {
  viz::CompositorFrame frame;
  frame.metadata.begin_frame_ack.frame_id =
      viz::BeginFrameId(viz::BeginFrameArgs::kManualSourceId,
                        viz::BeginFrameArgs::kStartingFrameNumber);
  frame.metadata.begin_frame_ack.has_damage = true;
  frame.metadata.device_scale_factor = last_frame_device_scale_factor_;
  frame.metadata.frame_token = ++compositor_frame_token_generator_;
  auto pass = viz::CompositorRenderPass::Create();
  pass->SetNew(viz::CompositorRenderPassId{1},
               gfx::Rect(last_frame_size_in_pixels_),
               gfx::Rect(last_frame_size_in_pixels_), gfx::Transform());
  frame.render_pass_list.push_back(std::move(pass));
  return frame;
}

void FrameSinkHolder::SetRootWindowForDeletion(aura::Window* root_window) {
  // The holder will delete itself when the root window is removed or when all
  // exported resources have been reclaimed.
  DCHECK(root_window);
  root_window_observation_.Observe(root_window);
}

void FrameSinkHolder::SetAutoUpdateMode(bool mode) {
  if (auto_update_ == mode) {
    return;
  }

  auto_update_ = mode;
  if (auto_update_) {
    ObserveBeginFrameSource(/*start=*/true);
  }
}

void FrameSinkHolder::SubmitCompositorFrame(bool synchronous_draw) {
  // We cannot request to submit a frame via `SubmitCompositorFrame()` if we are
  // in auto_update mode.
  DCHECK(!auto_update_);
  // Once the lifetime of FrameSinkHolder is extended, we should not submit new
  // frames since the `get_compositor_frame_callback_` can become null.
  DCHECK(!WaitingToScheduleDelete());

  if (delete_pending_ || auto_update_ || WaitingToScheduleDelete()) {
    return;
  }

  ObserveBeginFrameSource(/*start=*/true);

  // If we are already submitted a frame we cannot submit a new frame until we
  // get an acknowledgement from display compositor and we fall back to
  // asynchronous drawing.
  // Some FrameSinkHosts can request to submit a frame synchronously, even
  // before viz thread is fully enabled therefore we wait till display
  // compositor asks for the first frame therefore we fall to asynchronous
  // drawing till signaled.
  if (!synchronous_draw || pending_compositor_frame_ack_ ||
      !first_frame_requested_) {
    pending_compositor_frame_ = true;
    return;
  }

  std::unique_ptr<viz::CompositorFrame> frame =
      get_compositor_frame_callback_.Run(
          viz::BeginFrameAck::CreateManualAckWithDamage(), resources_manager_,
          auto_update_, last_frame_size_in_pixels_,
          last_frame_device_scale_factor_);

  if (!frame) {
    return;
  }

  SubmitCompositorFrameInternal(std::move(frame));
}

void FrameSinkHolder::SubmitCompositorFrameInternal(
    std::unique_ptr<viz::CompositorFrame> frame) {
  consecutive_begin_frames_produced_no_frame_count_ = 0;

  pending_compositor_frame_ = false;
  pending_compositor_frame_ack_ = true;

  last_frame_size_in_pixels_ = frame->size_in_pixels();
  last_frame_device_scale_factor_ = frame->metadata.device_scale_factor;

  frame->metadata.frame_token = ++compositor_frame_token_generator_;
  frame_sink_->SubmitCompositorFrame(std::move(*frame),
                                     /*hit_test_data_changed=*/true);
}

void FrameSinkHolder::OnBeginFrameSourcePausedChanged(bool paused) {}

bool FrameSinkHolder::OnBeginFrameDerivedImpl(const viz::BeginFrameArgs& args) {
  // Once the lifetime of FrameSinkHolder is extended, we should not submit new
  // frames asynchronously since the `get_compositor_frame_callback_` can become
  // null.
  if (WaitingToScheduleDelete()) {
    return false;
  }

  if (!first_frame_requested_) {
    first_frame_requested_ = true;
    on_first_frame_requested_callback_.Run();
  }

  viz::BeginFrameAck current_begin_frame_ack(args, false);

  if (pending_compositor_frame_ack_ ||
      !(pending_compositor_frame_ || auto_update_)) {
    const cc::FrameSkippedReason reason =
        pending_compositor_frame_ack_ ? cc::FrameSkippedReason::kWaitingOnMain
                                      : cc::FrameSkippedReason::kNoDamage;
    DidNotProduceFrame(std::move(current_begin_frame_ack), reason);
    return false;
  }

  std::unique_ptr<viz::CompositorFrame> frame =
      get_compositor_frame_callback_.Run(
          current_begin_frame_ack, resources_manager_, auto_update_,
          last_frame_size_in_pixels_, last_frame_device_scale_factor_);

  if (!frame) {
    // Failure to produce a frame is treated as if there was no damage.
    DidNotProduceFrame(std::move(current_begin_frame_ack),
                       cc::FrameSkippedReason::kNoDamage);
    return false;
  }

  SubmitCompositorFrameInternal(std::move(frame));

  return true;
}

void FrameSinkHolder::SetBeginFrameSource(viz::BeginFrameSource* source) {
  ObserveBeginFrameSource(/*start=*/false);
  begin_frame_source_ = source;
  ObserveBeginFrameSource(/*start=*/true);
}

void FrameSinkHolder::ObserveBeginFrameSource(bool start) {
  if (begin_frame_observation_.IsObserving() == start) {
    return;
  }

  if (begin_frame_source_) {
    consecutive_begin_frames_produced_no_frame_count_ = 0;
    if (start) {
      begin_frame_observation_.Observe(begin_frame_source_);
    } else {
      begin_frame_observation_.Reset();
    }
  }
}

void FrameSinkHolder::MaybeStopObservingBeingFrameSource() {
  if (!auto_update_ && consecutive_begin_frames_produced_no_frame_count_ >=
                           kPauseBeginFrameThreshold) {
    ObserveBeginFrameSource(/*start=*/false);
  }
}

void FrameSinkHolder::DidNotProduceFrame(viz::BeginFrameAck&& begin_frame_ack,
                                         cc::FrameSkippedReason reason) {
  frame_sink_->DidNotProduceFrame(begin_frame_ack, reason);
  ++consecutive_begin_frames_produced_no_frame_count_;
  MaybeStopObservingBeingFrameSource();
}

std::optional<viz::HitTestRegionList> FrameSinkHolder::BuildHitTestData() {
  return std::nullopt;
}

void FrameSinkHolder::ReclaimResources(
    std::vector<viz::ReturnedResource> resources) {
  if (delete_pending_) {
    return;
  }

  resource_manager().ReclaimResources(resources);

  if (WaitingToScheduleDelete() &&
      resource_manager().exported_resources_count() == 0) {
    ScheduleDelete();
  }
}

void FrameSinkHolder::SetTreeActivationCallback(
    base::RepeatingClosure callback) {}

void FrameSinkHolder::DidReceiveCompositorFrameAck() {
  pending_compositor_frame_ack_ = false;
}

void FrameSinkHolder::DidPresentCompositorFrame(
    uint32_t frame_token,
    const viz::FrameTimingDetails& details) {
  if (!presentation_callback_.is_null()) {
    presentation_callback_.Run(details.presentation_feedback);
  }
}

void FrameSinkHolder::DidLoseLayerTreeFrameSink() {
  resource_manager().LostExportedResources();
  if (WaitingToScheduleDelete()) {
    ScheduleDelete();
  }
}

void FrameSinkHolder::OnDraw(const gfx::Transform& transform,
                             const gfx::Rect& viewport,
                             bool resourceless_software_draw,
                             bool skip_draw) {}

void FrameSinkHolder::SetMemoryPolicy(const cc::ManagedMemoryPolicy& policy) {}

void FrameSinkHolder::SetExternalTilePriorityConstraints(
    const gfx::Rect& viewport_rect,
    const gfx::Transform& transform) {}

void FrameSinkHolder::OnWindowDestroying(aura::Window* window) {
  // Since we are destroying the root_window via which we were extending the
  // lifetime of the layer_sink_holder, after this point we cannot recover the
  // exported resources therefore just mark the exported resources as lost.
  resources_manager_.LostExportedResources();
  root_window_observation_.Reset();
  // Detaching client from `frame_sink_` ensures that display_compositor does
  // not call methods on `this` after we have scheduled the deletion of this
  // holder.
  frame_sink_->DetachFromClient();
  frame_sink_.reset();
  ScheduleDelete();
}

void FrameSinkHolder::ScheduleDelete() {
  if (delete_pending_) {
    return;
  }
  delete_pending_ = true;
  base::SingleThreadTaskRunner::GetCurrentDefault()->DeleteSoon(FROM_HERE,
                                                                this);
}

bool FrameSinkHolder::WaitingToScheduleDelete() const {
  // We only start observing the root window after calling
  // FrameSinkHolder::DeleteWhenLastResourceHasBeenReclaimed. An observing
  // root_window_observation_ means that we are waiting for all the exported
  // resources to be returned before we can delete `this` frame sink holder.
  return root_window_observation_.IsObserving();
}

}  // namespace ash
