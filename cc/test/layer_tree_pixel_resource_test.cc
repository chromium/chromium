// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/test/layer_tree_pixel_resource_test.h"

#include "base/task/single_thread_task_runner.h"
#include "cc/layers/layer.h"
#include "cc/raster/gpu_raster_buffer_provider.h"
#include "cc/raster/one_copy_raster_buffer_provider.h"
#include "cc/raster/raster_buffer_provider.h"
#include "cc/raster/zero_copy_raster_buffer_provider.h"
#include "cc/resources/resource_pool.h"
#include "cc/trees/layer_tree_frame_sink.h"
#include "gpu/GLES2/gl2extchromium.h"
#include "gpu/ipc/client/client_shared_image_interface.h"

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
  int max_staging_buffer_usage_in_bytes = 32 * 1024 * 1024;

  switch (raster_type()) {
    case TestRasterType::kBitmap:
      EXPECT_FALSE(compositor_context_provider);
      EXPECT_TRUE(use_software_renderer());

      return std::make_unique<ZeroCopyRasterBufferProvider>(
          host_impl->layer_tree_frame_sink()->shared_image_interface(),
          /*is_software=*/true);
    case TestRasterType::kGpu:
      EXPECT_TRUE(compositor_context_provider);
      EXPECT_TRUE(worker_context_provider);
      EXPECT_FALSE(use_software_renderer());

      return std::make_unique<GpuRasterBufferProvider>(
          worker_context_provider->SharedImageInterface(),
          compositor_context_provider, worker_context_provider,
          /*is_overlay_candidate=*/false, gfx::Size(),
          host_impl->GetRasterQueryQueueForTesting());
    case TestRasterType::kZeroCopy:
      EXPECT_TRUE(compositor_context_provider);
      EXPECT_FALSE(use_software_renderer());

      return std::make_unique<ZeroCopyRasterBufferProvider>(
          compositor_context_provider->SharedImageInterface(),
          /*is_software=*/false);
    case TestRasterType::kOneCopy:
      EXPECT_TRUE(compositor_context_provider);
      EXPECT_TRUE(worker_context_provider);
      EXPECT_FALSE(use_software_renderer());

      return std::make_unique<OneCopyRasterBufferProvider>(
          worker_context_provider->SharedImageInterface(), task_runner,
          compositor_context_provider, worker_context_provider, false,
          max_staging_buffer_usage_in_bytes,
          /*is_overlay_candidate=*/false);
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
