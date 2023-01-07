// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/mojo_embedder/async_layer_tree_frame_sink.h"

#include <utility>

#include "base/functional/bind.h"
#include "base/metrics/histogram.h"
#include "base/metrics/histogram_functions.h"
#include "base/metrics/histogram_macros.h"
#include "base/threading/platform_thread.h"
#include "base/trace_event/trace_event.h"
#include "build/build_config.h"
#include "cc/base/histograms.h"
#include "cc/trees/layer_tree_frame_sink_client.h"
#include "components/power_scheduler/power_mode.h"
#include "components/power_scheduler/power_mode_arbiter.h"
#include "components/power_scheduler/power_mode_voter.h"
#include "components/viz/common/features.h"
#include "components/viz/common/frame_sinks/begin_frame_args.h"
#include "components/viz/common/hit_test/hit_test_region_list.h"
#include "components/viz/common/quads/compositor_frame.h"

namespace cc {
namespace mojo_embedder {

AsyncLayerTreeFrameSink::InitParams::InitParams() = default;
AsyncLayerTreeFrameSink::InitParams::~InitParams() = default;

AsyncLayerTreeFrameSink::UnboundMessagePipes::UnboundMessagePipes() = default;
AsyncLayerTreeFrameSink::UnboundMessagePipes::~UnboundMessagePipes() = default;

bool AsyncLayerTreeFrameSink::UnboundMessagePipes::HasUnbound() const {
  return client_receiver.is_valid() &&
         (compositor_frame_sink_remote.is_valid() ^
          compositor_frame_sink_associated_remote.is_valid());
}

AsyncLayerTreeFrameSink::UnboundMessagePipes::UnboundMessagePipes(
    UnboundMessagePipes&& other) = default;

AsyncLayerTreeFrameSink::AsyncLayerTreeFrameSink(
    scoped_refptr<viz::ContextProvider> context_provider,
    scoped_refptr<RasterContextProviderWrapper> worker_context_provider_wrapper,
    InitParams* params)
    : LayerTreeFrameSink(std::move(context_provider),
                         std::move(worker_context_provider_wrapper),
                         std::move(params->compositor_task_runner),
                         params->gpu_memory_buffer_manager),
      synthetic_begin_frame_source_(
          std::move(params->synthetic_begin_frame_source)),
#if BUILDFLAG(IS_ANDROID)
      io_thread_id_(params->io_thread_id),
#endif
      pipes_(std::move(params->pipes)),
      wants_animate_only_begin_frames_(params->wants_animate_only_begin_frames),
      power_mode_voter_("PowerModeVoter.Animation") {
  DETACH_FROM_THREAD(thread_checker_);
}

AsyncLayerTreeFrameSink::~AsyncLayerTreeFrameSink() {}

bool AsyncLayerTreeFrameSink::BindToClient(LayerTreeFrameSinkClient* client) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);

  if (!LayerTreeFrameSink::BindToClient(client))
    return false;

  DCHECK(pipes_.HasUnbound());
  if (pipes_.compositor_frame_sink_remote.is_valid()) {
    compositor_frame_sink_.Bind(std::move(pipes_.compositor_frame_sink_remote));
    compositor_frame_sink_.set_disconnect_with_reason_handler(
        base::BindOnce(&AsyncLayerTreeFrameSink::OnMojoConnectionError,
                       weak_factory_.GetWeakPtr()));
    compositor_frame_sink_ptr_ = compositor_frame_sink_.get();
  } else if (pipes_.compositor_frame_sink_associated_remote.is_valid()) {
    compositor_frame_sink_associated_.Bind(
        std::move(pipes_.compositor_frame_sink_associated_remote));
    compositor_frame_sink_associated_.set_disconnect_with_reason_handler(
        base::BindOnce(&AsyncLayerTreeFrameSink::OnMojoConnectionError,
                       weak_factory_.GetWeakPtr()));
    compositor_frame_sink_ptr_ = compositor_frame_sink_associated_.get();
  }
  client_receiver_.Bind(std::move(pipes_.client_receiver),
                        compositor_task_runner_);

  if (synthetic_begin_frame_source_) {
    client->SetBeginFrameSource(synthetic_begin_frame_source_.get());
  } else {
    begin_frame_source_ = std::make_unique<viz::ExternalBeginFrameSource>(this);
    begin_frame_source_->OnSetBeginFrameSourcePaused(begin_frames_paused_);
    client->SetBeginFrameSource(begin_frame_source_.get());
  }

  if (wants_animate_only_begin_frames_)
    compositor_frame_sink_->SetWantsAnimateOnlyBeginFrames();

  compositor_frame_sink_ptr_->InitializeCompositorFrameSinkType(
      viz::mojom::CompositorFrameSinkType::kLayerTree);

#if BUILDFLAG(IS_ANDROID)
  std::vector<int32_t> thread_ids;
  thread_ids.push_back(base::PlatformThread::CurrentId());
  if (io_thread_id_ != base::kInvalidThreadId)
    thread_ids.push_back(io_thread_id_);
  compositor_frame_sink_ptr_->SetThreadIds(thread_ids);
#endif

  return true;
}

void AsyncLayerTreeFrameSink::DetachFromClient() {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  client_->SetBeginFrameSource(nullptr);
  begin_frame_source_.reset();
  synthetic_begin_frame_source_.reset();
  client_receiver_.reset();
  // `compositor_frame_sink_ptr_` points to either `compositor_frame_sink_` or
  // `compositor_frame_sink_associated_`, so it must be set to nullptr first.
  compositor_frame_sink_ptr_ = nullptr;
  compositor_frame_sink_.reset();
  compositor_frame_sink_associated_.reset();
  LayerTreeFrameSink::DetachFromClient();
}

void AsyncLayerTreeFrameSink::SetLocalSurfaceId(
    const viz::LocalSurfaceId& local_surface_id) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  DCHECK(local_surface_id.is_valid());
  local_surface_id_ = local_surface_id;
}

void AsyncLayerTreeFrameSink::SubmitCompositorFrame(
    viz::CompositorFrame frame,
    bool hit_test_data_changed) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  DCHECK(compositor_frame_sink_ptr_);
  DCHECK(frame.metadata.begin_frame_ack.has_damage);
  DCHECK(frame.metadata.begin_frame_ack.frame_id.IsSequenceValid());

  TRACE_EVENT_WITH_FLOW2(
      "viz,benchmark", "Graphics.Pipeline",
      TRACE_ID_GLOBAL(frame.metadata.begin_frame_ack.trace_id),
      TRACE_EVENT_FLAG_FLOW_IN | TRACE_EVENT_FLAG_FLOW_OUT, "step",
      "SubmitCompositorFrame", "local_surface_id",
      local_surface_id_.ToString());

  if (local_surface_id_ == last_submitted_local_surface_id_) {
    DCHECK_EQ(last_submitted_device_scale_factor_, frame.device_scale_factor());
    DCHECK_EQ(last_submitted_size_in_pixels_.height(),
              frame.size_in_pixels().height());
    DCHECK_EQ(last_submitted_size_in_pixels_.width(),
              frame.size_in_pixels().width());
  }

  absl::optional<viz::HitTestRegionList> hit_test_region_list =
      client_->BuildHitTestData();

  // If |hit_test_data_changed| was set or local_surface_id has been updated,
  // we always send hit-test data; otherwise we check for equality with the
  // last submitted hit-test data for possible optimization.
  if (!hit_test_region_list) {
    last_hit_test_data_ = viz::HitTestRegionList();
  } else if (!hit_test_data_changed &&
             local_surface_id_ == last_submitted_local_surface_id_) {
    if (viz::HitTestRegionList::IsEqual(*hit_test_region_list,
                                        last_hit_test_data_)) {
      DCHECK(!viz::HitTestRegionList::IsEqual(*hit_test_region_list,
                                              viz::HitTestRegionList()));
      hit_test_region_list = absl::nullopt;
    } else {
      last_hit_test_data_ = *hit_test_region_list;
    }

    UMA_HISTOGRAM_BOOLEAN("Event.VizHitTest.HitTestDataIsEqualAccuracy",
                          !hit_test_region_list);
  } else {
    last_hit_test_data_ = *hit_test_region_list;
  }

  if (last_submitted_local_surface_id_ != local_surface_id_) {
    last_submitted_local_surface_id_ = local_surface_id_;
    last_submitted_device_scale_factor_ = frame.device_scale_factor();
    last_submitted_size_in_pixels_ = frame.size_in_pixels();

    // These traces are split into two due to the incoming flow using
    // TRACE_ID_LOCAL, and the outgoing flow using TRACE_ID_GLOBAL. This is
    // needed to ensure the incoming flow is not messed up. The outgoing flow is
    // going to a different process.
    TRACE_EVENT_WITH_FLOW2(
        TRACE_DISABLED_BY_DEFAULT("viz.surface_id_flow"),
        "LocalSurfaceId.Submission.Flow",
        TRACE_ID_LOCAL(local_surface_id_.submission_trace_id()),
        TRACE_EVENT_FLAG_FLOW_IN, "step", "SubmitCompositorFrame", "surface_id",
        local_surface_id_.ToString());
    TRACE_EVENT_WITH_FLOW2(
        TRACE_DISABLED_BY_DEFAULT("viz.surface_id_flow"),
        "LocalSurfaceId.Submission.Flow",
        TRACE_ID_GLOBAL(local_surface_id_.submission_trace_id()),
        TRACE_EVENT_FLAG_FLOW_OUT, "step", "SubmitCompositorFrame",
        "surface_id", local_surface_id_.ToString());
  }

  // The trace_id is negated in order to keep the Graphics.Pipeline and
  // Event.Pipeline flows separated.
  const int64_t trace_id = ~frame.metadata.begin_frame_ack.trace_id;
  TRACE_EVENT_WITH_FLOW1(TRACE_DISABLED_BY_DEFAULT("viz.hit_testing_flow"),
                         "Event.Pipeline", TRACE_ID_GLOBAL(trace_id),
                         TRACE_EVENT_FLAG_FLOW_OUT, "step",
                         "SubmitHitTestData");

  power_mode_voter_.OnFrameProduced(frame.render_pass_list.back()->damage_rect,
                                    frame.device_scale_factor());

  compositor_frame_sink_ptr_->SubmitCompositorFrame(
      local_surface_id_, std::move(frame), std::move(hit_test_region_list), 0);
}

void AsyncLayerTreeFrameSink::DidNotProduceFrame(const viz::BeginFrameAck& ack,
                                                 FrameSkippedReason reason) {
  DCHECK(compositor_frame_sink_ptr_);
  DCHECK(!ack.has_damage);
  DCHECK(ack.frame_id.IsSequenceValid());
  TRACE_EVENT_WITH_FLOW2("viz,benchmark", "Graphics.Pipeline",
                         TRACE_ID_GLOBAL(ack.trace_id),
                         TRACE_EVENT_FLAG_FLOW_IN | TRACE_EVENT_FLAG_FLOW_OUT,
                         "step", "DidNotProduceFrame", "reason", reason);
  bool frame_completed = reason == FrameSkippedReason::kNoDamage;
  bool waiting_on_main = reason == FrameSkippedReason::kWaitingOnMain;
  power_mode_voter_.OnFrameSkipped(frame_completed, waiting_on_main);
  compositor_frame_sink_ptr_->DidNotProduceFrame(ack);
}

void AsyncLayerTreeFrameSink::DidAllocateSharedBitmap(
    base::ReadOnlySharedMemoryRegion region,
    const viz::SharedBitmapId& id) {
  DCHECK(compositor_frame_sink_ptr_);
  compositor_frame_sink_ptr_->DidAllocateSharedBitmap(std::move(region), id);
}

void AsyncLayerTreeFrameSink::DidDeleteSharedBitmap(
    const viz::SharedBitmapId& id) {
  DCHECK(compositor_frame_sink_ptr_);
  compositor_frame_sink_ptr_->DidDeleteSharedBitmap(id);
}

void AsyncLayerTreeFrameSink::DidReceiveCompositorFrameAck(
    std::vector<viz::ReturnedResource> resources) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  client_->ReclaimResources(std::move(resources));
  client_->DidReceiveCompositorFrameAck();
}

void AsyncLayerTreeFrameSink::OnBeginFrame(
    const viz::BeginFrameArgs& args,
    const viz::FrameTimingDetailsMap& timing_details) {
  for (const auto& pair : timing_details) {
    client_->DidPresentCompositorFrame(pair.first, pair.second);
  }

  if (!needs_begin_frames_) {
    TRACE_EVENT_WITH_FLOW1("viz,benchmark", "Graphics.Pipeline",
                           TRACE_ID_GLOBAL(args.trace_id),
                           TRACE_EVENT_FLAG_FLOW_IN | TRACE_EVENT_FLAG_FLOW_OUT,
                           "step", "ReceiveBeginFrameDiscard");
    // We had a race with SetNeedsBeginFrame(false) and still need to let the
    // sink know that we didn't use this BeginFrame. OnBeginFrame() can also be
    // called to deliver presentation feedback.
    DidNotProduceFrame(viz::BeginFrameAck(args, false),
                       FrameSkippedReason::kNoDamage);
    return;
  }
  TRACE_EVENT_WITH_FLOW1("viz,benchmark", "Graphics.Pipeline",
                         TRACE_ID_GLOBAL(args.trace_id),
                         TRACE_EVENT_FLAG_FLOW_IN | TRACE_EVENT_FLAG_FLOW_OUT,
                         "step", "ReceiveBeginFrame");

  if (begin_frame_source_)
    begin_frame_source_->OnBeginFrame(args);
}

void AsyncLayerTreeFrameSink::OnBeginFramePausedChanged(bool paused) {
  begin_frames_paused_ = paused;
  if (begin_frame_source_)
    begin_frame_source_->OnSetBeginFrameSourcePaused(paused);
}

void AsyncLayerTreeFrameSink::ReclaimResources(
    std::vector<viz::ReturnedResource> resources) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  client_->ReclaimResources(std::move(resources));
}

void AsyncLayerTreeFrameSink::OnCompositorFrameTransitionDirectiveProcessed(
    uint32_t sequence_id) {
  client_->OnCompositorFrameTransitionDirectiveProcessed(sequence_id);
}

void AsyncLayerTreeFrameSink::OnNeedsBeginFrames(bool needs_begin_frames) {
  DCHECK(compositor_frame_sink_ptr_);
  if (needs_begin_frames_ != needs_begin_frames) {
    if (needs_begin_frames) {
      TRACE_EVENT_NESTABLE_ASYNC_BEGIN0("cc,benchmark", "NeedsBeginFrames",
                                        this);
    } else {
      TRACE_EVENT_NESTABLE_ASYNC_END0("cc,benchmark", "NeedsBeginFrames", this);
    }
    power_mode_voter_.OnNeedsBeginFramesChanged(needs_begin_frames);
  }
  needs_begin_frames_ = needs_begin_frames;
  compositor_frame_sink_ptr_->SetNeedsBeginFrame(needs_begin_frames);
}

void AsyncLayerTreeFrameSink::OnMojoConnectionError(
    uint32_t custom_reason,
    const std::string& description) {
  // TODO(rivr): Use DLOG(FATAL) once crbug.com/1043899 is resolved.
  if (custom_reason)
    DLOG(ERROR) << description;
  if (client_)
    client_->DidLoseLayerTreeFrameSink();
}

}  // namespace mojo_embedder
}  // namespace cc
