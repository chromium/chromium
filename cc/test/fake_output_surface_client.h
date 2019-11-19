// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CC_TEST_FAKE_OUTPUT_SURFACE_CLIENT_H_
#define CC_TEST_FAKE_OUTPUT_SURFACE_CLIENT_H_

#include "components/viz/service/display/output_surface_client.h"

namespace cc {

class FakeOutputSurfaceClient : public viz::OutputSurfaceClient {
 public:
  FakeOutputSurfaceClient() = default;

  void SetNeedsRedrawRect(const gfx::Rect& damage_rect) override {}
  void DidReceiveSwapBuffersAck(const gfx::SwapTimings& timings) override;
  void DidReceiveTextureInUseResponses(
      const gpu::TextureInUseResponses& responses) override {}
  void DidReceiveCALayerParams(
      const gfx::CALayerParams& ca_layer_params) override {}
  void DidSwapWithSize(const gfx::Size& pixel_size) override {}
  void DidFinishLatencyInfo(
      const std::vector<ui::LatencyInfo>& latency_info) override {}
  void DidReceivePresentationFeedback(
      const gfx::PresentationFeedback& feedback) override {}

  int swap_count() { return swap_count_; }

 private:
  int swap_count_ = 0;
};

}  // namespace cc

#endif  // CC_TEST_FAKE_OUTPUT_SURFACE_CLIENT_H_
