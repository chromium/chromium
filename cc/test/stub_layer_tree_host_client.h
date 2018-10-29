// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CC_TEST_STUB_LAYER_TREE_HOST_CLIENT_H_
#define CC_TEST_STUB_LAYER_TREE_HOST_CLIENT_H_

#include "cc/trees/layer_tree_host_client.h"

namespace cc {

class StubLayerTreeHostClient : public LayerTreeHostClient {
 public:
  ~StubLayerTreeHostClient() override;

  // LayerTreeHostClient implementation.
  void WillBeginMainFrame() override {}
  void DidBeginMainFrame() override {}
  void BeginMainFrame(const viz::BeginFrameArgs& args) override {}
  void RecordEndOfFrameMetrics(base::TimeTicks) override {}
  void BeginMainFrameNotExpectedSoon() override {}
  void BeginMainFrameNotExpectedUntil(base::TimeTicks time) override {}
  void UpdateLayerTreeHost() override {}
  void ApplyViewportChanges(const ApplyViewportChangesArgs&) override {}
  void RecordWheelAndTouchScrollingCount(bool has_scrolled_by_wheel,
                                         bool has_scrolled_by_touch) override {}
  void RequestNewLayerTreeFrameSink() override {}
  void DidInitializeLayerTreeFrameSink() override {}
  void DidFailToInitializeLayerTreeFrameSink() override {}
  void WillCommit() override {}
  void DidCommit() override {}
  void DidCommitAndDrawFrame() override {}
  void DidReceiveCompositorFrameAck() override {}
  void DidCompletePageScaleAnimation() override {}
  void DidPresentCompositorFrame(
      uint32_t frame_token,
      const gfx::PresentationFeedback& feedback) override {}
};

}  // namespace cc

#endif  // CC_TEST_STUB_LAYER_TREE_HOST_CLIENT_H_
