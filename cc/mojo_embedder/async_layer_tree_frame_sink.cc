// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/mojo_embedder/async_layer_tree_frame_sink.h"

#include <utility>

#include "base/functional/bind.h"
#include "base/metrics/histogram.h"
#include "base/metrics/histogram_functions.h"
#include "base/metrics/histogram_macros.h"
#include "base/task/current_thread.h"
#include "base/threading/platform_thread.h"
#include "base/trace_event/trace_event.h"
#include "base/trace_event/typed_macros.h"
#include "build/build_config.h"
#include "cc/base/histograms.h"
#include "cc/mojo_embedder/viz_layer_context.h"
#include "cc/trees/layer_tree_frame_sink_client.h"
#include "components/viz/common/features.h"
#include "components/viz/common/frame_sinks/begin_frame_args.h"
#include "components/viz/common/hit_test/hit_test_region_list.h"
#include "components/viz/common/quads/compositor_frame.h"

namespace cc {
namespace mojo_embedder {

namespace {
auto to_proto_enum(FrameSkippedReason reason) {
  using ProtoReason =
      ::perfetto::protos::pbzero::ChromeGraphicsPipeline::FrameSkippedReason;
  switch (reason) {
    case FrameSkippedReason::kRecoverLatency:
      return ProtoReason::SKIPPED_REASON_RECOVER_LATENCY;
    case FrameSkippedReason::kNoDamage:
      return ProtoReason::SKIPPED_REASON_NO_DAMAGE;
    case FrameSkippedReason::kWaitingOnMain:
      return ProtoReason::SKIPPED_REASON_WAITING_ON_MAIN;
    case FrameSkippedReason::kDrawThrottled:
      return ProtoReason::SKIPPED_REASON_DRAW_THROTTLED;
    default:
      return ProtoReason::SKIPPED_REASON_UNKNOWN;
  }
}
}  // namespace
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
    scoped_refptr<viz::RasterContextProvider> context_provider,
    scoped_refptr<RasterContextProviderWrapper> worker_context_provider_wrapper,
    scoped_refptr<gpu::ClientSharedImageInterface> shared_image_interface,
    InitParams* params)
    : LayerTreeFrameSink(std::move(context_provider),
                         std::move(worker_context_provider_wrapper),
                         std::move(params->compositor_task_runner),
                         params->gpu_memory_buffer_manager,
                         std::move(shared_image_interface)),
      use_direct_client_receiver_(params->use_direct_client_receiver),
      synthetic_begin_frame_source_(
          std::move(params->synthetic_begin_frame_source)),
#if BUILDFLAG(IS_ANDROID)
      io_thread_id_(params->io_thread_id),
      main_thread_id_(params->main_thread_id),
#endif
      pipes_(std::move(params->pipes)),
      wants_animate_only_begin_frames_(params->wants_animate_only_begin_frames),
      auto_needs_begin_frame_(params->auto_needs_begin_frame),
      wants_begin_frame_acks_(params->wants_begin_frame_acks),
      use_begin_frame_presentation_feedback_(
          params->use_begin_frame_presentation_feedback) {
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

  if (use_direct_client_receiver_ && base::CurrentIOThread::IsSet()) {
    auto& receiver = client_receiver_.emplace<DirectClientReceiver>(
        mojo::DirectReceiverKey{}, this);
    receiver.Bind(std::move(pipes_.client_receiver));
  } else {
    auto& receiver = client_receiver_.emplace<ClientReceiver>(this);
    receiver.Bind(std::move(pipes_.client_receiver), compositor_task_runner_);
  }

  if (synthetic_begin_frame_source_) {
    client->SetBeginFrameSource(synthetic_begin_frame_source_.get());
  } else {
    begin_frame_source_ = std::make_unique<viz::ExternalBeginFrameSource>(this);
    begin_frame_source_->OnSetBeginFrameSourcePaused(begin_frames_paused_);
    client->SetBeginFrameSource(begin_frame_source_.get());
  }

  if (wants_animate_only_begin_frames_) {
    compositor_frame_sink_->SetWantsAnimateOnlyBeginFrames();
  }
  if (wants_begin_frame_acks_) {
    compositor_frame_sink_ptr_->SetWantsBeginFrameAcks();
  }
  if (auto_needs_begin_frame_) {
    compositor_frame_sink_ptr_->SetAutoNeedsBeginFrame();
  }

  compositor_frame_sink_ptr_->InitializeCompositorFrameSinkType(
      viz::mojom::CompositorFrameSinkType::kLayerTree);

#if BUILDFLAG(IS_ANDROID)
  std::vector<int32_t> thread_ids;
  thread_ids.push_back(base::PlatformThread::CurrentId());
  if (io_thread_id_ != base::kInvalidThreadId)
    thread_ids.push_back(io_thread_id_);
  if (main_thread_id_ != base::kInvalidThreadId) {
    thread_ids.push_back(main_thread_id_);
  }
  compositor_frame_sink_ptr_->SetThreadIds(thread_ids);
#endif

  return true;
}

void AsyncLayerTreeFrameSink::DetachFromClient() {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  client_->SetBeginFrameSource(nullptr);
  begin_frame_source_.reset();
  synthetic_begin_frame_source_.reset();
  client_receiver_ = absl::monostate{};
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

  if (auto_needs_begin_frame_ && !needs_begin_frames_) {
    UpdateNeedsBeginFramesInternal(/*needs_begin_frames=*/true);
  }

  TRACE_EVENT(
      "viz,benchmark,graphics.pipeline", "Graphics.Pipeline",
      perfetto::Flow::Global(frame.metadata.begin_frame_ack.trace_id),
      [&](perfetto::EventContext ctx) {
        auto* event = ctx.event<perfetto::protos::pbzero::ChromeTrackEvent>();
        auto* data = event->set_chrome_graphics_pipeline();
        data->set_step(perfetto::protos::pbzero::ChromeGraphicsPipeline::
                           StepName::STEP_SUBMIT_COMPOSITOR_FRAME);
        local_surface_id_.WriteIntoTrace(
            ctx.Wrap(data->set_local_surface_id()));
        data->set_display_trace_id(frame.metadata.begin_frame_ack.trace_id);
      });
  if (local_surface_id_ == last_submitted_local_surface_id_) {
    DCHECK_EQ(last_submitted_device_scale_factor_, frame.device_scale_factor());
    DCHECK_EQ(last_submitted_size_in_pixels_.height(),
              frame.size_in_pixels().height());
    DCHECK_EQ(last_submitted_size_in_pixels_.width(),
              frame.size_in_pixels().width());
  }

  std::optional<viz::HitTestRegionList> hit_test_region_list =
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
      hit_test_region_list = std::nullopt;
    } else {
      last_hit_test_data_ = *hit_test_region_list;
    }
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

  compositor_frame_sink_ptr_->SubmitCompositorFrame(
      local_surface_id_, std::move(frame), std::move(hit_test_region_list), 0);
}

void AsyncLayerTreeFrameSink::DidNotProduceFrame(const viz::BeginFrameAck& ack,
                                                 FrameSkippedReason reason) {
  DCHECK(compositor_frame_sink_ptr_);
  DCHECK(!ack.has_damage);
  DCHECK(ack.frame_id.IsSequenceValid());
  TRACE_EVENT(
      "viz,benchmark,graphics.pipeline", "Graphics.Pipeline",
      perfetto::Flow::Global(ack.trace_id), [&](perfetto::EventContext ctx) {
        auto* event = ctx.event<perfetto::protos::pbzero::ChromeTrackEvent>();
        auto* data = event->set_chrome_graphics_pipeline();
        data->set_step(perfetto::protos::pbzero::ChromeGraphicsPipeline::
                           StepName::STEP_DID_NOT_PRODUCE_FRAME);
        data->set_frame_skipped_reason(to_proto_enum(reason));
        data->set_display_trace_id(ack.trace_id);
      });
  compositor_frame_sink_ptr_->DidNotProduceFrame(ack);
}

std::unique_ptr<LayerContext> AsyncLayerTreeFrameSink::CreateLayerContext(
    LayerTreeHostImpl& host_impl) {
  CHECK(compositor_frame_sink_ptr_);
  return std::make_unique<VizLayerContext>(*compositor_frame_sink_ptr_,
                                           host_impl);
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
    const viz::FrameTimingDetailsMap& timing_details,
    bool frame_ack,
    std::vector<viz::ReturnedResource> resources) {
  viz::BeginFrameArgs adjusted_args = args;
  adjusted_args.client_arrival_time = base::TimeTicks::Now();

  TRACE_EVENT(
      "viz,benchmark,graphics.pipeline", "Graphics.Pipeline",
      perfetto::Flow::Global(adjusted_args.trace_id),
      [&](perfetto::EventContext ctx) {
        auto* event = ctx.event<perfetto::protos::pbzero::ChromeTrackEvent>();
        auto* data = event->set_chrome_graphics_pipeline();
        data->set_step(needs_begin_frames_
                           ? perfetto::protos::pbzero::ChromeGraphicsPipeline::
                                 StepName::STEP_RECEIVE_BEGIN_FRAME
                           : perfetto::protos::pbzero::ChromeGraphicsPipeline::
                                 StepName::STEP_RECEIVE_BEGIN_FRAME_DISCARD);
        if (needs_begin_frames_) {
          data->set_frame_sequence(adjusted_args.frame_id.sequence_number);
        }
        data->set_display_trace_id(adjusted_args.trace_id);
      });

  if (features::IsOnBeginFrameAcksEnabled()) {
    if (frame_ack) {
      DidReceiveCompositorFrameAck(std::move(resources));
    } else if (!resources.empty()) {
      ReclaimResources(std::move(resources));
    }
  }

  for (const auto& pair : timing_details) {
    client_->DidPresentCompositorFrame(pair.first, pair.second);
    if (synthetic_begin_frame_source_ &&
        use_begin_frame_presentation_feedback_) {
      const auto& feedback = pair.second.presentation_feedback;
      synthetic_begin_frame_source_->OnUpdateVSyncParameters(feedback.timestamp,
                                                             feedback.interval);
    }
  }

  if (!needs_begin_frames_) {
    // We had a race with SetNeedsBeginFrame(false) and still need to let the
    // sink know that we didn't use this BeginFrame. OnBeginFrame() can also be
    // called to deliver presentation feedback.
    DidNotProduceFrame(viz::BeginFrameAck(adjusted_args, false),
                       FrameSkippedReason::kNoDamage);
    return;
  }

  if (begin_frame_source_)
    begin_frame_source_->OnBeginFrame(adjusted_args);
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

void AsyncLayerTreeFrameSink::OnSurfaceEvicted(
    const viz::LocalSurfaceId& local_surface_id) {
  client_->OnSurfaceEvicted(local_surface_id);
}

void AsyncLayerTreeFrameSink::OnNeedsBeginFrames(bool needs_begin_frames) {
  DCHECK(compositor_frame_sink_ptr_);

  // If `auto_needs_begin_frame_` is set to true, rely on unsolicited frames
  // instead of SetNeedsBeginFrame(true) to indicate that the client needs
  // BeginFrame requests.
  if (auto_needs_begin_frame_ && needs_begin_frames) {
    return;
  }

  UpdateNeedsBeginFramesInternal(needs_begin_frames);

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

void AsyncLayerTreeFrameSink::UpdateNeedsBeginFramesInternal(
    bool needs_begin_frames) {
  if (needs_begin_frames_ == needs_begin_frames) {
    return;
  }

  if (needs_begin_frames) {
    TRACE_EVENT_NESTABLE_ASYNC_BEGIN0("cc,benchmark", "NeedsBeginFrames", this);
  } else {
    TRACE_EVENT_NESTABLE_ASYNC_END0("cc,benchmark", "NeedsBeginFrames", this);
  }
  needs_begin_frames_ = needs_begin_frames;
}

}  // namespace mojo_embedder
}  // namespace cc
