// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CC_TEST_FAKE_OUTPUT_SURFACE_CLIENT_H_
#define CC_TEST_FAKE_OUTPUT_SURFACE_CLIENT_H_

#include <vector>

#include "components/viz/service/display/output_surface_client.h"

namespace cc {

class FakeOutputSurfaceClient : public viz::OutputSurfaceClient {
 public:
  FakeOutputSurfaceClient() = default;

  void DidReceiveSwapBuffersAck(const gpu::SwapBuffersCompleteParams& params,
                                gfx::GpuFenceHandle release_fence) override;
  void DidReceiveCALayerParams(
      const gfx::CALayerParams& ca_layer_params) override {}
  void DidSwapWithSize(const gfx::Size& pixel_size) override {}
  void DidReceivePresentationFeedback(
      const gfx::PresentationFeedback& feedback) override {}
  void DidReceiveReleasedOverlays(
      const std::vector<gpu::Mailbox>& released_overlays) override {}
  void AddChildWindowToBrowser(gpu::SurfaceHandle child_window) override {}

  int swap_count() { return swap_count_; }

 private:
  int swap_count_ = 0;
};

}  // namespace cc

#endif  // CC_TEST_FAKE_OUTPUT_SURFACE_CLIENT_H_
