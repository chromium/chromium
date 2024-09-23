// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/functional/bind.h"
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
#include "components/viz/test/buildflags.h"
#include "components/viz/test/paths.h"
#include "gpu/command_buffer/client/raster_interface.h"

#if !BUILDFLAG(IS_ANDROID)

namespace cc {
namespace {

// Can't templatize a class on its own members, so TestReadBackType and
// ReadbackTestConfig are declared here, before LayerTreeHostReadbackPixelTest.
enum class TestReadBackType {
  kTexture,
  kBitmap,
};

struct ReadbackTestConfig {
  viz::RendererType renderer_type;
  TestReadBackType readback_type;
};

// Provides a test readback suffix appropriate for |type|.
const char* ReadbackTypeTestSuffix(TestReadBackType type) {
  switch (type) {
    case TestReadBackType::kTexture:
      return "Texture";
    case TestReadBackType::kBitmap:
      return "Bitmap";
  }
}

void PrintTo(const ReadbackTestConfig& config, std::ostream* os) {
  PrintTo(config.renderer_type, os);
  *os << '_' << ReadbackTypeTestSuffix(config.readback_type);
}

class LayerTreeHostReadbackPixelTest
    : public LayerTreePixelTest,
      public testing::WithParamInterface<ReadbackTestConfig> {
 protected:
  LayerTreeHostReadbackPixelTest() : LayerTreePixelTest(renderer_type()) {}

  viz::RendererType renderer_type() const { return GetParam().renderer_type; }

  TestReadBackType readback_type() const { return GetParam().readback_type; }

  std::unique_ptr<viz::CopyOutputRequest> CreateCopyOutputRequest() override {
    std::unique_ptr<viz::CopyOutputRequest> request;

    if (readback_type() == TestReadBackType::kBitmap) {
      request = std::make_unique<viz::CopyOutputRequest>(
          viz::CopyOutputRequest::ResultFormat::RGBA,
          viz::CopyOutputRequest::ResultDestination::kSystemMemory,
          base::BindOnce(
              &LayerTreeHostReadbackPixelTest::ReadbackResultAsBitmap,
              base::Unretained(this)));
    } else {
      DCHECK_NE(renderer_type_, viz::RendererType::kSoftware);
      request = std::make_unique<viz::CopyOutputRequest>(
          viz::CopyOutputRequest::ResultFormat::RGBA,
          viz::CopyOutputRequest::ResultDestination::kNativeTextures,
          base::BindOnce(
              &LayerTreeHostReadbackPixelTest::ReadbackResultAsTexture,
              base::Unretained(this)));
    }

    if (!copy_subrect_.IsEmpty())
      request->set_area(copy_subrect_);
    return request;
  }

  std::unique_ptr<TestLayerTreeFrameSink> CreateLayerTreeFrameSink(
      const viz::RendererSettings& renderer_settings,
      double refresh_rate,
      scoped_refptr<viz::RasterContextProvider> compositor_context_provider,
      scoped_refptr<viz::RasterContextProvider> worker_context_provider)
      override {
    auto frame_sink = LayerTreePixelTest::CreateLayerTreeFrameSink(
        renderer_settings, refresh_rate, std::move(compositor_context_provider),
        std::move(worker_context_provider));
    context_provider_ = frame_sink->worker_context_provider();
    return frame_sink;
  }

  void BeginTest() override {
    if (insert_copy_request_after_frame_count_ == 0) {
      Layer* const target = readback_target_ ? readback_target_.get()
                                             : layer_tree_host()->root_layer();
      target->RequestCopyOfOutput(CreateCopyOutputRequest());
    }
    PostSetNeedsCommitToMainThread();
  }

  void CleanupBeforeDestroy() override {
    // Avoid extending the lifetime of the context.
    context_provider_.reset();
  }

  void DidCommitAndDrawFrame() override {
    if (insert_copy_request_after_frame_count_ ==
        layer_tree_host()->SourceFrameNumber()) {
      Layer* const target = readback_target_ ? readback_target_.get()
                                             : layer_tree_host()->root_layer();
      target->RequestCopyOfOutput(CreateCopyOutputRequest());
    }
  }

  void ReadbackResultAsBitmap(std::unique_ptr<viz::CopyOutputResult> result) {
    EXPECT_TRUE(task_runner_provider()->IsMainThread());
    EXPECT_FALSE(result->IsEmpty());
    auto scoped_sk_bitmap = result->ScopedAccessSkBitmap();
    result_bitmap_ =
        std::make_unique<SkBitmap>(scoped_sk_bitmap.GetOutScopedBitmap());
    EXPECT_TRUE(result_bitmap_->readyToDraw());
    EndTest();
  }

  SkBitmap CopyMailboxToBitmap(const gfx::Size& size,
                               const gpu::Mailbox& mailbox,
                               const gpu::SyncToken& sync_token,
                               const gfx::ColorSpace& color_space) {
    DCHECK(context_provider_);
    viz::RasterContextProvider::ScopedRasterContextLock lock(
        context_provider_.get());
    auto* ri = context_provider_->RasterInterface();

    if (sync_token.HasData()) {
      ri->WaitSyncTokenCHROMIUM(sync_token.GetConstData());
    }

    SkBitmap bitmap;
    bitmap.allocPixels(SkImageInfo::MakeN32Premul(
        size.width(), size.height(), color_space.ToSkColorSpace()));

    ri->ReadbackImagePixels(mailbox, bitmap.info(), bitmap.rowBytes(), 0, 0,
                            /*plane_index=*/0, bitmap.getPixels());
    EXPECT_EQ(ri->GetError(), static_cast<unsigned>(GL_NO_ERROR));

    return bitmap;
  }

  void ReadbackResultAsTexture(std::unique_ptr<viz::CopyOutputResult> result) {
    EXPECT_TRUE(task_runner_provider()->IsMainThread());
    ASSERT_FALSE(result->IsEmpty());
    ASSERT_EQ(result->format(), viz::CopyOutputResult::Format::RGBA);
    ASSERT_EQ(result->destination(),
              viz::CopyOutputResult::Destination::kNativeTextures);

    gpu::Mailbox mailbox = result->GetTextureResult()->mailbox;
    gfx::ColorSpace color_space = result->GetTextureResult()->color_space;
    EXPECT_EQ(color_space, output_color_space_);

    viz::CopyOutputResult::ReleaseCallbacks release_callbacks =
        result->TakeTextureOwnership();
    EXPECT_EQ(1u, release_callbacks.size());

    SkBitmap bitmap = CopyMailboxToBitmap(result->size(), mailbox,
                                          gpu::SyncToken(), color_space);
    std::move(release_callbacks[0]).Run(gpu::SyncToken(), false);

    ReadbackResultAsBitmap(std::make_unique<viz::CopyOutputSkBitmapResult>(
        result->rect(), std::move(bitmap)));
  }

  gfx::Rect copy_subrect_;
  gfx::ColorSpace output_color_space_ = gfx::ColorSpace::CreateSRGB();
  int insert_copy_request_after_frame_count_ = 0;
  scoped_refptr<viz::RasterContextProvider> context_provider_;
};

TEST_P(LayerTreeHostReadbackPixelTest, ReadbackRootLayer) {
  scoped_refptr<SolidColorLayer> background =
      CreateSolidColorLayer(gfx::Rect(200, 200), SK_ColorWHITE);

  scoped_refptr<SolidColorLayer> green =
      CreateSolidColorLayer(gfx::Rect(200, 200), SK_ColorGREEN);
  background->AddChild(green);

  RunPixelTest(background, base::FilePath(FILE_PATH_LITERAL("green.png")));
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

  RunPixelTest(background,
               base::FilePath(FILE_PATH_LITERAL("green_with_blue_corner.png")));
}

TEST_P(LayerTreeHostReadbackPixelTest, ReadbackNonRootLayer) {
  scoped_refptr<SolidColorLayer> background =
      CreateSolidColorLayer(gfx::Rect(200, 200), SK_ColorWHITE);

  scoped_refptr<SolidColorLayer> green =
      CreateSolidColorLayer(gfx::Rect(200, 200), SK_ColorGREEN);
  background->AddChild(green);

  RunPixelTestWithReadbackTarget(
      background, green.get(), base::FilePath(FILE_PATH_LITERAL("green.png")));
}

TEST_P(LayerTreeHostReadbackPixelTest, ReadbackSmallNonRootLayer) {
  scoped_refptr<SolidColorLayer> background =
      CreateSolidColorLayer(gfx::Rect(200, 200), SK_ColorWHITE);

  scoped_refptr<SolidColorLayer> green =
      CreateSolidColorLayer(gfx::Rect(100, 100, 100, 100), SK_ColorGREEN);
  background->AddChild(green);

  RunPixelTestWithReadbackTarget(
      background, green.get(),
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
      background, green.get(),
      base::FilePath(FILE_PATH_LITERAL("green_small_with_blue_corner.png")));
}

TEST_P(LayerTreeHostReadbackPixelTest, ReadbackSubtreeSurroundsTargetLayer) {
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
      background, target.get(),
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
      background, target.get(),
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
      background, hidden_target.get(),
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
  RunPixelTest(background, base::FilePath(FILE_PATH_LITERAL("black.png")));
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
      background,
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
      background, green.get(),
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
      background, target.get(),
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
      background, target.get(),
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
      background, green.get(),
      base::FilePath(FILE_PATH_LITERAL("green_with_blue_corner.png")));
}

TEST_P(LayerTreeHostReadbackPixelTest, ReadbackNonRootOrFirstLayer) {
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
      background, background.get(),
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
      background, background.get(),
      base::FilePath(FILE_PATH_LITERAL("green.png")));
}

// TODO(crbug.com/41463412): Enable these tests for Skia Vulkan using texture
// readback.
ReadbackTestConfig const kTestConfigs[] = {
    ReadbackTestConfig{viz::RendererType::kSoftware, TestReadBackType::kBitmap},
#if BUILDFLAG(ENABLE_GL_BACKEND_TESTS)
    ReadbackTestConfig{viz::RendererType::kSkiaGL, TestReadBackType::kTexture},
    ReadbackTestConfig{viz::RendererType::kSkiaGL, TestReadBackType::kBitmap},
#endif  // BUILDFLAG(ENABLE_GL_BACKEND_TESTS)
#if BUILDFLAG(ENABLE_VULKAN_BACKEND_TESTS)
    ReadbackTestConfig{viz::RendererType::kSkiaVk, TestReadBackType::kBitmap},
    ReadbackTestConfig{viz::RendererType::kSkiaVk, TestReadBackType::kTexture},
#endif  // BUILDFLAG(ENABLE_VULKAN_BACKEND_TESTS)
#if BUILDFLAG(ENABLE_SKIA_GRAPHITE_TESTS)
    ReadbackTestConfig{viz::RendererType::kSkiaGraphiteDawn,
                       TestReadBackType::kBitmap},
#if BUILDFLAG(IS_IOS)
    ReadbackTestConfig{viz::RendererType::kSkiaGraphiteMetal,
                       TestReadBackType::kBitmap},
#endif  // BUILDFLAG(IS_IOS)
#endif  // BUILDFLAG(ENABLE_SKIA_GRAPHITE_TESTS)
};

INSTANTIATE_TEST_SUITE_P(All,
                         LayerTreeHostReadbackPixelTest,
                         ::testing::ValuesIn(kTestConfigs),
                         ::testing::PrintToStringParamName());

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
      background,
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
      background, green.get(),
      base::FilePath(FILE_PATH_LITERAL("green_small_with_blue_corner.png")));
}

INSTANTIATE_TEST_SUITE_P(All,
                         LayerTreeHostReadbackDeviceScalePixelTest,
                         ::testing::ValuesIn(kTestConfigs),
                         ::testing::PrintToStringParamName());

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
      scoped_refptr<viz::RasterContextProvider> compositor_context_provider,
      scoped_refptr<viz::RasterContextProvider> worker_context_provider)
      override {
    std::unique_ptr<TestLayerTreeFrameSink> frame_sink =
        LayerTreePixelTest::CreateLayerTreeFrameSink(
            renderer_settings, refresh_rate, compositor_context_provider,
            worker_context_provider);
    frame_sink->SetDisplayColorSpace(output_color_space_);
    context_provider_ = frame_sink->worker_context_provider();
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
  RunPixelTest(background,
               base::FilePath(FILE_PATH_LITERAL("srgb_green_in_p3.png")));
}

INSTANTIATE_TEST_SUITE_P(All,
                         LayerTreeHostReadbackColorSpacePixelTest,
                         ::testing::ValuesIn(kTestConfigs),
                         ::testing::PrintToStringParamName());

}  // namespace
}  // namespace cc

#endif  // BUILDFLAG(IS_ANDROID)
