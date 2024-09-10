// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CC_TEST_PIXEL_TEST_H_
#define CC_TEST_PIXEL_TEST_H_

#include <memory>
#include <utility>
#include <vector>

#include "base/files/file_util.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/shared_memory_mapping.h"
#include "base/task/single_thread_task_runner.h"
#include "base/test/scoped_feature_list.h"
#include "build/build_config.h"
#include "cc/test/pixel_comparator.h"
#include "cc/trees/layer_tree_settings.h"
#include "components/viz/client/client_resource_provider.h"
#include "components/viz/common/gpu/raster_context_provider.h"
#include "components/viz/common/quads/compositor_render_pass.h"
#include "components/viz/common/resources/shared_bitmap.h"
#include "components/viz/service/display/aggregated_frame.h"
#include "components/viz/service/display/output_surface.h"
#include "components/viz/service/display/skia_renderer.h"
#include "components/viz/service/display/software_renderer.h"
#include "components/viz/test/test_gpu_service_holder.h"
#include "gpu/command_buffer/client/gpu_memory_buffer_manager.h"
#include "gpu/command_buffer/service/shared_image/shared_image_manager.h"
#include "gpu/command_buffer/service/sync_point_manager.h"
#include "gpu/ipc/in_process_command_buffer.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/gfx/geometry/size.h"

namespace viz {
class CopyOutputResult;
class DirectRenderer;
class DisplayResourceProvider;
class GpuServiceImpl;
}

namespace cc {
class FakeOutputSurfaceClient;

class PixelTest : public testing::Test {
 protected:
  // Some graphics backends require command line or base::Feature initialization
  // which must occur in the constructor to avoid potential races.
  enum GraphicsBackend {
    // The pixel test will be initialized for software or GL renderers. No work
    // needs to be done in the constructor.
    kDefault,
    // SkiaRenderer with the Vulkan backend will be used.
    kSkiaVulkan,
    // SkiaRenderer with the Skia Graphite on Dawn will be used.
    kSkiaGraphiteDawn,
    // SkiaRenderer with the Skia Graphite on Metal will be used.
    kSkiaGraphiteMetal,
  };

  explicit PixelTest(GraphicsBackend backend = kDefault);
  ~PixelTest() override;

  bool RunPixelTest(viz::AggregatedRenderPassList* pass_list,
                    const base::FilePath& ref_file,
                    const PixelComparator& comparator);

  bool RunPixelTest(viz::AggregatedRenderPassList* pass_list,
                    std::vector<SkColor>* ref_pixels,
                    const PixelComparator& comparator);

  bool RunPixelTest(viz::AggregatedRenderPassList* pass_list,
                    SkBitmap ref_bitmap,
                    const PixelComparator& comparator);

  bool RunPixelTestWithCopyOutputRequest(
      viz::AggregatedRenderPassList* pass_list,
      viz::AggregatedRenderPass* target,
      const base::FilePath& ref_file,
      const PixelComparator& comparator);

  bool RunPixelTestWithCopyOutputRequestAndArea(
      viz::AggregatedRenderPassList* pass_list,
      viz::AggregatedRenderPass* target,
      const base::FilePath& ref_file,
      const PixelComparator& comparator,
      const gfx::Rect* copy_rect);

  viz::GpuServiceImpl* gpu_service() {
    return gpu_service_holder_->gpu_service();
  }

  gpu::CommandBufferTaskExecutor* task_executor() {
    return gpu_service_holder_->task_executor();
  }

  // Allocates a SharedMemory bitmap.
  void AllocateSharedBitmapMemory(
      scoped_refptr<viz::RasterContextProvider> context_provider,
      const gfx::Size& size,
      scoped_refptr<gpu::ClientSharedImage>& shared_image,
      base::WritableSharedMemoryMapping& mapping,
      gpu::SyncToken& sync_token);
  // Uses AllocateSharedBitmapMemory() then registers a ResourceId with the
  // |child_resource_provider_|, and copies the contents of |source| into the
  // software resource backing.
  viz::ResourceId AllocateAndFillSoftwareResource(
      scoped_refptr<viz::RasterContextProvider> context_provider,
      const gfx::Size& size,
      const SkBitmap& source);

  // |scoped_feature_list_| must be the first member to ensure that it is
  // destroyed after any member that might be using it.
  base::test::ScopedFeatureList scoped_feature_list_;
  viz::TestGpuServiceHolder::ScopedResetter gpu_service_resetter_;

  // For SkiaRenderer.
  raw_ptr<viz::TestGpuServiceHolder> gpu_service_holder_ = nullptr;

  viz::RendererSettings renderer_settings_;
  viz::DebugRendererSettings debug_settings_;
  gfx::Size device_viewport_size_;
  gfx::DisplayColorSpaces display_color_spaces_;
  viz::SurfaceDamageRectList surface_damage_rect_list_;
  std::unique_ptr<viz::DisplayCompositorMemoryAndTaskController>
      display_controller_;
  std::unique_ptr<FakeOutputSurfaceClient> output_surface_client_;
  std::unique_ptr<viz::OutputSurface> output_surface_;
  std::unique_ptr<viz::DisplayResourceProvider> resource_provider_;
  scoped_refptr<viz::RasterContextProvider> child_context_provider_;
  std::unique_ptr<viz::ClientResourceProvider> child_resource_provider_;
  std::unique_ptr<viz::DirectRenderer> renderer_;
  raw_ptr<viz::SoftwareRenderer> software_renderer_ = nullptr;
  std::unique_ptr<SkBitmap> result_bitmap_;

  void SetUpSkiaRenderer(gfx::SurfaceOrigin output_surface_origin);
  void SetUpSoftwareRenderer();

  void TearDown() override;

  bool use_skia_graphite() const {
    return graphics_backend_ == GraphicsBackend::kSkiaGraphiteDawn ||
           graphics_backend_ == GraphicsBackend::kSkiaGraphiteMetal;
  }

 private:
  void FinishSetup();

  // Render |pass_list| and readback the |copy_rect| portion of |target| to
  // |result_bitmap_|.
  void RenderReadbackTargetAndAreaToResultBitmap(
      viz::AggregatedRenderPassList* pass_list,
      viz::AggregatedRenderPass* target,
      const gfx::Rect* copy_rect);

  void ReadbackResult(base::OnceClosure quit_run_loop,
                      std::unique_ptr<viz::CopyOutputResult> result);

  std::unique_ptr<gl::DisableNullDrawGLBindings> enable_pixel_output_;
  GraphicsBackend graphics_backend_ = GraphicsBackend::kDefault;
};

}  // namespace cc

#endif  // CC_TEST_PIXEL_TEST_H_
