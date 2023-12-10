// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/trees/local_layer_context.h"

#include <memory>

#include "base/task/single_thread_task_runner.h"
#include "cc/raster/categorized_worker_pool.h"
#include "cc/trees/commit_state.h"
#include "cc/trees/layer_tree_host.h"
#include "cc/trees/layer_tree_settings.h"

namespace cc {

namespace {

LayerListSettings CreateSettings() {
  LayerListSettings settings;
  settings.commit_to_active_tree = true;
  return settings;
}

LayerTreeHost::InitParams CreateInitParams(LocalLayerContext& context,
                                           const LayerListSettings& settings,
                                           MutatorHost* mutator_host) {
  LayerTreeHost::InitParams params;
  params.client = &context;
  params.settings = &settings;
  params.task_graph_runner = CategorizedWorkerPool::GetOrCreate();
  params.main_task_runner = base::SingleThreadTaskRunner::GetCurrentDefault();
  params.mutator_host = mutator_host;
  return params;
}

}  // namespace

LocalLayerContext::LocalLayerContext(MutatorHost* mutator_host)
    : mutator_host_(mutator_host),
      layer_list_settings_(CreateSettings()),
      host_(LayerTreeHost::CreateSingleThreaded(
          this,
          CreateInitParams(*this, layer_list_settings_, mutator_host_))) {}

LocalLayerContext::~LocalLayerContext() = default;

void LocalLayerContext::SetTargetLocalSurfaceId(const viz::LocalSurfaceId& id) {
  host_->SetTargetLocalSurfaceId(id);
}

void LocalLayerContext::SetVisible(bool visible) {
  host_->SetVisible(visible);
}

void LocalLayerContext::Commit(const CommitState& state) {
  // TODO(https://crbug.com/1431762): Actually update the tree contents.
  host_->SetViewportRectAndScale(state.device_viewport_rect,
                                 state.device_scale_factor,
                                 state.local_surface_id_from_parent);
}

void LocalLayerContext::WillBeginMainFrame() {}

void LocalLayerContext::DidBeginMainFrame() {}

void LocalLayerContext::WillUpdateLayers() {}

void LocalLayerContext::DidUpdateLayers() {}

void LocalLayerContext::BeginMainFrame(const viz::BeginFrameArgs& args) {}

void LocalLayerContext::OnDeferMainFrameUpdatesChanged(bool) {}

void LocalLayerContext::OnDeferCommitsChanged(
    bool defer_status,
    PaintHoldingReason reason,
    std::optional<PaintHoldingCommitTrigger> trigger) {}

void LocalLayerContext::OnCommitRequested() {}

void LocalLayerContext::BeginMainFrameNotExpectedSoon() {}

void LocalLayerContext::BeginMainFrameNotExpectedUntil(base::TimeTicks time) {}

void LocalLayerContext::UpdateLayerTreeHost() {}

void LocalLayerContext::ApplyViewportChanges(
    const ApplyViewportChangesArgs& args) {}

void LocalLayerContext::UpdateCompositorScrollState(
    const CompositorCommitData& commit_data) {}

void LocalLayerContext::RequestNewLayerTreeFrameSink() {}

void LocalLayerContext::DidInitializeLayerTreeFrameSink() {}

void LocalLayerContext::DidFailToInitializeLayerTreeFrameSink() {}

void LocalLayerContext::WillCommit(const CommitState&) {}

void LocalLayerContext::DidCommit(int source_frame_number,
                                  base::TimeTicks commit_start_time,
                                  base::TimeTicks commit_finish_time) {}

void LocalLayerContext::DidCommitAndDrawFrame(int source_frame_number) {}

void LocalLayerContext::DidReceiveCompositorFrameAck() {}

void LocalLayerContext::DidCompletePageScaleAnimation(int source_frame_number) {
}

void LocalLayerContext::DidPresentCompositorFrame(
    uint32_t frame_token,
    const gfx::PresentationFeedback& feedback) {}

void LocalLayerContext::RecordStartOfFrameMetrics() {}

void LocalLayerContext::RecordEndOfFrameMetrics(
    base::TimeTicks frame_begin_time,
    ActiveFrameSequenceTrackers trackers) {}

std::unique_ptr<BeginMainFrameMetrics>
LocalLayerContext::GetBeginMainFrameMetrics() {
  return nullptr;
}

std::unique_ptr<WebVitalMetrics> LocalLayerContext::GetWebVitalMetrics() {
  return nullptr;
}

void LocalLayerContext::NotifyThroughputTrackerResults(
    CustomTrackerResults results) {}

void LocalLayerContext::DidObserveFirstScrollDelay(
    int source_frame_number,
    base::TimeDelta first_scroll_delay,
    base::TimeTicks first_scroll_timestamp) {}

void LocalLayerContext::RunPaintBenchmark(int repeat_count,
                                          PaintBenchmarkResult& result) {}

void LocalLayerContext::ScheduleAnimationForWebTests() {}

void LocalLayerContext::FrameIntervalUpdated(base::TimeDelta interval) {}

void LocalLayerContext::DidSubmitCompositorFrame() {}

void LocalLayerContext::DidLoseLayerTreeFrameSink() {}

void LocalLayerContext::FrameSinksToThrottleUpdated(
    const base::flat_set<viz::FrameSinkId>& ids) {}

}  // namespace cc
