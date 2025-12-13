// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/mojo_embedder/async_layer_tree_frame_sink.h"

#include <utility>
#include <variant>

#include "base/functional/bind.h"
#include "base/metrics/histogram.h"
#include "base/metrics/histogram_functions.h"
#include "base/metrics/histogram_macros.h"
#include "base/task/common/task_annotator.h"
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
#include "components/viz/common/gpu/raster_context_provider.h"
#include "components/viz/common/hit_test/hit_test_region_list.h"
#include "components/viz/common/quads/compositor_frame.h"
#include "services/viz/public/mojom/compositing/thread.mojom.h"
#include "third_party/perfetto/include/perfetto/tracing/track.h"

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
    scoped_refptr<viz::RasterContextProvider> worker_context_provider,
    scoped_refptr<gpu::SharedImageInterface> shared_image_interface,
    InitParams* params)
    : LayerTreeFrameSink(std::move(context_provider),
                         std::move(worker_context_provider),
                         std::move(params->compositor_task_runner),
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
      no_compositor_frame_acks_(params->no_compositor_frame_acks),
      manual_begin_frame_(params->manual_begin_frame),
      use_begin_frame_presentation_feedback_(
          params->use_begin_frame_presentation_feedback),
      num_did_not_produce_frame_before_internal_begin_frame_source_(
          params
              ->num_did_not_produce_frame_before_internal_begin_frame_source) {
  DETACH_FROM_THREAD(thread_checker_);
  CHECK(manual_begin_frame_ && auto_needs_begin_frame_ || !manual_begin_frame_);
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

  if (wants_animate_only_begin_frames_ || auto_needs_begin_frame_ ||
      no_compositor_frame_acks_) {
    auto params = viz::mojom::CompositorFrameSinkParams::New();
    params->wants_animate_only_begin_frames = wants_animate_only_begin_frames_;
    params->auto_needs_begin_frame = auto_needs_begin_frame_;
    params->no_compositor_frame_acks = no_compositor_frame_acks_;
    compositor_frame_sink_ptr_->SetParams(std::move(params));
  }
  if (num_did_not_produce_frame_before_internal_begin_frame_source_) {
    DCHECK(auto_needs_begin_frame_);
    internal_begin_frame_source_ =
        std::make_unique<viz::DelayBasedBeginFrameSource>(
            std::make_unique<viz::DelayBasedTimeSource>(
                compositor_task_runner_.get()),
            viz::BeginFrameSource::kNotRestartableId);
  }

#if BUILDFLAG(IS_ANDROID)
  std::vector<viz::Thread> threads;
  threads.push_back(
      {base::PlatformThread::CurrentId(), viz::Thread::Type::kCompositor});
  if (io_thread_id_ != base::kInvalidThreadId)
    threads.push_back({io_thread_id_, viz::Thread::Type::kIO});
  if (main_thread_id_ != base::kInvalidThreadId) {
    threads.push_back({main_thread_id_, viz::Thread::Type::kMain});
  }
  compositor_frame_sink_ptr_->SetThreads(threads);
#endif

  return true;
}

void AsyncLayerTreeFrameSink::DetachFromClient() {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  client_->SetBeginFrameSource(nullptr);
  begin_frame_source_.reset();
  synthetic_begin_frame_source_.reset();
  internal_begin_frame_source_.reset();
  num_did_not_produce_frame_since_last_submit_ = 0;
  client_receiver_ = std::monostate{};
  // `compositor_frame_sink_ptr_` points to either `compositor_frame_sink_` or
  // `compositor_frame_sink_associated_`, so it must be set to nullptr first.
  compositor_frame_sink_ptr_ = nullptr;
  compositor_frame_sink_.reset();
  compositor_frame_sink_associated_.reset();
  LayerTreeFrameSink::DetachFromClient();
  weak_factory_.InvalidateWeakPtrs();
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

  // TODO(crbug.com/411268742): there're test failures if we update
  // needs_begin_frames_ when
  // num_did_not_produce_frame_before_internal_begin_frame_source_ is set.
  if (auto_needs_begin_frame_ && !needs_begin_frames_ &&
      !num_did_not_produce_frame_before_internal_begin_frame_source_ &&
      !manual_begin_frame_) {
    UpdateNeedsBeginFramesInternal(/*needs_begin_frames=*/true);
  }

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
  const int64_t trace_id = frame.metadata.begin_frame_ack.trace_id;
  const int64_t negated_trace_id = ~trace_id;
  TRACE_EVENT_WITH_FLOW1(TRACE_DISABLED_BY_DEFAULT("viz.hit_testing_flow"),
                         "Event.Pipeline", TRACE_ID_GLOBAL(negated_trace_id),
                         TRACE_EVENT_FLAG_FLOW_OUT, "step",
                         "SubmitHitTestData");

  TRACE_EVENT(
      "graphics.pipeline", "Graphics.Pipeline",
      perfetto::Flow::Global(trace_id), [&](perfetto::EventContext ctx) {
        auto* event = ctx.event<perfetto::protos::pbzero::ChromeTrackEvent>();
        auto* data = event->set_chrome_graphics_pipeline();
        data->set_step(
            perfetto::protos::pbzero::ChromeGraphicsPipeline::StepName::
                STEP_SEND_SUBMIT_COMPOSITOR_FRAME_MOJO_MESSAGE);
        data->set_surface_frame_trace_id(trace_id);
      });

  if (internal_begin_frame_source_ &&
      frame.metadata.begin_frame_ack.frame_id.source_id ==
          internal_begin_frame_source_->source_id()) {
    // If the frame is from internal begin frame source, use kManualSourceId.
    frame.metadata.begin_frame_ack.frame_id.source_id =
        viz::BeginFrameArgs::kManualSourceId;
  }
  compositor_frame_sink_ptr_->SubmitCompositorFrame(
      local_surface_id_, std::move(frame), std::move(hit_test_region_list), 0);

  ExportFrameTiming();

  num_did_not_produce_frame_since_last_submit_ = 0;
  if (use_internal_begin_frame_source_) {
    // Stop using internal begin frame source after DidFinishFrame.
    compositor_task_runner_->PostTask(
        FROM_HERE,
        base::BindOnce(&AsyncLayerTreeFrameSink::UpdateInternalBeginFrameSource,
                       weak_factory_.GetWeakPtr(), false));
  }
}

void AsyncLayerTreeFrameSink::DidNotProduceFrame(const viz::BeginFrameAck& ack,
                                                 FrameSkippedReason reason) {
  DCHECK(compositor_frame_sink_ptr_);
  DCHECK(!ack.has_damage);
  DCHECK(ack.frame_id.IsSequenceValid());
  TRACE_EVENT(
      "viz,benchmark,graphics.pipeline", "Graphics.Pipeline",
      perfetto::Flow::Global(ack.trace_id), [&](perfetto::EventContext ctx) {
        base::TaskAnnotator::EmitTaskTimingDetails(ctx);
        auto* event = ctx.event<perfetto::protos::pbzero::ChromeTrackEvent>();
        auto* data = event->set_chrome_graphics_pipeline();
        data->set_step(perfetto::protos::pbzero::ChromeGraphicsPipeline::
                           StepName::STEP_DID_NOT_PRODUCE_COMPOSITOR_FRAME);
        data->set_frame_skipped_reason(to_proto_enum(reason));
        data->set_surface_frame_trace_id(ack.trace_id);
      });

  ExportFrameTiming();

  // TODO(crbug.com/40900977): Once we validate
  // `features::kInternalBeginFrameSourceOnManyDidNotProduceFrame` we can use
  // the `internal_begin_frame_source_` for all begin frames until we actually
  // produce damage.
  if (auto_needs_begin_frame_ &&
      ack.frame_id.source_id == viz::BeginFrameArgs::kManualSourceId) {
    compositor_frame_sink_ptr_->SetNeedsBeginFrame(needs_begin_frames_);
    return;
  }

  if (use_internal_begin_frame_source_) {
    return;
  }
  compositor_frame_sink_ptr_->DidNotProduceFrame(ack);

  if (num_did_not_produce_frame_before_internal_begin_frame_source_ &&
      ++num_did_not_produce_frame_since_last_submit_ >
          num_did_not_produce_frame_before_internal_begin_frame_source_) {
    // Start internal begin frame source after this DidFinishFrame.
    compositor_task_runner_->PostTask(
        FROM_HERE,
        base::BindOnce(&AsyncLayerTreeFrameSink::UpdateInternalBeginFrameSource,
                       weak_factory_.GetWeakPtr(), true));
  }
}

void AsyncLayerTreeFrameSink::ExportFrameTiming() {
  for (const auto& pair : timing_details_) {
    client_->DidPresentCompositorFrame(pair.first, pair.second);
  }
  timing_details_.clear();
}

std::unique_ptr<LayerContext> AsyncLayerTreeFrameSink::CreateLayerContext(
    LayerTreeHostImpl& host_impl) {
  CHECK(compositor_frame_sink_ptr_);
  return std::make_unique<VizLayerContext>(*compositor_frame_sink_ptr_,
                                           host_impl);
}

void AsyncLayerTreeFrameSink::DidReceiveCompositorFrameAck(
    std::vector<viz::ReturnedResource> resources) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  client_->ReclaimResources(std::move(resources));
  if (!no_compositor_frame_acks_ &&
      !base::FeatureList::IsEnabled(features::kNoCompositorFrameAcks)) {
    client_->DidReceiveCompositorFrameAck();
  }
}

void AsyncLayerTreeFrameSink::OnBeginFrame(
    const viz::BeginFrameArgs& args,
    const viz::FrameTimingDetailsMap& timing_details,
    std::vector<viz::ReturnedResource> resources) {
  viz::BeginFrameArgs adjusted_args = args;
  adjusted_args.client_arrival_time = base::TimeTicks::Now();

  TRACE_EVENT(
      "viz,benchmark,graphics.pipeline", "Graphics.Pipeline",
      perfetto::Flow::Global(adjusted_args.trace_id),
      [&](perfetto::EventContext ctx) {
        base::TaskAnnotator::EmitTaskTimingDetails(ctx);
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
        data->set_surface_frame_trace_id(adjusted_args.trace_id);
      });

  if (!resources.empty()) {
    ReclaimResources(std::move(resources));
  }

  timing_details_.insert(timing_details.begin(), timing_details.end());

  for (const auto& pair : timing_details) {
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

  if (internal_begin_frame_source_ &&
      internal_begin_frame_source_->last_begin_frame_args().IsValid() &&
      adjusted_args.frame_time <
          internal_begin_frame_source_->last_begin_frame_args().frame_time +
              internal_begin_frame_source_->last_begin_frame_args().interval) {
    // If the internal begin frame source was used, we need to ensure that the
    // frame_time of Viz begin frame are not within the interval of last
    // Internal begin frame. If it is, we need to skip this frame.
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
  if (use_internal_begin_frame_source_) {
    if (paused) {
      client_->SetBeginFrameSource(begin_frame_source_.get());
    } else {
      client_->SetBeginFrameSource(internal_begin_frame_source_.get());
    }
  }
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

void AsyncLayerTreeFrameSink::NotifyNewLocalSurfaceIdExpectedWhilePaused() {
  DCHECK(compositor_frame_sink_ptr_);
  compositor_frame_sink_ptr_->NotifyNewLocalSurfaceIdExpectedWhilePaused();
}

void AsyncLayerTreeFrameSink::OnNeedsBeginFrames(bool needs_begin_frames) {
  DCHECK(compositor_frame_sink_ptr_);

  if (!needs_begin_frames_ && needs_begin_frames &&
      num_did_not_produce_frame_before_internal_begin_frame_source_) {
    if (use_internal_begin_frame_source_) {
      // OnNeedsBeginFrames is only called when ExternalBeginFrameSourceClient
      // is the current active BeginFrameSource. This means we've just
      // switched from internal begin frame source and a CompositorFrame is
      // submitted. So only update needs_begin_frames_ here.
      UpdateNeedsBeginFramesInternal(needs_begin_frames);
      return;
    }
    // If no CompositorFrame submitted(!needs_begin_frames_ &&
    // !use_internal_begin_frame_source_), issue internal begin frames
    // after current OnNeedsBeginFrames.
    compositor_task_runner_->PostTask(
        FROM_HERE,
        base::BindOnce(&AsyncLayerTreeFrameSink::UpdateInternalBeginFrameSource,
                       weak_factory_.GetWeakPtr(), true));
  }

  //  If `auto_needs_begin_frame_` is set to true, rely on unsolicited frames
  // instead of SetNeedsBeginFrame(true) to indicate that the client needs
  // BeginFrame requests.
  if (auto_needs_begin_frame_ && needs_begin_frames) {
    if (manual_begin_frame_) {
      UpdateNeedsBeginFramesInternal(needs_begin_frames);
      // This needs to be a `PostTask`. `OnNeedsBeginFrames` is called by the
      // `ExternalBeginFrameSource` for which we are a client. This is called
      // when a new `BeginFrameObserver` is being added, but before it is
      // actually added. Due to this calling `OnBeginFrame` here would fail to
      // notify the new observer.
      compositor_task_runner_->PostTask(
          FROM_HERE,
          base::BindOnce(&AsyncLayerTreeFrameSink::SendManualBeginFrame,
                         weak_factory_.GetWeakPtr()));
    }
    return;
  }

  UpdateNeedsBeginFramesInternal(needs_begin_frames);

  compositor_frame_sink_ptr_->SetNeedsBeginFrame(needs_begin_frames);
}

void AsyncLayerTreeFrameSink::SendManualBeginFrame() {
  // It is possible that we were disconnected from `compositor_frame_sink_ptr_`
  // by the time this task was posted. Or that a subsequent update turns off
  // `needs_begin_frames_`. In these cases do not send the `OnBeginFrame`.
  if (!compositor_frame_sink_ptr_ || !needs_begin_frames_) {
    return;
  }
  base::TimeTicks frame_time = base::TimeTicks::Now();
  base::TimeDelta interval = viz::BeginFrameArgs::DefaultInterval();
  viz::BeginFrameArgs args = viz::BeginFrameArgs::Create(
      BEGINFRAME_FROM_HERE, viz::BeginFrameArgs::kManualSourceId,
      ++manual_sequence_number_, frame_time, frame_time + interval, interval,
      viz::BeginFrameArgs::NORMAL);
  OnBeginFrame(args, {}, {});
}

void AsyncLayerTreeFrameSink::OnMojoConnectionError(
    uint32_t custom_reason,
    const std::string& description) {
  // TODO(rivr): Use DLOG(FATAL) once crbug.com/1043899 is resolved.
  if (custom_reason)
    DLOG(ERROR) << description;
  if (use_internal_begin_frame_source_) {
    UpdateInternalBeginFrameSource(false);
  }
  if (client_)
    client_->DidLoseLayerTreeFrameSink();
}

void AsyncLayerTreeFrameSink::UpdateNeedsBeginFramesInternal(
    bool needs_begin_frames) {
  if (needs_begin_frames_ == needs_begin_frames) {
    return;
  }

  if (needs_begin_frames) {
    TRACE_EVENT_BEGIN("cc,benchmark", "NeedsBeginFrames",
                      perfetto::Track::FromPointer(this));
  } else {
    TRACE_EVENT_END("cc,benchmark",
                    /*"NeedsBeginFrames"*/ perfetto::Track::FromPointer(this));
  }
  needs_begin_frames_ = needs_begin_frames;
}

void AsyncLayerTreeFrameSink::UpdateInternalBeginFrameSource(
    bool use_internal_source) {
  if (use_internal_source == use_internal_begin_frame_source_) {
    return;
  }
  if (!begin_frame_source_) {
    return;
  }
  if (use_internal_source) {
    viz::BeginFrameArgs last_args =
        begin_frame_source_->last_begin_frame_args();
    if (last_args.IsValid()) {
      internal_begin_frame_source_->OnUpdateVSyncParameters(
          last_args.frame_time, last_args.interval);
    }
    if (!begin_frames_paused_) {
      client_->SetBeginFrameSource(internal_begin_frame_source_.get());
    }
    use_internal_begin_frame_source_ = true;
  } else {
    client_->SetBeginFrameSource(begin_frame_source_.get());
    use_internal_begin_frame_source_ = false;
  }
  TRACE_EVENT1("cc", "UpdateInternalBeginFrameSource",
               "use_internal_begin_frame_source_",
               use_internal_begin_frame_source_);
}

void AsyncLayerTreeFrameSink::SetTimeSourceOfInternalBeginFrameForTesting(
    std::unique_ptr<viz::DelayBasedTimeSource> source) {
  internal_begin_frame_source_ =
      std::make_unique<viz::DelayBasedBeginFrameSource>(
          std::move(source), viz::BeginFrameSource::kNotRestartableId);
}

}  // namespace mojo_embedder
}  // namespace cc
