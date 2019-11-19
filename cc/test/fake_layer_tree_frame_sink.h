// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CC_TEST_FAKE_LAYER_TREE_FRAME_SINK_H_
#define CC_TEST_FAKE_LAYER_TREE_FRAME_SINK_H_

#include <stddef.h>
#include <vector>

#include "base/callback.h"
#include "base/logging.h"
#include "base/memory/ptr_util.h"
#include "base/memory/read_only_shared_memory_region.h"
#include "base/memory/weak_ptr.h"
#include "base/time/time.h"
#include "cc/trees/layer_tree_frame_sink.h"
#include "components/viz/common/frame_sinks/begin_frame_args.h"
#include "components/viz/common/quads/compositor_frame.h"
#include "components/viz/service/display/software_output_device.h"
#include "components/viz/test/test_context_provider.h"
#include "components/viz/test/test_gles2_interface.h"
#include "components/viz/test/test_gpu_memory_buffer_manager.h"

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
    Builder& AllContexts(void (viz::TestGLES2Interface::*fn)(Args...),
                         Args... args) {
      DCHECK(compositor_context_provider_);
      DCHECK(worker_context_provider_);
      (compositor_context_provider_->UnboundTestContextGL()->*fn)(
          std::forward<Args>(args)...);
      (worker_context_provider_->UnboundTestContextGL()->*fn)(
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

  static std::unique_ptr<FakeLayerTreeFrameSink> Create3d(
      std::unique_ptr<viz::TestGLES2Interface> gl) {
    return base::WrapUnique(new FakeLayerTreeFrameSink(
        viz::TestContextProvider::Create(std::move(gl)),
        viz::TestContextProvider::CreateWorker()));
  }

  static std::unique_ptr<FakeLayerTreeFrameSink> Create3dForGpuRasterization(
      int max_msaa_samples = 0,
      bool msaa_is_slow = false) {
    return Builder()
        .AllContexts(&viz::TestGLES2Interface::set_gpu_rasterization, true)
        .AllContexts(&viz::TestGLES2Interface::set_msaa_is_slow, msaa_is_slow)
        .AllContexts(&viz::TestGLES2Interface::SetMaxSamples, max_msaa_samples)
        .Build();
  }

  static std::unique_ptr<FakeLayerTreeFrameSink> Create3dForOopRasterization(
      int max_msaa_samples = 0,
      bool msaa_is_slow = false) {
    // TODO(enne): this should really use a TestRasterInterface.
    // It's very fake to use "supports oop raster" on a gles2 interface.
    return Builder()
        .AllContexts(&viz::TestGLES2Interface::set_gpu_rasterization, true)
        .AllContexts(&viz::TestGLES2Interface::set_supports_oop_raster, true)
        .AllContexts(&viz::TestGLES2Interface::set_msaa_is_slow, msaa_is_slow)
        .AllContexts(&viz::TestGLES2Interface::SetMaxSamples, max_msaa_samples)
        .Build();
  }

  static std::unique_ptr<FakeLayerTreeFrameSink> CreateSoftware() {
    return base::WrapUnique(new FakeLayerTreeFrameSink(nullptr, nullptr));
  }

  // LayerTreeFrameSink implementation.
  bool BindToClient(LayerTreeFrameSinkClient* client) override;
  void DetachFromClient() override;
  void SubmitCompositorFrame(viz::CompositorFrame frame,
                             bool hit_test_data_changed,
                             bool show_hit_test_borders) override;
  void DidNotProduceFrame(const viz::BeginFrameAck& ack) override;
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

 protected:
  FakeLayerTreeFrameSink(
      scoped_refptr<viz::ContextProvider> context_provider,
      scoped_refptr<viz::RasterContextProvider> worker_context_provider);

  viz::TestGpuMemoryBufferManager test_gpu_memory_buffer_manager_;

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
