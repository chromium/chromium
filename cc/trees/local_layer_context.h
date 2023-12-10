// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CC_TREES_LOCAL_LAYER_CONTEXT_H_
#define CC_TREES_LOCAL_LAYER_CONTEXT_H_

#include <memory>

#include "base/memory/raw_ptr.h"
#include "cc/cc_export.h"
#include "cc/trees/layer_context.h"
#include "cc/trees/layer_tree_host_client.h"
#include "cc/trees/layer_tree_host_single_thread_client.h"
#include "cc/trees/layer_tree_settings.h"

namespace cc {

class LayerTreeHost;
class MutatorHost;

// LocalLayerContext owns and manages a LayerTreeHostImpl to be controlled
// indirectly by a client's corresponding LayerTreeHost.
class CC_EXPORT LocalLayerContext : public LayerContext,
                                    public LayerTreeHostClient,
                                    public LayerTreeHostSingleThreadClient {
 public:
  explicit LocalLayerContext(MutatorHost* mutator_host);
  ~LocalLayerContext() override;

  // LayerContext:
  void SetTargetLocalSurfaceId(const viz::LocalSurfaceId& id) override;
  void SetVisible(bool visible) override;
  void Commit(const CommitState& state) override;

  // LayerTreeHostClient:
  void WillBeginMainFrame() override;
  void DidBeginMainFrame() override;
  void WillUpdateLayers() override;
  void DidUpdateLayers() override;
  void BeginMainFrame(const viz::BeginFrameArgs& args) override;
  void OnDeferMainFrameUpdatesChanged(bool) override;
  void OnDeferCommitsChanged(
      bool defer_status,
      PaintHoldingReason reason,
      std::optional<PaintHoldingCommitTrigger> trigger) override;
  void OnCommitRequested() override;
  void BeginMainFrameNotExpectedSoon() override;
  void BeginMainFrameNotExpectedUntil(base::TimeTicks time) override;
  void UpdateLayerTreeHost() override;
  void ApplyViewportChanges(const ApplyViewportChangesArgs& args) override;
  void UpdateCompositorScrollState(
      const CompositorCommitData& commit_data) override;
  void RequestNewLayerTreeFrameSink() override;
  void DidInitializeLayerTreeFrameSink() override;
  void DidFailToInitializeLayerTreeFrameSink() override;
  void WillCommit(const CommitState&) override;
  void DidCommit(int source_frame_number,
                 base::TimeTicks commit_start_time,
                 base::TimeTicks commit_finish_time) override;
  void DidCommitAndDrawFrame(int source_frame_number) override;
  void DidReceiveCompositorFrameAck() override;
  void DidCompletePageScaleAnimation(int source_frame_number) override;
  void DidPresentCompositorFrame(
      uint32_t frame_token,
      const gfx::PresentationFeedback& feedback) override;
  void RecordStartOfFrameMetrics() override;
  void RecordEndOfFrameMetrics(base::TimeTicks frame_begin_time,
                               ActiveFrameSequenceTrackers trackers) override;
  std::unique_ptr<BeginMainFrameMetrics> GetBeginMainFrameMetrics() override;
  std::unique_ptr<WebVitalMetrics> GetWebVitalMetrics() override;
  void NotifyThroughputTrackerResults(CustomTrackerResults results) override;
  void DidObserveFirstScrollDelay(
      int source_frame_number,
      base::TimeDelta first_scroll_delay,
      base::TimeTicks first_scroll_timestamp) override;
  void RunPaintBenchmark(int repeat_count,
                         PaintBenchmarkResult& result) override;

  // LayerTreeHostSingleThreadClient:
  void ScheduleAnimationForWebTests() override;
  void FrameIntervalUpdated(base::TimeDelta interval) override;
  void DidSubmitCompositorFrame() override;
  void DidLoseLayerTreeFrameSink() override;
  void FrameSinksToThrottleUpdated(
      const base::flat_set<viz::FrameSinkId>& ids) override;

 private:
  const raw_ptr<MutatorHost> mutator_host_;

  // The concrete LayerTreeHost which the client is controlling.
  // TODO(https://crbug.com/1431762): Own and manage a LayerTreeHostImpl
  // directly instead of proxying through a single-threaded LayerTreeHost.
  const LayerListSettings layer_list_settings_;
  std::unique_ptr<LayerTreeHost> host_;
};

}  // namespace cc

#endif  // CC_TREES_LOCAL_LAYER_CONTEXT_H_
