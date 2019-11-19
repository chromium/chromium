// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/test/layer_tree_pixel_resource_test.h"

#include "base/single_thread_task_runner.h"
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
    PixelResourceTestCase test_case) {
  InitializeFromTestCase(test_case);
}

LayerTreeHostPixelResourceTest::LayerTreeHostPixelResourceTest() = default;

void LayerTreeHostPixelResourceTest::InitializeFromTestCase(
    PixelResourceTestCase test_case) {
  DCHECK(!initialized_);
  test_case_ = test_case;
  initialized_ = true;
}

const char* LayerTreeHostPixelResourceTest::GetRendererSuffix() const {
  switch (renderer_type()) {
    case RENDERER_GL:
      return "gl";
    case RENDERER_SKIA_GL:
      return "skia_gl";
    case RENDERER_SKIA_VK:
      return "skia_vk";
    case RENDERER_SOFTWARE:
      return "sw";
  }
}

void LayerTreeHostPixelResourceTest::InitializeSettings(
    LayerTreeSettings* settings) {
  LayerTreePixelTest::InitializeSettings(settings);
  if (raster_type() != GPU) {
    settings->gpu_rasterization_disabled = true;
    settings->gpu_rasterization_forced = false;
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
  DCHECK(initialized_);

  LayerTreeFrameSink* layer_tree_frame_sink =
      host_impl->layer_tree_frame_sink();
  viz::ContextProvider* compositor_context_provider =
      layer_tree_frame_sink->context_provider();
  viz::RasterContextProvider* worker_context_provider =
      layer_tree_frame_sink->worker_context_provider();
  gpu::GpuMemoryBufferManager* gpu_memory_buffer_manager =
      layer_tree_frame_sink->gpu_memory_buffer_manager();
  int max_bytes_per_copy_operation = 1024 * 1024;
  int max_staging_buffer_usage_in_bytes = 32 * 1024 * 1024;

  viz::ResourceFormat gpu_raster_format;
  viz::ResourceFormat sw_raster_format;
  if (compositor_context_provider) {
    if (host_impl->settings().use_rgba_4444) {
      gpu_raster_format = sw_raster_format = viz::RGBA_4444;
    } else {
      gpu_raster_format = viz::PlatformColor::BestSupportedRenderBufferFormat(
          compositor_context_provider->ContextCapabilities());
      sw_raster_format = viz::PlatformColor::BestSupportedTextureFormat(
          compositor_context_provider->ContextCapabilities());
    }
  }
  switch (raster_type()) {
    case SOFTWARE:
      EXPECT_FALSE(compositor_context_provider);
      EXPECT_EQ(RENDERER_SOFTWARE, renderer_type());

      return std::make_unique<BitmapRasterBufferProvider>(
          host_impl->layer_tree_frame_sink());
    case GPU: {
      EXPECT_TRUE(compositor_context_provider);
      EXPECT_TRUE(worker_context_provider);
      EXPECT_NE(RENDERER_SOFTWARE, renderer_type());
      bool enable_oopr = renderer_type() == RENDERER_SKIA_VK;
      return std::make_unique<GpuRasterBufferProvider>(
          compositor_context_provider, worker_context_provider, false,
          gpu_raster_format, gfx::Size(), true, enable_oopr);
    }
    case ZERO_COPY:
      EXPECT_TRUE(compositor_context_provider);
      EXPECT_TRUE(gpu_memory_buffer_manager);
      EXPECT_NE(RENDERER_SOFTWARE, renderer_type());

      return std::make_unique<ZeroCopyRasterBufferProvider>(
          gpu_memory_buffer_manager, compositor_context_provider,
          sw_raster_format);
    case ONE_COPY:
      EXPECT_TRUE(compositor_context_provider);
      EXPECT_TRUE(worker_context_provider);
      EXPECT_NE(RENDERER_SOFTWARE, renderer_type());

      return std::make_unique<OneCopyRasterBufferProvider>(
          task_runner, compositor_context_provider, worker_context_provider,
          gpu_memory_buffer_manager, max_bytes_per_copy_operation, false, false,
          max_staging_buffer_usage_in_bytes, sw_raster_format);
  }
}

void LayerTreeHostPixelResourceTest::RunPixelResourceTest(
    scoped_refptr<Layer> content_root,
    base::FilePath file_name) {
  RunPixelTest(renderer_type(), content_root, file_name);
}

void LayerTreeHostPixelResourceTest::RunPixelResourceTest(
    scoped_refptr<Layer> content_root,
    const SkBitmap& expected_bitmap) {
  RunPixelTest(renderer_type(), content_root, expected_bitmap);
}

void LayerTreeHostPixelResourceTest::RunPixelResourceTestWithLayerList(
    base::FilePath file_name) {
  RunPixelTestWithLayerList(renderer_type(), file_name);
}

ParameterizedPixelResourceTest::ParameterizedPixelResourceTest()
    : LayerTreeHostPixelResourceTest(GetParam()) {}

}  // namespace cc
