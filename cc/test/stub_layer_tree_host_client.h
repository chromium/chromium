// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CC_TEST_STUB_LAYER_TREE_HOST_CLIENT_H_
#define CC_TEST_STUB_LAYER_TREE_HOST_CLIENT_H_

#include <memory>
#include <vector>

#include "cc/paint/element_id.h"
#include "cc/trees/layer_tree_host_client.h"
#include "cc/trees/paint_holding_reason.h"

namespace cc {

struct CommitState;

class StubLayerTreeHostClient : public LayerTreeHostClient {
 public:
  ~StubLayerTreeHostClient() override;

  // LayerTreeHostClient implementation.
  void WillBeginMainFrame() override {}
  void DidBeginMainFrame() override {}
  void WillUpdateLayers() override {}
  void DidUpdateLayers() override {}
  void BeginMainFrame(const viz::BeginFrameArgs& args) override {}
  void OnDeferMainFrameUpdatesChanged(bool) override {}
  void OnDeferCommitsChanged(
      bool,
      PaintHoldingReason,
      std::optional<PaintHoldingCommitTrigger>) override {}
  void OnCommitRequested() override {}
  void RecordStartOfFrameMetrics() override {}
  void RecordEndOfFrameMetrics(base::TimeTicks,
                               ActiveFrameSequenceTrackers) override {}
  std::unique_ptr<BeginMainFrameMetrics> GetBeginMainFrameMetrics() override;
  void NotifyThroughputTrackerResults(CustomTrackerResults results) override {}
  void BeginMainFrameNotExpectedSoon() override {}
  void BeginMainFrameNotExpectedUntil(base::TimeTicks time) override {}
  void UpdateLayerTreeHost() override {}
  void ApplyViewportChanges(const ApplyViewportChangesArgs&) override {}
  void UpdateCompositorScrollState(
      const CompositorCommitData& commit_data) override {}
  void RequestNewLayerTreeFrameSink() override {}
  void DidInitializeLayerTreeFrameSink() override {}
  void DidFailToInitializeLayerTreeFrameSink() override {}
  void WillCommit(const CommitState&) override {}
  void DidCommit(int source_frame_number,
                 base::TimeTicks,
                 base::TimeTicks) override {}
  void DidCommitAndDrawFrame(int source_frame_number) override {}
  void DidObserveFirstScrollDelay(
      int source_frame_number,
      base::TimeDelta first_scroll_delay,
      base::TimeTicks first_scroll_timestamp) override {}
  void DidCompletePageScaleAnimation(int source_frame_number) override {}
  void DidPresentCompositorFrame(
      uint32_t frame_token,
      const viz::FrameTimingDetails& frame_timing_details) override {}
};

}  // namespace cc

#endif  // CC_TEST_STUB_LAYER_TREE_HOST_CLIENT_H_
