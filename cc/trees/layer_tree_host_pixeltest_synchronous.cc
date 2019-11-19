// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "build/build_config.h"
#include "cc/layers/content_layer_client.h"
#include "cc/layers/picture_image_layer.h"
#include "cc/layers/picture_layer.h"
#include "cc/layers/solid_color_layer.h"
#include "cc/test/fake_content_layer_client.h"
#include "cc/test/layer_tree_pixel_test.h"
#include "cc/test/pixel_comparator.h"

#if !defined(OS_ANDROID)

namespace cc {
namespace {

class LayerTreeHostSynchronousPixelTest
    : public LayerTreePixelTest,
      public ::testing::WithParamInterface<LayerTreeTest::RendererType> {
 protected:
  void InitializeSettings(LayerTreeSettings* settings) override {
    LayerTreePixelTest::InitializeSettings(settings);
    settings->single_thread_proxy_scheduler = false;
    settings->gpu_rasterization_forced = gpu_rasterization_forced_;
    settings->gpu_rasterization_disabled = !settings->gpu_rasterization_forced;
    settings->use_zero_copy = use_zero_copy_;
  }

  LayerTreeTest::RendererType renderer_type() { return GetParam(); }

  void BeginTest() override {
    LayerTreePixelTest::BeginTest();
    PostCompositeImmediatelyToMainThread();
  }

  void DoContentLayerTest() {
    gfx::Size bounds(200, 200);

    FakeContentLayerClient client;
    client.set_bounds(bounds);
    PaintFlags green_flags;
    green_flags.setColor(SkColorSetARGB(255, 0, 255, 0));
    client.add_draw_rect(gfx::Rect(bounds), green_flags);
    scoped_refptr<PictureLayer> root = PictureLayer::Create(&client);
    root->SetBounds(bounds);
    root->SetIsDrawable(true);

    RunSingleThreadedPixelTest(renderer_type(), root,
                               base::FilePath(FILE_PATH_LITERAL("green.png")));
  }

  bool gpu_rasterization_forced_ = false;
  bool use_zero_copy_ = false;
};

LayerTreeTest::RendererType const kRendererTypesGpu[] = {
    LayerTreeTest::RENDERER_GL,
    LayerTreeTest::RENDERER_SKIA_GL,
#if defined(ENABLE_CC_VULKAN_TESTS)
    LayerTreeTest::RENDERER_SKIA_VK,
#endif
};

INSTANTIATE_TEST_SUITE_P(,
                         LayerTreeHostSynchronousPixelTest,
                         ::testing::ValuesIn(kRendererTypesGpu));

TEST_P(LayerTreeHostSynchronousPixelTest, OneContentLayerZeroCopy) {
  use_zero_copy_ = true;
  DoContentLayerTest();
}

TEST_P(LayerTreeHostSynchronousPixelTest, OneContentLayerGpuRasterization) {
  gpu_rasterization_forced_ = true;
  DoContentLayerTest();
}

}  // namespace
}  // namespace cc

#endif  // OS_ANDROID
