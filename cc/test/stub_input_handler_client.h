// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CC_TEST_STUB_INPUT_HANDLER_CLIENT_H_
#define CC_TEST_STUB_INPUT_HANDLER_CLIENT_H_

#include "cc/input/input_handler.h"

namespace cc {

class StubInputHandlerClient : public InputHandlerClient {
 public:
  ~StubInputHandlerClient() override = default;

  void WillShutdown() override {}
  void Animate(base::TimeTicks time) override {}
  void ReconcileElasticOverscrollAndRootScroll() override {}
  void SetPrefersReducedMotion(bool prefers_reduced_motion) override {}
  void UpdateRootLayerStateForSynchronousInputHandler(
      const gfx::PointF& total_scroll_offset,
      const gfx::PointF& max_scroll_offset,
      const gfx::SizeF& scrollable_size,
      float page_scale_factor,
      float min_page_scale_factor,
      float max_page_scale_factor) override {}
  void DeliverInputForBeginFrame(const viz::BeginFrameArgs& args) override {}
  void DeliverInputForHighLatencyMode() override {}
  void DeliverInputForDeadline() override {}
  void DidFinishImplFrame() override {}
  bool HasQueuedInput() const override;
  void SetScrollEventDispatchMode(
      InputHandlerClient::ScrollEventDispatchMode mode) override {}
};

}  // namespace cc

#endif  // CC_TEST_STUB_INPUT_HANDLER_CLIENT_H_
