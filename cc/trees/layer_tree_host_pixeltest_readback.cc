// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/bind.h"
#include "build/build_config.h"
#include "cc/layers/solid_color_layer.h"
#include "cc/layers/texture_layer.h"
#include "cc/test/fake_picture_layer.h"
#include "cc/test/fake_picture_layer_impl.h"
#include "cc/test/layer_tree_pixel_test.h"
#include "cc/test/solid_color_content_layer_client.h"
#include "cc/test/test_layer_tree_frame_sink.h"
#include "cc/trees/layer_tree_impl.h"
#include "components/viz/common/frame_sinks/copy_output_request.h"
#include "components/viz/common/frame_sinks/copy_output_result.h"
#include "components/viz/test/paths.h"

#if !defined(OS_ANDROID)

namespace cc {
namespace {

// Can't templatize a class on its own members, so ReadbackType and
// ReadbackTestConfig are declared here, before LayerTreeHostReadbackPixelTest.
enum ReadbackType {
  READBACK_TEXTURE,
  READBACK_BITMAP,
};

struct ReadbackTestConfig {
  LayerTreeTest::RendererType renderer_type;
  ReadbackType readback_type;
};

class LayerTreeHostReadbackPixelTest
    : public LayerTreePixelTest,
      public testing::WithParamInterface<ReadbackTestConfig> {
 protected:
  LayerTreeHostReadbackPixelTest()
      : insert_copy_request_after_frame_count_(0) {}

  RendererType renderer_type() const { return GetParam().renderer_type; }

  ReadbackType readback_type() const { return GetParam().readback_type; }

  std::unique_ptr<viz::CopyOutputRequest> CreateCopyOutputRequest() override {
    std::unique_ptr<viz::CopyOutputRequest> request;

    if (readback_type() == READBACK_BITMAP) {
      request = std::make_unique<viz::CopyOutputRequest>(
          viz::CopyOutputRequest::ResultFormat::RGBA_BITMAP,
          base::BindOnce(
              &LayerTreeHostReadbackPixelTest::ReadbackResultAsBitmap,
              base::Unretained(this)));
    } else {
      DCHECK_NE(renderer_type_, RENDERER_SOFTWARE);
      request = std::make_unique<viz::CopyOutputRequest>(
          viz::CopyOutputRequest::ResultFormat::RGBA_TEXTURE,
          base::BindOnce(
              &LayerTreeHostReadbackPixelTest::ReadbackResultAsTexture,
              base::Unretained(this)));
    }

    if (!copy_subrect_.IsEmpty())
      request->set_area(copy_subrect_);
    return request;
  }

  void BeginTest() override {
    if (insert_copy_request_after_frame_count_ == 0) {
      Layer* const target =
          readback_target_ ? readback_target_ : layer_tree_host()->root_layer();
      target->RequestCopyOfOutput(CreateCopyOutputRequest());
    }
    PostSetNeedsCommitToMainThread();
  }

  void DidCommitAndDrawFrame() override {
    if (insert_copy_request_after_frame_count_ ==
        layer_tree_host()->SourceFrameNumber()) {
      Layer* const target =
          readback_target_ ? readback_target_ : layer_tree_host()->root_layer();
      target->RequestCopyOfOutput(CreateCopyOutputRequest());
    }
  }

  void ReadbackResultAsBitmap(std::unique_ptr<viz::CopyOutputResult> result) {
    EXPECT_TRUE(task_runner_provider()->IsMainThread());
    EXPECT_FALSE(result->IsEmpty());
    result_bitmap_ = std::make_unique<SkBitmap>(result->AsSkBitmap());
    EXPECT_TRUE(result_bitmap_->readyToDraw());
    EndTest();
  }

  void ReadbackResultAsTexture(std::unique_ptr<viz::CopyOutputResult> result) {
    EXPECT_TRUE(task_runner_provider()->IsMainThread());
    ASSERT_EQ(result->format(), viz::CopyOutputResult::Format::RGBA_TEXTURE);

    gpu::Mailbox mailbox = result->GetTextureResult()->mailbox;
    gpu::SyncToken sync_token = result->GetTextureResult()->sync_token;
    gfx::ColorSpace color_space = result->GetTextureResult()->color_space;
    EXPECT_EQ(result->GetTextureResult()->color_space, output_color_space_);
    std::unique_ptr<viz::SingleReleaseCallback> release_callback =
        result->TakeTextureOwnership();

    const SkBitmap bitmap =
        CopyMailboxToBitmap(result->size(), mailbox, sync_token, color_space);
    release_callback->Run(gpu::SyncToken(), false);

    ReadbackResultAsBitmap(std::make_unique<viz::CopyOutputSkBitmapResult>(
        result->rect(), bitmap));
  }

  gfx::Rect copy_subrect_;
  gfx::ColorSpace output_color_space_ = gfx::ColorSpace::CreateSRGB();
  int insert_copy_request_after_frame_count_;
};

TEST_P(LayerTreeHostReadbackPixelTest, ReadbackRootLayer) {
  scoped_refptr<SolidColorLayer> background =
      CreateSolidColorLayer(gfx::Rect(200, 200), SK_ColorWHITE);

  scoped_refptr<SolidColorLayer> green =
      CreateSolidColorLayer(gfx::Rect(200, 200), SK_ColorGREEN);
  background->AddChild(green);

  RunPixelTest(renderer_type(), background,
               base::FilePath(FILE_PATH_LITERAL("green.png")));
}

TEST_P(LayerTreeHostReadbackPixelTest, ReadbackRootLayerWithChild) {
  scoped_refptr<SolidColorLayer> background =
      CreateSolidColorLayer(gfx::Rect(200, 200), SK_ColorWHITE);

  scoped_refptr<SolidColorLayer> green =
      CreateSolidColorLayer(gfx::Rect(200, 200), SK_ColorGREEN);
  background->AddChild(green);

  scoped_refptr<SolidColorLayer> blue =
      CreateSolidColorLayer(gfx::Rect(150, 150, 50, 50), SK_ColorBLUE);
  green->AddChild(blue);

  RunPixelTest(renderer_type(), background,
               base::FilePath(FILE_PATH_LITERAL("green_with_blue_corner.png")));
}

TEST_P(LayerTreeHostReadbackPixelTest, ReadbackNonRootLayer) {
  scoped_refptr<SolidColorLayer> background =
      CreateSolidColorLayer(gfx::Rect(200, 200), SK_ColorWHITE);

  scoped_refptr<SolidColorLayer> green =
      CreateSolidColorLayer(gfx::Rect(200, 200), SK_ColorGREEN);
  background->AddChild(green);

  RunPixelTestWithReadbackTarget(
      renderer_type(), background, green.get(),
      base::FilePath(FILE_PATH_LITERAL("green.png")));
}

TEST_P(LayerTreeHostReadbackPixelTest, ReadbackSmallNonRootLayer) {
  scoped_refptr<SolidColorLayer> background =
      CreateSolidColorLayer(gfx::Rect(200, 200), SK_ColorWHITE);

  scoped_refptr<SolidColorLayer> green =
      CreateSolidColorLayer(gfx::Rect(100, 100, 100, 100), SK_ColorGREEN);
  background->AddChild(green);

  RunPixelTestWithReadbackTarget(
      renderer_type(), background, green.get(),
      base::FilePath(FILE_PATH_LITERAL("green_small.png")));
}

TEST_P(LayerTreeHostReadbackPixelTest, ReadbackSmallNonRootLayerWithChild) {
  scoped_refptr<SolidColorLayer> background =
      CreateSolidColorLayer(gfx::Rect(200, 200), SK_ColorWHITE);

  scoped_refptr<SolidColorLayer> green =
      CreateSolidColorLayer(gfx::Rect(100, 100, 100, 100), SK_ColorGREEN);
  background->AddChild(green);

  scoped_refptr<SolidColorLayer> blue =
      CreateSolidColorLayer(gfx::Rect(50, 50, 50, 50), SK_ColorBLUE);
  green->AddChild(blue);

  RunPixelTestWithReadbackTarget(
      renderer_type(), background, green.get(),
      base::FilePath(FILE_PATH_LITERAL("green_small_with_blue_corner.png")));
}

using LayerTreeHostReadbackPixelTestMaybeVulkan =
    LayerTreeHostReadbackPixelTest;

TEST_P(LayerTreeHostReadbackPixelTestMaybeVulkan,
       ReadbackSubtreeSurroundsTargetLayer) {
  scoped_refptr<SolidColorLayer> background =
      CreateSolidColorLayer(gfx::Rect(0, 0, 200, 200), SK_ColorWHITE);

  scoped_refptr<SolidColorLayer> target =
      CreateSolidColorLayer(gfx::Rect(100, 100, 100, 100), SK_ColorRED);
  background->AddChild(target);

  scoped_refptr<SolidColorLayer> green =
      CreateSolidColorLayer(gfx::Rect(-100, -100, 300, 300), SK_ColorGREEN);
  target->AddChild(green);

  scoped_refptr<SolidColorLayer> blue =
      CreateSolidColorLayer(gfx::Rect(50, 50, 50, 50), SK_ColorBLUE);
  target->AddChild(blue);

  copy_subrect_ = gfx::Rect(0, 0, 100, 100);
  RunPixelTestWithReadbackTarget(
      renderer_type(), background, target.get(),
      base::FilePath(FILE_PATH_LITERAL("green_small_with_blue_corner.png")));
}

TEST_P(LayerTreeHostReadbackPixelTest,
       ReadbackSubtreeExtendsBeyondTargetLayer) {
  scoped_refptr<SolidColorLayer> background =
      CreateSolidColorLayer(gfx::Rect(0, 0, 200, 200), SK_ColorWHITE);

  scoped_refptr<SolidColorLayer> target =
      CreateSolidColorLayer(gfx::Rect(50, 50, 150, 150), SK_ColorRED);
  background->AddChild(target);

  scoped_refptr<SolidColorLayer> green =
      CreateSolidColorLayer(gfx::Rect(50, 50, 200, 200), SK_ColorGREEN);
  target->AddChild(green);

  scoped_refptr<SolidColorLayer> blue =
      CreateSolidColorLayer(gfx::Rect(100, 100, 50, 50), SK_ColorBLUE);
  target->AddChild(blue);

  copy_subrect_ = gfx::Rect(50, 50, 100, 100);
  RunPixelTestWithReadbackTarget(
      renderer_type(), background, target.get(),
      base::FilePath(FILE_PATH_LITERAL("green_small_with_blue_corner.png")));
}

TEST_P(LayerTreeHostReadbackPixelTest, ReadbackHiddenSubtree) {
  scoped_refptr<SolidColorLayer> background =
      CreateSolidColorLayer(gfx::Rect(200, 200), SK_ColorBLACK);

  scoped_refptr<SolidColorLayer> hidden_target =
      CreateSolidColorLayer(gfx::Rect(200, 200), SK_ColorGREEN);
  hidden_target->SetHideLayerAndSubtree(true);
  background->AddChild(hidden_target);

  scoped_refptr<SolidColorLayer> blue =
      CreateSolidColorLayer(gfx::Rect(150, 150, 50, 50), SK_ColorBLUE);
  hidden_target->AddChild(blue);

  RunPixelTestWithReadbackTarget(
      renderer_type(), background, hidden_target.get(),
      base::FilePath(FILE_PATH_LITERAL("green_with_blue_corner.png")));
}

TEST_P(LayerTreeHostReadbackPixelTest,
       HiddenSubtreeNotVisibleWhenDrawnForReadback) {
  scoped_refptr<SolidColorLayer> background =
      CreateSolidColorLayer(gfx::Rect(200, 200), SK_ColorBLACK);

  scoped_refptr<SolidColorLayer> hidden_target =
      CreateSolidColorLayer(gfx::Rect(200, 200), SK_ColorGREEN);
  hidden_target->SetHideLayerAndSubtree(true);
  background->AddChild(hidden_target);

  scoped_refptr<SolidColorLayer> blue =
      CreateSolidColorLayer(gfx::Rect(150, 150, 50, 50), SK_ColorBLUE);
  hidden_target->AddChild(blue);

  hidden_target->RequestCopyOfOutput(
      viz::CopyOutputRequest::CreateStubForTesting());
  RunPixelTest(renderer_type(), background,
               base::FilePath(FILE_PATH_LITERAL("black.png")));
}

TEST_P(LayerTreeHostReadbackPixelTest, ReadbackSubrect) {
  scoped_refptr<SolidColorLayer> background =
      CreateSolidColorLayer(gfx::Rect(200, 200), SK_ColorWHITE);

  scoped_refptr<SolidColorLayer> green =
      CreateSolidColorLayer(gfx::Rect(200, 200), SK_ColorGREEN);
  background->AddChild(green);

  scoped_refptr<SolidColorLayer> blue =
      CreateSolidColorLayer(gfx::Rect(100, 100, 50, 50), SK_ColorBLUE);
  green->AddChild(blue);

  // Grab the middle of the root layer.
  copy_subrect_ = gfx::Rect(50, 50, 100, 100);

  RunPixelTest(
      renderer_type(), background,
      base::FilePath(FILE_PATH_LITERAL("green_small_with_blue_corner.png")));
}

TEST_P(LayerTreeHostReadbackPixelTest, ReadbackNonRootLayerSubrect) {
  scoped_refptr<SolidColorLayer> background =
      CreateSolidColorLayer(gfx::Rect(200, 200), SK_ColorWHITE);

  scoped_refptr<SolidColorLayer> green =
      CreateSolidColorLayer(gfx::Rect(25, 25, 150, 150), SK_ColorGREEN);
  background->AddChild(green);

  scoped_refptr<SolidColorLayer> blue =
      CreateSolidColorLayer(gfx::Rect(75, 75, 50, 50), SK_ColorBLUE);
  green->AddChild(blue);

  // Grab the middle of the green layer.
  copy_subrect_ = gfx::Rect(25, 25, 100, 100);

  RunPixelTestWithReadbackTarget(
      renderer_type(), background, green.get(),
      base::FilePath(FILE_PATH_LITERAL("green_small_with_blue_corner.png")));
}

TEST_P(LayerTreeHostReadbackPixelTest, ReadbackWhenNoDamage) {
  scoped_refptr<SolidColorLayer> background =
      CreateSolidColorLayer(gfx::Rect(0, 0, 200, 200), SK_ColorWHITE);

  scoped_refptr<SolidColorLayer> parent =
      CreateSolidColorLayer(gfx::Rect(0, 0, 150, 150), SK_ColorRED);
  background->AddChild(parent);

  scoped_refptr<SolidColorLayer> target =
      CreateSolidColorLayer(gfx::Rect(0, 0, 100, 100), SK_ColorGREEN);
  parent->AddChild(target);

  scoped_refptr<SolidColorLayer> blue =
      CreateSolidColorLayer(gfx::Rect(50, 50, 50, 50), SK_ColorBLUE);
  target->AddChild(blue);

  insert_copy_request_after_frame_count_ = 1;
  RunPixelTestWithReadbackTarget(
      renderer_type(), background, target.get(),
      base::FilePath(FILE_PATH_LITERAL("green_small_with_blue_corner.png")));
}

TEST_P(LayerTreeHostReadbackPixelTest, ReadbackOutsideViewportWhenNoDamage) {
  scoped_refptr<SolidColorLayer> background =
      CreateSolidColorLayer(gfx::Rect(0, 0, 200, 200), SK_ColorWHITE);

  scoped_refptr<SolidColorLayer> parent =
      CreateSolidColorLayer(gfx::Rect(0, 0, 200, 200), SK_ColorRED);
  EXPECT_FALSE(parent->masks_to_bounds());
  background->AddChild(parent);

  scoped_refptr<SolidColorLayer> target =
      CreateSolidColorLayer(gfx::Rect(250, 250, 100, 100), SK_ColorGREEN);
  parent->AddChild(target);

  scoped_refptr<SolidColorLayer> blue =
      CreateSolidColorLayer(gfx::Rect(50, 50, 50, 50), SK_ColorBLUE);
  target->AddChild(blue);

  insert_copy_request_after_frame_count_ = 1;
  RunPixelTestWithReadbackTarget(
      renderer_type(), background, target.get(),
      base::FilePath(FILE_PATH_LITERAL("green_small_with_blue_corner.png")));
}

TEST_P(LayerTreeHostReadbackPixelTest, ReadbackNonRootLayerOutsideViewport) {
  scoped_refptr<SolidColorLayer> background =
      CreateSolidColorLayer(gfx::Rect(200, 200), SK_ColorWHITE);

  scoped_refptr<SolidColorLayer> green =
      CreateSolidColorLayer(gfx::Rect(200, 200), SK_ColorGREEN);
  // Only the top left quarter of the layer is inside the viewport, so the
  // blue layer is entirely outside.
  green->SetPosition(gfx::PointF(100.f, 100.f));
  background->AddChild(green);

  scoped_refptr<SolidColorLayer> blue =
      CreateSolidColorLayer(gfx::Rect(150, 150, 50, 50), SK_ColorBLUE);
  green->AddChild(blue);

  RunPixelTestWithReadbackTarget(
      renderer_type(), background, green.get(),
      base::FilePath(FILE_PATH_LITERAL("green_with_blue_corner.png")));
}

TEST_P(LayerTreeHostReadbackPixelTestMaybeVulkan, ReadbackNonRootOrFirstLayer) {
  // This test has 3 render passes with the copy request on the render pass in
  // the middle. This test caught an issue where copy requests on non-root
  // non-first render passes were being treated differently from the first
  // render pass.
  scoped_refptr<SolidColorLayer> background =
      CreateSolidColorLayer(gfx::Rect(200, 200), SK_ColorGREEN);

  scoped_refptr<SolidColorLayer> blue =
      CreateSolidColorLayer(gfx::Rect(150, 150, 50, 50), SK_ColorBLUE);
  blue->RequestCopyOfOutput(viz::CopyOutputRequest::CreateStubForTesting());
  background->AddChild(blue);

  RunPixelTestWithReadbackTarget(
      renderer_type(), background, background.get(),
      base::FilePath(FILE_PATH_LITERAL("green_with_blue_corner.png")));
}

TEST_P(LayerTreeHostReadbackPixelTest, MultipleReadbacksOnLayer) {
  // This test has 2 copy requests on the background layer. One is added in the
  // test body, another is added in RunReadbackTestWithReadbackTarget. For every
  // copy request after the first, state must be restored via a call to
  // UseRenderPass (see http://crbug.com/99393). This test ensures that the
  // renderer correctly handles cases where UseRenderPass is called multiple
  // times for a single layer.
  scoped_refptr<SolidColorLayer> background =
      CreateSolidColorLayer(gfx::Rect(200, 200), SK_ColorGREEN);

  background->RequestCopyOfOutput(
      viz::CopyOutputRequest::CreateStubForTesting());

  RunPixelTestWithReadbackTarget(
      renderer_type(), background, background.get(),
      base::FilePath(FILE_PATH_LITERAL("green.png")));
}

// TODO(crbug.com/971257): Enable these tests for Skia Vulkan using texture
// readback.
ReadbackTestConfig const kTestConfigs[] = {
    ReadbackTestConfig{LayerTreeTest::RENDERER_SOFTWARE, READBACK_BITMAP},
    ReadbackTestConfig{LayerTreeTest::RENDERER_GL, READBACK_TEXTURE},
    ReadbackTestConfig{LayerTreeTest::RENDERER_GL, READBACK_BITMAP},
    ReadbackTestConfig{LayerTreeTest::RENDERER_SKIA_GL, READBACK_TEXTURE},
    ReadbackTestConfig{LayerTreeTest::RENDERER_SKIA_GL, READBACK_BITMAP},
#if defined(ENABLE_CC_VULKAN_TESTS)
    ReadbackTestConfig{LayerTreeTest::RENDERER_SKIA_VK, READBACK_BITMAP},
#endif
};

INSTANTIATE_TEST_SUITE_P(,
                         LayerTreeHostReadbackPixelTest,
                         ::testing::ValuesIn(kTestConfigs));

// TODO(crbug.com/974283): These tests are crashing with vulkan when TSan or
// MSan are used.
ReadbackTestConfig const kMaybeVulkanTestConfigs[] = {
    ReadbackTestConfig{LayerTreeTest::RENDERER_SOFTWARE, READBACK_BITMAP},
    ReadbackTestConfig{LayerTreeTest::RENDERER_GL, READBACK_TEXTURE},
    ReadbackTestConfig{LayerTreeTest::RENDERER_GL, READBACK_BITMAP},
    ReadbackTestConfig{LayerTreeTest::RENDERER_SKIA_GL, READBACK_TEXTURE},
    ReadbackTestConfig{LayerTreeTest::RENDERER_SKIA_GL, READBACK_BITMAP},
#if defined(ENABLE_CC_VULKAN_TESTS) && !defined(THREAD_SANITIZER) && \
    !defined(MEMORY_SANITIZER)
    ReadbackTestConfig{LayerTreeTest::RENDERER_SKIA_VK, READBACK_BITMAP},
#endif
};

INSTANTIATE_TEST_SUITE_P(,
                         LayerTreeHostReadbackPixelTestMaybeVulkan,
                         ::testing::ValuesIn(kMaybeVulkanTestConfigs));

class LayerTreeHostReadbackDeviceScalePixelTest
    : public LayerTreeHostReadbackPixelTest {
 protected:
  LayerTreeHostReadbackDeviceScalePixelTest()
      : device_scale_factor_(1.f),
        white_client_(SK_ColorWHITE, gfx::Size(200, 200)),
        green_client_(SK_ColorGREEN, gfx::Size(200, 200)),
        blue_client_(SK_ColorBLUE, gfx::Size(200, 200)) {}

  void SetupTree() override {
    SetInitialDeviceScaleFactor(device_scale_factor_);
    LayerTreePixelTest::SetupTree();
  }

  void DrawLayersOnThread(LayerTreeHostImpl* host_impl) override {
    EXPECT_EQ(device_scale_factor_,
              host_impl->active_tree()->device_scale_factor());
  }

  float device_scale_factor_;
  SolidColorContentLayerClient white_client_;
  SolidColorContentLayerClient green_client_;
  SolidColorContentLayerClient blue_client_;
};

TEST_P(LayerTreeHostReadbackDeviceScalePixelTest, ReadbackSubrect) {
  scoped_refptr<FakePictureLayer> background =
      FakePictureLayer::Create(&white_client_);
  background->SetBounds(gfx::Size(100, 100));
  background->SetIsDrawable(true);

  scoped_refptr<FakePictureLayer> green =
      FakePictureLayer::Create(&green_client_);
  green->SetBounds(gfx::Size(100, 100));
  green->SetIsDrawable(true);
  background->AddChild(green);

  scoped_refptr<FakePictureLayer> blue =
      FakePictureLayer::Create(&blue_client_);
  blue->SetPosition(gfx::PointF(50.f, 50.f));
  blue->SetBounds(gfx::Size(25, 25));
  blue->SetIsDrawable(true);
  green->AddChild(blue);

  // Grab the middle of the root layer.
  copy_subrect_ = gfx::Rect(25, 25, 50, 50);
  device_scale_factor_ = 2.f;
  RunPixelTest(
      renderer_type(), background,
      base::FilePath(FILE_PATH_LITERAL("green_small_with_blue_corner.png")));
}

TEST_P(LayerTreeHostReadbackDeviceScalePixelTest, ReadbackNonRootLayerSubrect) {
  scoped_refptr<FakePictureLayer> background =
      FakePictureLayer::Create(&white_client_);
  background->SetBounds(gfx::Size(100, 100));
  background->SetIsDrawable(true);

  scoped_refptr<FakePictureLayer> green =
      FakePictureLayer::Create(&green_client_);
  green->SetPosition(gfx::PointF(10.f, 20.f));
  green->SetBounds(gfx::Size(90, 80));
  green->SetIsDrawable(true);
  background->AddChild(green);

  scoped_refptr<FakePictureLayer> blue =
      FakePictureLayer::Create(&blue_client_);
  blue->SetPosition(gfx::PointF(50.f, 50.f));
  blue->SetBounds(gfx::Size(25, 25));
  blue->SetIsDrawable(true);
  green->AddChild(blue);

  // Grab the green layer's content with blue in the bottom right.
  copy_subrect_ = gfx::Rect(25, 25, 50, 50);
  device_scale_factor_ = 2.f;
  RunPixelTestWithReadbackTarget(
      renderer_type(), background, green.get(),
      base::FilePath(FILE_PATH_LITERAL("green_small_with_blue_corner.png")));
}

INSTANTIATE_TEST_SUITE_P(,
                         LayerTreeHostReadbackDeviceScalePixelTest,
                         ::testing::ValuesIn(kTestConfigs));

class LayerTreeHostReadbackColorSpacePixelTest
    : public LayerTreeHostReadbackPixelTest {
 protected:
  LayerTreeHostReadbackColorSpacePixelTest()
      : green_client_(SK_ColorGREEN, gfx::Size(200, 200)) {
    output_color_space_ = gfx::ColorSpace::CreateDisplayP3D65();
  }

  std::unique_ptr<TestLayerTreeFrameSink> CreateLayerTreeFrameSink(
      const viz::RendererSettings& renderer_settings,
      double refresh_rate,
      scoped_refptr<viz::ContextProvider> compositor_context_provider,
      scoped_refptr<viz::RasterContextProvider> worker_context_provider)
      override {
    std::unique_ptr<TestLayerTreeFrameSink> frame_sink =
        LayerTreePixelTest::CreateLayerTreeFrameSink(
            renderer_settings, refresh_rate, compositor_context_provider,
            worker_context_provider);
    frame_sink->SetDisplayColorSpace(output_color_space_);
    return frame_sink;
  }

  SolidColorContentLayerClient green_client_;
};

TEST_P(LayerTreeHostReadbackColorSpacePixelTest, Readback) {
  scoped_refptr<FakePictureLayer> background =
      FakePictureLayer::Create(&green_client_);
  background->SetBounds(gfx::Size(200, 200));
  background->SetIsDrawable(true);

  // The sRGB green should be converted into P3.
  RunPixelTest(renderer_type(), background,
               base::FilePath(FILE_PATH_LITERAL("srgb_green_in_p3.png")));
}

INSTANTIATE_TEST_SUITE_P(,
                         LayerTreeHostReadbackColorSpacePixelTest,
                         ::testing::ValuesIn(kTestConfigs));

}  // namespace
}  // namespace cc

#endif  // OS_ANDROID
