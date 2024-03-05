// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/test/layer_tree_pixel_resource_test.h"

#include "base/task/single_thread_task_runner.h"
#include "cc/layers/layer.h"
#include "cc/raster/bitmap_raster_buffer_provider.h"
#include "cc/raster/gpu_raster_buffer_provider.h"
#include "cc/raster/one_copy_raster_buffer_provider.h"
#include "cc/raster/raster_buffer_provider.h"
#include "cc/raster/zero_copy_raster_buffer_provider.h"
#include "cc/resources/resource_pool.h"
#include "cc/trees/layer_tree_frame_sink.h"
#include "gpu/GLES2/gl2extchromium.h"

namespace cc {

LayerTreeHostPixelResourceTest::LayerTreeHostPixelResourceTest(
    RasterTestConfig test_config)
    : LayerTreePixelTest(test_config.renderer_type), test_config_(test_config) {
  set_raster_type(test_config_.raster_type);
}

const char* LayerTreeHostPixelResourceTest::GetRendererSuffix() const {
  switch (renderer_type_) {
    case viz::RendererType::kSkiaGL:
      return "skia_gl";
    case viz::RendererType::kSkiaVk:
      return "skia_vk";
    case viz::RendererType::kSkiaGraphiteDawn:
    case viz::RendererType::kSkiaGraphiteMetal:
      return "skia_graphite";
    case viz::RendererType::kSoftware:
      return "sw";
  }
}

std::unique_ptr<RasterBufferProvider>
LayerTreeHostPixelResourceTest::CreateRasterBufferProvider(
    LayerTreeHostImpl* host_impl) {
  scoped_refptr<base::SingleThreadTaskRunner> task_runner =
      task_runner_provider()->HasImplThread()
          ? task_runner_provider()->ImplThreadTaskRunner()
          : task_runner_provider()->MainThreadTaskRunner();
  DCHECK(task_runner);

  LayerTreeFrameSink* layer_tree_frame_sink =
      host_impl->layer_tree_frame_sink();
  viz::RasterContextProvider* compositor_context_provider =
      layer_tree_frame_sink->context_provider();
  viz::RasterContextProvider* worker_context_provider =
      layer_tree_frame_sink->worker_context_provider();
  int max_bytes_per_copy_operation = 1024 * 1024;
  int max_staging_buffer_usage_in_bytes = 32 * 1024 * 1024;

  RasterCapabilities raster_caps;
  if (compositor_context_provider) {
    if (host_impl->settings().use_rgba_4444) {
      raster_caps.tile_format = viz::SinglePlaneFormat::kRGBA_4444;
    } else {
      if (raster_type() == TestRasterType::kGpu) {
        raster_caps.tile_format =
            viz::PlatformColor::BestSupportedRenderBufferFormat(
                compositor_context_provider->ContextCapabilities());
      } else {
        raster_caps.tile_format =
            viz::PlatformColor::BestSupportedTextureFormat(
                compositor_context_provider->ContextCapabilities());
      }
    }
  }

  switch (raster_type()) {
    case TestRasterType::kBitmap:
      EXPECT_FALSE(compositor_context_provider);
      EXPECT_TRUE(use_software_renderer());

      return std::make_unique<BitmapRasterBufferProvider>(
          host_impl->layer_tree_frame_sink());
    case TestRasterType::kGpu:
      EXPECT_TRUE(compositor_context_provider);
      EXPECT_TRUE(worker_context_provider);
      EXPECT_FALSE(use_software_renderer());

      raster_caps.use_gpu_rasterization = true;
      return std::make_unique<GpuRasterBufferProvider>(
          compositor_context_provider, worker_context_provider, raster_caps,
          gfx::Size(), true, host_impl->GetRasterQueryQueueForTesting());
    case TestRasterType::kZeroCopy:
      EXPECT_TRUE(compositor_context_provider);
      EXPECT_FALSE(use_software_renderer());

      return std::make_unique<ZeroCopyRasterBufferProvider>(
          compositor_context_provider, raster_caps);
    case TestRasterType::kOneCopy:
      EXPECT_TRUE(compositor_context_provider);
      EXPECT_TRUE(worker_context_provider);
      EXPECT_FALSE(use_software_renderer());

      return std::make_unique<OneCopyRasterBufferProvider>(
          task_runner, compositor_context_provider, worker_context_provider,
          max_bytes_per_copy_operation, false,
          max_staging_buffer_usage_in_bytes, raster_caps);
  }
}

void LayerTreeHostPixelResourceTest::RunPixelResourceTest(
    scoped_refptr<Layer> content_root,
    base::FilePath file_name) {
  RunPixelTest(content_root, file_name);
}

void LayerTreeHostPixelResourceTest::RunPixelResourceTest(
    scoped_refptr<Layer> content_root,
    const SkBitmap& expected_bitmap) {
  RunPixelTest(content_root, expected_bitmap);
}

void LayerTreeHostPixelResourceTest::RunPixelResourceTestWithLayerList(
    base::FilePath file_name) {
  RunPixelTestWithLayerList(file_name);
}

ParameterizedPixelResourceTest::ParameterizedPixelResourceTest()
    : LayerTreeHostPixelResourceTest(GetParam()) {}

}  // namespace cc
