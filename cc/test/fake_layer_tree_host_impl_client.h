// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CC_TEST_FAKE_LAYER_TREE_HOST_IMPL_CLIENT_H_
#define CC_TEST_FAKE_LAYER_TREE_HOST_IMPL_CLIENT_H_

#include "cc/trees/layer_tree_host_impl.h"
#include "components/viz/common/frame_sinks/begin_frame_args.h"

namespace viz {
struct FrameTimingDetails;
}

namespace cc {

class FakeLayerTreeHostImplClient : public LayerTreeHostImplClient {
 public:
  void DidLoseLayerTreeFrameSinkOnImplThread() override {}
  void SetBeginFrameSource(viz::BeginFrameSource* source) override {}
  void DidReceiveCompositorFrameAckOnImplThread() override {}
  void OnCanDrawStateChanged(bool can_draw) override {}
  void NotifyReadyToActivate() override;
  bool IsReadyToActivate() override;
  void NotifyReadyToDraw() override;
  void SetNeedsRedrawOnImplThread() override {}
  void SetNeedsOneBeginImplFrameOnImplThread() override {}
  void SetNeedsUpdateDisplayTreeOnImplThread() override {}
  void SetNeedsCommitOnImplThread() override {}
  void SetNeedsPrepareTilesOnImplThread() override {}
  void SetVideoNeedsBeginFrames(bool needs_begin_frames) override {}
  void SetDeferBeginMainFrameFromImpl(bool defer_begin_main_frame) override {}
  bool IsInsideDraw() override;
  void RenewTreePriority() override {}
  void PostDelayedAnimationTaskOnImplThread(base::OnceClosure task,
                                            base::TimeDelta delay) override {}
  void DidActivateSyncTree() override {}
  void DidPrepareTiles() override {}
  void DidCompletePageScaleAnimationOnImplThread() override {}
  void OnDrawForLayerTreeFrameSink(bool resourceless_software_draw,
                                   bool skip_draw) override {}
  void SetNeedsImplSideInvalidation(
      bool needs_first_draw_on_activation) override;
  void NotifyImageDecodeRequestFinished(int request_id,
                                        bool decode_succeeded) override {}
  void NotifyTransitionRequestFinished(uint32_t sequence_id) override {}
  void DidPresentCompositorFrameOnImplThread(
      uint32_t frame_token,
      PresentationTimeCallbackBuffer::PendingCallbacks activated,
      const viz::FrameTimingDetails& details) override {}

  void NotifyAnimationWorkletStateChange(AnimationWorkletMutationState state,
                                         ElementListType tree_type) override {}
  void NotifyPaintWorkletStateChange(
      Scheduler::PaintWorkletState state) override {}
  void NotifyThroughputTrackerResults(CustomTrackerResults results) override {}
  void DidObserveFirstScrollDelay(
      int source_frame_number,
      base::TimeDelta first_scroll_delay,
      base::TimeTicks first_scroll_timestamp) override {}
  bool IsInSynchronousComposite() const override;
  void FrameSinksToThrottleUpdated(
      const base::flat_set<viz::FrameSinkId>& ids) override {}
  void ClearHistory() override {}
  void SetHasActiveThreadedScroll(bool is_scrolling) override {}
  void SetWaitingForScrollEvent(bool waiting_for_scroll_event) override {}
  size_t CommitDurationSampleCountForTesting() const override;

  void reset_did_request_impl_side_invalidation() {
    did_request_impl_side_invalidation_ = false;
  }
  bool did_request_impl_side_invalidation() const {
    return did_request_impl_side_invalidation_;
  }

  void reset_ready_to_activate() { ready_to_activate_ = false; }
  bool ready_to_activate() const { return ready_to_activate_; }

  void reset_ready_to_draw() { ready_to_draw_ = false; }
  bool ready_to_draw() const { return ready_to_draw_; }

  void set_is_synchronous_composite(bool value) {
    is_synchronous_composite_ = value;
  }

 private:
  bool did_request_impl_side_invalidation_ = false;
  bool ready_to_activate_ = false;
  bool ready_to_draw_ = false;
  bool is_synchronous_composite_ = false;
};

}  // namespace cc

#endif  // CC_TEST_FAKE_LAYER_TREE_HOST_IMPL_CLIENT_H_
