// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/mojo_embedder/async_layer_tree_frame_sink.h"

#include <utility>

#include "base/bind.h"
#include "base/metrics/histogram.h"
#include "base/metrics/histogram_base.h"
#include "base/metrics/histogram_functions.h"
#include "base/metrics/histogram_macros.h"
#include "base/trace_event/trace_event.h"
#include "cc/base/histograms.h"
#include "cc/trees/layer_tree_frame_sink_client.h"
#include "components/viz/client/hit_test_data_provider.h"
#include "components/viz/client/local_surface_id_provider.h"
#include "components/viz/common/frame_sinks/begin_frame_args.h"
#include "components/viz/common/hit_test/hit_test_region_list.h"
#include "components/viz/common/quads/compositor_frame.h"

namespace {

base::HistogramBase* GetHistogramNamed(const char* histogram_name_format,
                                       const char* client_name) {
  if (!client_name)
    return nullptr;

  return base::LinearHistogram::FactoryMicrosecondsTimeGet(
      base::StringPrintf(histogram_name_format, client_name),
      base::TimeDelta::FromMicroseconds(1),
      base::TimeDelta::FromMilliseconds(200), 50,
      base::HistogramBase::kUmaTargetedHistogramFlag);
}
}  // namespace

namespace cc {
namespace mojo_embedder {

AsyncLayerTreeFrameSink::PipelineReporting::PipelineReporting(
    const viz::BeginFrameArgs args,
    base::TimeTicks now,
    base::HistogramBase* submit_begin_frame_histogram)
    : trace_id_(args.trace_id),
      frame_time_(now),
      submit_begin_frame_histogram_(submit_begin_frame_histogram) {}

AsyncLayerTreeFrameSink::PipelineReporting::~PipelineReporting() = default;

void AsyncLayerTreeFrameSink::PipelineReporting::Report() {
  TRACE_EVENT_WITH_FLOW1("viz,benchmark", "Graphics.Pipeline",
                         TRACE_ID_GLOBAL(trace_id_),
                         TRACE_EVENT_FLAG_FLOW_IN | TRACE_EVENT_FLAG_FLOW_OUT,
                         "step", "SubmitCompositorFrame");
  auto report_time = base::TimeTicks::Now() - frame_time_;

  if (submit_begin_frame_histogram_)
    submit_begin_frame_histogram_->AddTimeMicrosecondsGranularity(report_time);
}

AsyncLayerTreeFrameSink::InitParams::InitParams() = default;
AsyncLayerTreeFrameSink::InitParams::~InitParams() = default;

AsyncLayerTreeFrameSink::UnboundMessagePipes::UnboundMessagePipes() = default;
AsyncLayerTreeFrameSink::UnboundMessagePipes::~UnboundMessagePipes() = default;

bool AsyncLayerTreeFrameSink::UnboundMessagePipes::HasUnbound() const {
  return client_request.is_pending() &&
         (compositor_frame_sink_info.is_valid() ^
          compositor_frame_sink_associated_info.is_valid());
}

AsyncLayerTreeFrameSink::UnboundMessagePipes::UnboundMessagePipes(
    UnboundMessagePipes&& other) = default;

AsyncLayerTreeFrameSink::AsyncLayerTreeFrameSink(
    scoped_refptr<viz::ContextProvider> context_provider,
    scoped_refptr<viz::RasterContextProvider> worker_context_provider,
    InitParams* params)
    : LayerTreeFrameSink(std::move(context_provider),
                         std::move(worker_context_provider),
                         std::move(params->compositor_task_runner),
                         params->gpu_memory_buffer_manager),
      hit_test_data_provider_(std::move(params->hit_test_data_provider)),
      local_surface_id_provider_(std::move(params->local_surface_id_provider)),
      synthetic_begin_frame_source_(
          std::move(params->synthetic_begin_frame_source)),
      pipes_(std::move(params->pipes)),
      client_binding_(this),
      enable_surface_synchronization_(params->enable_surface_synchronization),
      wants_animate_only_begin_frames_(params->wants_animate_only_begin_frames),
      receive_begin_frame_histogram_(
          GetHistogramNamed("GraphicsPipeline.%s.ReceivedBeginFrame",
                            params->client_name)),
      submit_begin_frame_histogram_(GetHistogramNamed(
          "GraphicsPipeline.%s.SubmitCompositorFrameAfterBeginFrame",
          params->client_name)),
      weak_factory_(this) {
  DETACH_FROM_THREAD(thread_checker_);
}

AsyncLayerTreeFrameSink::~AsyncLayerTreeFrameSink() {}

bool AsyncLayerTreeFrameSink::BindToClient(LayerTreeFrameSinkClient* client) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);

  if (!LayerTreeFrameSink::BindToClient(client))
    return false;

  DCHECK(pipes_.HasUnbound());
  if (pipes_.compositor_frame_sink_info.is_valid()) {
    compositor_frame_sink_.Bind(std::move(pipes_.compositor_frame_sink_info));
    compositor_frame_sink_.set_connection_error_with_reason_handler(
        base::BindOnce(&AsyncLayerTreeFrameSink::OnMojoConnectionError,
                       weak_factory_.GetWeakPtr()));
    compositor_frame_sink_ptr_ = compositor_frame_sink_.get();
  } else if (pipes_.compositor_frame_sink_associated_info.is_valid()) {
    compositor_frame_sink_associated_.Bind(
        std::move(pipes_.compositor_frame_sink_associated_info));
    compositor_frame_sink_associated_.set_connection_error_with_reason_handler(
        base::BindOnce(&AsyncLayerTreeFrameSink::OnMojoConnectionError,
                       weak_factory_.GetWeakPtr()));
    compositor_frame_sink_ptr_ = compositor_frame_sink_associated_.get();
  }
  client_binding_.Bind(std::move(pipes_.client_request),
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

  return true;
}

void AsyncLayerTreeFrameSink::DetachFromClient() {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  client_->SetBeginFrameSource(nullptr);
  begin_frame_source_.reset();
  synthetic_begin_frame_source_.reset();
  client_binding_.Close();
  compositor_frame_sink_.reset();
  compositor_frame_sink_associated_.reset();
  compositor_frame_sink_ptr_ = nullptr;
  LayerTreeFrameSink::DetachFromClient();
}

void AsyncLayerTreeFrameSink::SetLocalSurfaceId(
    const viz::LocalSurfaceId& local_surface_id) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  DCHECK(local_surface_id.is_valid());
  DCHECK(enable_surface_synchronization_);
  local_surface_id_ = local_surface_id;
}

void AsyncLayerTreeFrameSink::SubmitCompositorFrame(
    viz::CompositorFrame frame) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  DCHECK(compositor_frame_sink_ptr_);
  DCHECK(frame.metadata.begin_frame_ack.has_damage);
  DCHECK_LE(viz::BeginFrameArgs::kStartingFrameNumber,
            frame.metadata.begin_frame_ack.sequence_number);
  TRACE_EVENT0("cc,benchmark",
               "AsyncLayerTreeFrameSink::SubmitCompositorFrame");

  // It's possible to request an immediate composite from cc which will bypass
  // BeginFrame. In that case, we cannot collect full graphics pipeline data.
  auto it = pipeline_reporting_frame_times_.find(
      frame.metadata.begin_frame_ack.trace_id);
  if (it != pipeline_reporting_frame_times_.end()) {
    it->second.Report();
    pipeline_reporting_frame_times_.erase(it);
  }

  if (!enable_surface_synchronization_) {
    local_surface_id_ =
        local_surface_id_provider_->GetLocalSurfaceIdForFrame(frame);
  } else {
    if (local_surface_id_ == last_submitted_local_surface_id_) {
      DCHECK_EQ(last_submitted_device_scale_factor_,
                frame.device_scale_factor());
      DCHECK_EQ(last_submitted_size_in_pixels_.height(),
                frame.size_in_pixels().height());
      DCHECK_EQ(last_submitted_size_in_pixels_.width(),
                frame.size_in_pixels().width());
    }
  }

  TRACE_EVENT_FLOW_BEGIN0(TRACE_DISABLED_BY_DEFAULT("cc.debug.ipc"),
                          "SubmitCompositorFrame", local_surface_id_.hash());
  bool tracing_enabled;
  TRACE_EVENT_CATEGORY_GROUP_ENABLED(TRACE_DISABLED_BY_DEFAULT("cc.debug.ipc"),
                                     &tracing_enabled);

  base::Optional<viz::HitTestRegionList> hit_test_region_list;
  if (hit_test_data_provider_)
    hit_test_region_list = hit_test_data_provider_->GetHitTestData(frame);
  else
    hit_test_region_list = client_->BuildHitTestData();

  if (last_submitted_local_surface_id_ != local_surface_id_) {
    last_submitted_local_surface_id_ = local_surface_id_;
    last_submitted_device_scale_factor_ = frame.device_scale_factor();
    last_submitted_size_in_pixels_ = frame.size_in_pixels();

    TRACE_EVENT_WITH_FLOW2(
        TRACE_DISABLED_BY_DEFAULT("viz.surface_id_flow"),
        "LocalSurfaceId.Submission.Flow",
        TRACE_ID_GLOBAL(local_surface_id_.submission_trace_id()),
        TRACE_EVENT_FLAG_FLOW_IN | TRACE_EVENT_FLAG_FLOW_OUT, "step",
        "SubmitCompositorFrame", "surface_id", local_surface_id_.ToString());
  }

  // The trace_id is negated in order to keep the Graphics.Pipeline and
  // Event.Pipeline flows separated.
  const int64_t trace_id = ~frame.metadata.begin_frame_ack.trace_id;
  TRACE_EVENT_WITH_FLOW1(TRACE_DISABLED_BY_DEFAULT("viz.hit_testing_flow"),
                         "Event.Pipeline", TRACE_ID_GLOBAL(trace_id),
                         TRACE_EVENT_FLAG_FLOW_OUT, "step",
                         "SubmitHitTestData");

  compositor_frame_sink_ptr_->SubmitCompositorFrame(
      local_surface_id_, std::move(frame), std::move(hit_test_region_list),
      tracing_enabled ? base::TimeTicks::Now().since_origin().InMicroseconds()
                      : 0);
}

void AsyncLayerTreeFrameSink::DidNotProduceFrame(
    const viz::BeginFrameAck& ack) {
  DCHECK(compositor_frame_sink_ptr_);
  DCHECK(!ack.has_damage);
  DCHECK_LE(viz::BeginFrameArgs::kStartingFrameNumber, ack.sequence_number);

  // TODO(yiyix): Remove duplicated calls of DidNotProduceFrame from the same
  // BeginFrames. https://crbug.com/881949
  auto it = pipeline_reporting_frame_times_.find(ack.trace_id);
  if (it != pipeline_reporting_frame_times_.end()) {
    compositor_frame_sink_ptr_->DidNotProduceFrame(ack);
    pipeline_reporting_frame_times_.erase(it);
  }
}

void AsyncLayerTreeFrameSink::DidAllocateSharedBitmap(
    mojo::ScopedSharedBufferHandle buffer,
    const viz::SharedBitmapId& id) {
  DCHECK(compositor_frame_sink_ptr_);
  compositor_frame_sink_ptr_->DidAllocateSharedBitmap(std::move(buffer), id);
}

void AsyncLayerTreeFrameSink::DidDeleteSharedBitmap(
    const viz::SharedBitmapId& id) {
  DCHECK(compositor_frame_sink_ptr_);
  compositor_frame_sink_ptr_->DidDeleteSharedBitmap(id);
}

void AsyncLayerTreeFrameSink::ForceAllocateNewId() {
  DCHECK(!enable_surface_synchronization_);
  local_surface_id_provider_->ForceAllocateNewId();
}

void AsyncLayerTreeFrameSink::DidReceiveCompositorFrameAck(
    const std::vector<viz::ReturnedResource>& resources) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  client_->ReclaimResources(resources);
  client_->DidReceiveCompositorFrameAck();
}

void AsyncLayerTreeFrameSink::DidPresentCompositorFrame(
    uint32_t presentation_token,
    const gfx::PresentationFeedback& feedback) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  client_->DidPresentCompositorFrame(presentation_token, feedback);
}

void AsyncLayerTreeFrameSink::OnBeginFrame(const viz::BeginFrameArgs& args) {
  DCHECK_LE(pipeline_reporting_frame_times_.size(), 25u);
  if (args.trace_id != -1) {
    base::TimeTicks current_time = base::TimeTicks::Now();
    PipelineReporting report(args, current_time, submit_begin_frame_histogram_);
    pipeline_reporting_frame_times_.emplace(args.trace_id, report);
    // Missed BeginFrames use the frame time of the last received BeginFrame
    // which is bogus from a reporting perspective if nothing has been updating
    // on screen for a while.
    if (args.type != viz::BeginFrameArgs::MISSED) {
      base::TimeDelta frame_difference = current_time - args.frame_time;

      if (receive_begin_frame_histogram_) {
        receive_begin_frame_histogram_->AddTimeMicrosecondsGranularity(
            frame_difference);
      }
    }
  }
  if (!needs_begin_frames_) {
    TRACE_EVENT_WITH_FLOW1("viz,benchmark", "Graphics.Pipeline",
                           TRACE_ID_GLOBAL(args.trace_id),
                           TRACE_EVENT_FLAG_FLOW_IN | TRACE_EVENT_FLAG_FLOW_OUT,
                           "step", "ReceiveBeginFrameDiscard");
    // We had a race with SetNeedsBeginFrame(false) and still need to let the
    // sink know that we didn't use this BeginFrame.
    DidNotProduceFrame(viz::BeginFrameAck(args, false));
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
    const std::vector<viz::ReturnedResource>& resources) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  client_->ReclaimResources(resources);
}

void AsyncLayerTreeFrameSink::OnNeedsBeginFrames(bool needs_begin_frames) {
  DCHECK(compositor_frame_sink_ptr_);
  needs_begin_frames_ = needs_begin_frames;
  compositor_frame_sink_ptr_->SetNeedsBeginFrame(needs_begin_frames);
}

void AsyncLayerTreeFrameSink::OnMojoConnectionError(
    uint32_t custom_reason,
    const std::string& description) {
  if (custom_reason)
    DLOG(FATAL) << description;
  if (client_)
    client_->DidLoseLayerTreeFrameSink();
}

}  // namespace mojo_embedder
}  // namespace cc
