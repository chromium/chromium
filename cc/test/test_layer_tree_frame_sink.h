// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CC_TEST_TEST_LAYER_TREE_FRAME_SINK_H_
#define CC_TEST_TEST_LAYER_TREE_FRAME_SINK_H_

#include "base/memory/ref_counted.h"
#include "base/memory/weak_ptr.h"
#include "cc/trees/layer_tree_frame_sink.h"
#include "components/viz/common/display/renderer_settings.h"
#include "components/viz/common/frame_sinks/begin_frame_source.h"
#include "components/viz/common/frame_timing_details_map.h"
#include "components/viz/common/surfaces/parent_local_surface_id_allocator.h"
#include "components/viz/service/display/display.h"
#include "components/viz/service/display/display_client.h"
#include "components/viz/service/frame_sinks/frame_sink_manager_impl.h"
#include "components/viz/test/test_shared_bitmap_manager.h"
#include "services/viz/public/mojom/compositing/compositor_frame_sink.mojom.h"

namespace base {
class SingleThreadTaskRunner;
}

namespace viz {
class CompositorFrameSinkSupport;
}  // namespace viz

namespace cc {

class TestLayerTreeFrameSinkClient {
 public:
  virtual ~TestLayerTreeFrameSinkClient() {}

  virtual std::unique_ptr<viz::SkiaOutputSurface>
  CreateDisplaySkiaOutputSurface() = 0;

  // This passes the ContextProvider being used by LayerTreeHostImpl which
  // can be used for the OutputSurface optionally.
  virtual std::unique_ptr<viz::OutputSurface> CreateDisplayOutputSurface(
      scoped_refptr<viz::ContextProvider> compositor_context_provider) = 0;

  virtual void DisplayReceivedLocalSurfaceId(
      const viz::LocalSurfaceId& local_surface_id) = 0;
  virtual void DisplayReceivedCompositorFrame(
      const viz::CompositorFrame& frame) = 0;
  virtual void DisplayWillDrawAndSwap(bool will_draw_and_swap,
                                      viz::RenderPassList* render_passes) = 0;
  virtual void DisplayDidDrawAndSwap() = 0;
};

// LayerTreeFrameSink that owns and forwards frames to a Display.
class TestLayerTreeFrameSink : public LayerTreeFrameSink,
                               public viz::mojom::CompositorFrameSinkClient,
                               public viz::DisplayClient,
                               public viz::ExternalBeginFrameSourceClient {
 public:
  // Pass true for |force_disable_reclaim_resources| to act like the Display
  // is out-of-process and can't return resources synchronously.
  // If |begin_frame_source| is specified, |disable_display_vsync| and
  // |refresh_rate| are ignored.
  TestLayerTreeFrameSink(
      scoped_refptr<viz::ContextProvider> compositor_context_provider,
      scoped_refptr<viz::RasterContextProvider> worker_context_provider,
      gpu::GpuMemoryBufferManager* gpu_memory_buffer_manager,
      const viz::RendererSettings& renderer_settings,
      scoped_refptr<base::SingleThreadTaskRunner> task_runner,
      bool synchronous_composite,
      bool disable_display_vsync,
      double refresh_rate,
      viz::BeginFrameSource* begin_frame_source = nullptr);
  ~TestLayerTreeFrameSink() override;

  // This client must be set before BindToClient() happens.
  void SetClient(TestLayerTreeFrameSinkClient* client) {
    test_client_ = client;
  }
  void SetEnlargePassTextureAmount(const gfx::Size& s) {
    enlarge_pass_texture_amount_ = s;
  }

  // Forward the color space to the existant Display, or the new one when it is
  // created.
  void SetDisplayColorSpace(const gfx::ColorSpace& output_color_space);

  viz::Display* display() const { return display_.get(); }

  // LayerTreeFrameSink implementation.
  bool BindToClient(LayerTreeFrameSinkClient* client) override;
  void DetachFromClient() override;
  void SetLocalSurfaceId(const viz::LocalSurfaceId& local_surface_id) override;
  void SubmitCompositorFrame(viz::CompositorFrame frame,
                             bool hit_test_data_changed,
                             bool show_hit_test_borders) override;
  void DidNotProduceFrame(const viz::BeginFrameAck& ack) override;
  void DidAllocateSharedBitmap(base::ReadOnlySharedMemoryRegion buffer,
                               const viz::SharedBitmapId& id) override;
  void DidDeleteSharedBitmap(const viz::SharedBitmapId& id) override;

  // mojom::CompositorFrameSinkClient implementation.
  void DidReceiveCompositorFrameAck(
      const std::vector<viz::ReturnedResource>& resources) override;
  void OnBeginFrame(const viz::BeginFrameArgs& args,
                    const viz::FrameTimingDetailsMap& timing_details) override;
  void ReclaimResources(
      const std::vector<viz::ReturnedResource>& resources) override;
  void OnBeginFramePausedChanged(bool paused) override;

  // DisplayClient implementation.
  void DisplayOutputSurfaceLost() override;
  void DisplayWillDrawAndSwap(bool will_draw_and_swap,
                              viz::RenderPassList* render_passes) override;
  void DisplayDidDrawAndSwap() override;
  void DisplayDidReceiveCALayerParams(
      const gfx::CALayerParams& ca_layer_params) override;
  void DisplayDidCompleteSwapWithSize(const gfx::Size& pixel_size) override;
  void SetPreferredFrameInterval(base::TimeDelta interval) override {}
  base::TimeDelta GetPreferredFrameIntervalForFrameSinkId(
      const viz::FrameSinkId& id) override;

  const std::set<viz::SharedBitmapId>& owned_bitmaps() const {
    return owned_bitmaps_;
  }

 private:
  // ExternalBeginFrameSource implementation.
  void OnNeedsBeginFrames(bool needs_begin_frames) override;

  void SendCompositorFrameAckToClient();

  const bool synchronous_composite_;
  const bool disable_display_vsync_;
  const viz::RendererSettings renderer_settings_;
  const double refresh_rate_;

  viz::FrameSinkId frame_sink_id_;
  // TODO(danakj): These don't need to be stored in unique_ptrs when
  // LayerTreeFrameSink is owned/destroyed on the compositor thread.
  std::unique_ptr<viz::TestSharedBitmapManager> shared_bitmap_manager_;
  std::unique_ptr<viz::FrameSinkManagerImpl> frame_sink_manager_;
  std::unique_ptr<viz::ParentLocalSurfaceIdAllocator>
      parent_local_surface_id_allocator_;
  gfx::Size display_size_;
  float device_scale_factor_ = 0;
  gfx::ColorSpace output_color_space_ = gfx::ColorSpace::CreateSRGB();

  // Uses surface_manager_.
  std::unique_ptr<viz::CompositorFrameSinkSupport> support_;

  std::unique_ptr<viz::SyntheticBeginFrameSource> begin_frame_source_;
  viz::BeginFrameSource* client_provided_begin_frame_source_;    // Not owned.
  viz::BeginFrameSource* display_begin_frame_source_ = nullptr;  // Not owned.
  viz::ExternalBeginFrameSource external_begin_frame_source_;

  // Uses surface_manager_, begin_frame_source_, shared_bitmap_manager_.
  std::unique_ptr<viz::Display> display_;

  TestLayerTreeFrameSinkClient* test_client_ = nullptr;
  gfx::Size enlarge_pass_texture_amount_;

  // The set of SharedBitmapIds that have been reported as allocated to this
  // interface. On closing this interface, the display compositor should drop
  // ownership of the bitmaps with these ids to avoid leaking them.
  std::set<viz::SharedBitmapId> owned_bitmaps_;

  base::WeakPtrFactory<TestLayerTreeFrameSink> weak_ptr_factory_{this};
};

}  // namespace cc

#endif  // CC_TEST_TEST_LAYER_TREE_FRAME_SINK_H_
