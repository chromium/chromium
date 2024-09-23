// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CC_TEST_FAKE_LAYER_TREE_FRAME_SINK_H_
#define CC_TEST_FAKE_LAYER_TREE_FRAME_SINK_H_

#include <stddef.h>

#include <memory>
#include <utility>
#include <vector>

#include "base/check.h"
#include "base/functional/callback.h"
#include "base/memory/ptr_util.h"
#include "base/memory/read_only_shared_memory_region.h"
#include "base/memory/weak_ptr.h"
#include "cc/trees/layer_tree_frame_sink.h"
#include "components/viz/common/frame_sinks/begin_frame_args.h"
#include "components/viz/common/frame_timing_details.h"
#include "components/viz/common/quads/compositor_frame.h"
#include "components/viz/service/display/software_output_device.h"
#include "components/viz/test/test_context_provider.h"
#include "components/viz/test/test_gles2_interface.h"
#include "components/viz/test/test_raster_interface.h"
#include "gpu/command_buffer/client/test_gpu_memory_buffer_manager.h"

namespace viz {
class BeginFrameSource;
}

namespace cc {

class FakeLayerTreeFrameSink : public LayerTreeFrameSink {
 public:
  class Builder {
   public:
    Builder();
    ~Builder();

    // Build should only be called once. After calling Build, the Builder is
    // no longer valid and should not be used (other than to destroy it).
    std::unique_ptr<FakeLayerTreeFrameSink> Build();

    // Calls a function on both the compositor and worker context.
    template <typename... Args>
    Builder& AllContexts(void (viz::TestRasterInterface::*context_fn)(Args...),
                         Args... args) {
      DCHECK(compositor_context_provider_);
      (compositor_context_provider_->UnboundTestRasterInterface()->*context_fn)(
          std::forward<Args>(args)...);
      DCHECK(worker_context_provider_);
      (worker_context_provider_->UnboundTestRasterInterface()->*context_fn)(
          std::forward<Args>(args)...);

      return *this;
    }

   private:
    scoped_refptr<viz::TestContextProvider> compositor_context_provider_;
    scoped_refptr<viz::TestContextProvider> worker_context_provider_;
  };

  ~FakeLayerTreeFrameSink() override;

  static std::unique_ptr<FakeLayerTreeFrameSink> Create3d() {
    return base::WrapUnique(
        new FakeLayerTreeFrameSink(viz::TestContextProvider::Create(),
                                   viz::TestContextProvider::CreateWorker()));
  }

  static std::unique_ptr<FakeLayerTreeFrameSink> Create3d(
      scoped_refptr<viz::TestContextProvider> context_provider) {
    return base::WrapUnique(new FakeLayerTreeFrameSink(
        std::move(context_provider), viz::TestContextProvider::CreateWorker()));
  }

  static std::unique_ptr<FakeLayerTreeFrameSink> Create3d(
      scoped_refptr<viz::TestContextProvider> context_provider,
      scoped_refptr<viz::TestContextProvider> worker_context_provider) {
    return base::WrapUnique(new FakeLayerTreeFrameSink(
        std::move(context_provider), std::move(worker_context_provider)));
  }

  static std::unique_ptr<FakeLayerTreeFrameSink> Create3dForGpuRasterization() {
    return Builder()
        .AllContexts(&viz::TestRasterInterface::set_gpu_rasterization, true)
        .Build();
  }

  static std::unique_ptr<FakeLayerTreeFrameSink> CreateSoftware() {
    return base::WrapUnique(new FakeLayerTreeFrameSink(nullptr, nullptr));
  }

  // LayerTreeFrameSink implementation.
  bool BindToClient(LayerTreeFrameSinkClient* client) override;
  void DetachFromClient() override;
  void SubmitCompositorFrame(viz::CompositorFrame frame,
                             bool hit_test_data_changed) override;
  void DidNotProduceFrame(const viz::BeginFrameAck& ack,
                          FrameSkippedReason reason) override;
  void DidAllocateSharedBitmap(base::ReadOnlySharedMemoryRegion region,
                               const viz::SharedBitmapId& id) override;
  void DidDeleteSharedBitmap(const viz::SharedBitmapId& id) override;

  viz::CompositorFrame* last_sent_frame() { return last_sent_frame_.get(); }
  size_t num_sent_frames() { return num_sent_frames_; }

  LayerTreeFrameSinkClient* client() { return client_; }

  const std::vector<viz::TransferableResource>& resources_held_by_parent() {
    return resources_held_by_parent_;
  }

  gfx::Rect last_swap_rect() const { return last_swap_rect_; }

  const std::vector<viz::SharedBitmapId>& shared_bitmaps() const {
    return shared_bitmaps_;
  }

  void ReturnResourcesHeldByParent();

  // A BeginFrame request usually comes with the frames that have been
  // presented. This allows a test to inform the compositor when a frame should
  // be considered as presented to the user.
  void NotifyDidPresentCompositorFrame(uint32_t frame_token,
                                       const viz::FrameTimingDetails& details);

 protected:
  FakeLayerTreeFrameSink(
      scoped_refptr<viz::RasterContextProvider> context_provider,
      scoped_refptr<viz::RasterContextProvider> worker_context_provider);

  gpu::TestGpuMemoryBufferManager test_gpu_memory_buffer_manager_;

  std::vector<viz::SharedBitmapId> shared_bitmaps_;
  std::unique_ptr<viz::CompositorFrame> last_sent_frame_;
  size_t num_sent_frames_ = 0;
  std::vector<viz::TransferableResource> resources_held_by_parent_;
  gfx::Rect last_swap_rect_;

 private:
  void DidReceiveCompositorFrameAck();

  std::unique_ptr<viz::BeginFrameSource> begin_frame_source_;
  base::WeakPtrFactory<FakeLayerTreeFrameSink> weak_ptr_factory_{this};
};

}  // namespace cc

#endif  // CC_TEST_FAKE_LAYER_TREE_FRAME_SINK_H_
