// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CC_TEST_PIXEL_TEST_OUTPUT_SURFACE_H_
#define CC_TEST_PIXEL_TEST_OUTPUT_SURFACE_H_

#include <memory>

#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "components/viz/service/display/output_surface.h"

namespace cc {

// Software output surface for pixel tests.
class PixelTestOutputSurface : public viz::OutputSurface {
 public:
  explicit PixelTestOutputSurface(
      std::unique_ptr<viz::SoftwareOutputDevice> software_device);
  ~PixelTestOutputSurface() override;

  // OutputSurface implementation.
  void BindToClient(viz::OutputSurfaceClient* client) override;
  void EnsureBackbuffer() override;
  void DiscardBackbuffer() override;
  void Reshape(const ReshapeParams& params) override;
  void SwapBuffers(viz::OutputSurfaceFrame frame) override;
  void SetUpdateVSyncParametersCallback(
      viz::UpdateVSyncParametersCallback callback) override;
  void SetDisplayTransformHint(gfx::OverlayTransform transform) override {}
  gfx::OverlayTransform GetDisplayTransform() override;
  void ReadbackForTesting(
      base::OnceCallback<void(std::unique_ptr<viz::CopyOutputResult>)> callback)
      override;

 private:
  void SwapBuffersCallback();

  raw_ptr<viz::OutputSurfaceClient> client_ = nullptr;
  base::WeakPtrFactory<PixelTestOutputSurface> weak_ptr_factory_{this};
};

}  // namespace cc

#endif  // CC_TEST_PIXEL_TEST_OUTPUT_SURFACE_H_
