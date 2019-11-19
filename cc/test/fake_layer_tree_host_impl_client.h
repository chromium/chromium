// Copyright 2012 The Chromium Authors. All rights reserved.
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
  // LayerTreeHostImplClient implementation.
  void DidLoseLayerTreeFrameSinkOnImplThread() override {}
  void SetBeginFrameSource(viz::BeginFrameSource* source) override {}
  void DidReceiveCompositorFrameAckOnImplThread() override {}
  void OnCanDrawStateChanged(bool can_draw) override {}
  void NotifyReadyToActivate() override;
  void NotifyReadyToDraw() override;
  void SetNeedsRedrawOnImplThread() override {}
  void SetNeedsOneBeginImplFrameOnImplThread() override {}
  void SetNeedsCommitOnImplThread() override {}
  void SetNeedsPrepareTilesOnImplThread() override {}
  void SetVideoNeedsBeginFrames(bool needs_begin_frames) override {}
  void PostAnimationEventsToMainThreadOnImplThread(
      std::unique_ptr<MutatorEvents> events) override;
  bool IsInsideDraw() override;
  bool IsBeginMainFrameExpected() override;
  void RenewTreePriority() override {}
  void PostDelayedAnimationTaskOnImplThread(base::OnceClosure task,
                                            base::TimeDelta delay) override {}
  void DidActivateSyncTree() override {}
  void WillPrepareTiles() override {}
  void DidPrepareTiles() override {}
  void DidCompletePageScaleAnimationOnImplThread() override {}
  void OnDrawForLayerTreeFrameSink(bool resourceless_software_draw,
                                   bool skip_draw) override {}
  void NeedsImplSideInvalidation(bool needs_first_draw_on_activation) override;
  void RequestBeginMainFrameNotExpected(bool new_state) override {}
  void NotifyImageDecodeRequestFinished() override {}
  void DidPresentCompositorFrameOnImplThread(
      uint32_t frame_token,
      std::vector<LayerTreeHost::PresentationTimeCallback> callbacks,
      const viz::FrameTimingDetails& details) override {}

  void NotifyAnimationWorkletStateChange(AnimationWorkletMutationState state,
                                         ElementListType tree_type) override {}
  void NotifyPaintWorkletStateChange(
      Scheduler::PaintWorkletState state) override {}

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

 private:
  bool did_request_impl_side_invalidation_ = false;
  bool ready_to_activate_ = false;
  bool ready_to_draw_ = false;
};

}  // namespace cc

#endif  // CC_TEST_FAKE_LAYER_TREE_HOST_IMPL_CLIENT_H_
