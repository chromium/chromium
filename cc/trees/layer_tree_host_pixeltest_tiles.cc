// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <stddef.h>

#include "build/build_config.h"
#include "build/chromeos_buildflags.h"
#include "cc/layers/content_layer_client.h"
#include "cc/layers/picture_layer.h"
#include "cc/paint/display_item_list.h"
#include "cc/paint/paint_flags.h"
#include "cc/paint/paint_op_buffer.h"
#include "cc/test/layer_tree_pixel_test.h"
#include "cc/test/pixel_comparator.h"
#include "cc/test/test_layer_tree_frame_sink.h"
#include "components/viz/common/frame_sinks/copy_output_request.h"
#include "components/viz/test/buildflags.h"
#include "gpu/command_buffer/client/raster_interface.h"

#if !BUILDFLAG(IS_ANDROID)

namespace cc {
namespace {

class LayerTreeHostTilesPixelTest
    : public LayerTreePixelTest,
      public ::testing::WithParamInterface<RasterTestConfig> {
 protected:
  LayerTreeHostTilesPixelTest() : LayerTreePixelTest(renderer_type()) {
    set_raster_type(GetParam().raster_type);
  }

  viz::RendererType renderer_type() const { return GetParam().renderer_type; }

  void InitializeSettings(LayerTreeSettings* settings) override {
    LayerTreePixelTest::InitializeSettings(settings);

    settings->use_partial_raster = use_partial_raster_;
  }

  void BeginTest() override {
    // Don't set up a readback target at the start of the test.
    PostSetNeedsCommitToMainThread();
  }

  void DoReadback() {
    Layer* target = readback_target_ ? readback_target_.get()
                                     : layer_tree_host()->root_layer();
    target->RequestCopyOfOutput(CreateCopyOutputRequest());
  }

  void WillPrepareTilesOnThread(LayerTreeHostImpl* host_impl) override {
    // Issue a GL finish before preparing tiles to ensure resources become
    // available for use in a timely manner. Needed for the one-copy path.
    viz::RasterContextProvider* context_provider =
        host_impl->layer_tree_frame_sink()->worker_context_provider();
    if (!context_provider)
      return;

    viz::RasterContextProvider::ScopedRasterContextLock lock(context_provider);
    lock.RasterInterface()->Finish();
  }

  base::FilePath ref_file_;
  std::unique_ptr<SkBitmap> result_bitmap_;
  bool use_partial_raster_ = false;
};

class BlueYellowClient : public ContentLayerClient {
 public:
  explicit BlueYellowClient(const gfx::Size& size)
      : size_(size), blue_top_(true) {}

  scoped_refptr<DisplayItemList> PaintContentsToDisplayList() override {
    auto display_list = base::MakeRefCounted<DisplayItemList>();

    display_list->StartPaint();

    gfx::Rect top(0, 0, size_.width(), size_.height() / 2);
    gfx::Rect bottom(0, size_.height() / 2, size_.width(), size_.height() / 2);

    gfx::Rect blue_rect = blue_top_ ? top : bottom;
    gfx::Rect yellow_rect = blue_top_ ? bottom : top;

    PaintFlags flags;
    flags.setStyle(PaintFlags::kFill_Style);

    // Use custom colors with 0xF2 rather than the default blue/yellow (which
    // use 0xFF), as the default won't show dither patterns as it exactly maps
    // to a 16-bit color.
    flags.setColor(SkColorSetRGB(0x00, 0x00, 0xF2));
    display_list->push<DrawRectOp>(gfx::RectToSkRect(blue_rect), flags);
    flags.setColor(SkColorSetRGB(0xF2, 0xF2, 0x00));
    display_list->push<DrawRectOp>(gfx::RectToSkRect(yellow_rect), flags);

    display_list->EndPaintOfUnpaired(gfx::Rect(size_));
    display_list->Finalize();
    return display_list;
  }

  bool FillsBoundsCompletely() const override { return true; }

  void set_blue_top(bool b) { blue_top_ = b; }

 private:
  gfx::Size size_;
  bool blue_top_;
};

class LayerTreeHostTilesTestPartialInvalidation
    : public LayerTreeHostTilesPixelTest {
 public:
  LayerTreeHostTilesTestPartialInvalidation()
      : client_(gfx::Size(200, 200)),
        picture_layer_(PictureLayer::Create(&client_)) {
    picture_layer_->SetBounds(gfx::Size(200, 200));
    picture_layer_->SetIsDrawable(true);
  }

  void DidCommitAndDrawFrame() override {
    switch (layer_tree_host()->SourceFrameNumber()) {
      case 1:
        // We have done one frame, but the resource may not be available for
        // partial raster yet. Force a second frame.
        picture_layer_->SetNeedsDisplayRect(gfx::Rect(50, 50, 100, 100));
        break;
      case 2:
        // We have done two frames, so the layer's content has been rastered
        // twice and the first frame's resource is available for partial
        // raster. Now we change the picture behind it to record something
        // completely different, but we give a smaller invalidation rect. The
        // layer should only re-raster the stuff in the rect. If it doesn't do
        // partial raster it would re-raster the whole thing instead.
        client_.set_blue_top(false);
        picture_layer_->SetNeedsDisplayRect(gfx::Rect(50, 50, 100, 100));

        // Add a copy request to see what happened!
        DoReadback();
        break;
    }
  }

 protected:
  BlueYellowClient client_;
  scoped_refptr<PictureLayer> picture_layer_;
};

class PrimaryColorClient : public ContentLayerClient {
 public:
  explicit PrimaryColorClient(const gfx::Size& size) : size_(size) {}

  scoped_refptr<DisplayItemList> PaintContentsToDisplayList() override {
    // When painted, the DisplayItemList should produce blocks of red, green,
    // and blue to test primary color reproduction.
    auto display_list = base::MakeRefCounted<DisplayItemList>();

    display_list->StartPaint();

    int w = size_.width() / 3;
    gfx::Rect red_rect(0, 0, w, size_.height());
    gfx::Rect green_rect(w, 0, w, size_.height());
    gfx::Rect blue_rect(w * 2, 0, w, size_.height());

    PaintFlags flags;
    flags.setStyle(PaintFlags::kFill_Style);

    flags.setColor(SK_ColorRED);
    display_list->push<DrawRectOp>(gfx::RectToSkRect(red_rect), flags);
    flags.setColor(SK_ColorGREEN);
    display_list->push<DrawRectOp>(gfx::RectToSkRect(green_rect), flags);
    flags.setColor(SK_ColorBLUE);
    display_list->push<DrawRectOp>(gfx::RectToSkRect(blue_rect), flags);

    display_list->EndPaintOfUnpaired(gfx::Rect(size_));
    display_list->Finalize();
    return display_list;
  }

  bool FillsBoundsCompletely() const override { return true; }

 private:
  gfx::Size size_;
};

class LayerTreeHostTilesTestRasterColorSpace
    : public LayerTreeHostTilesPixelTest {
 public:
  LayerTreeHostTilesTestRasterColorSpace()
      : client_(gfx::Size(150, 150)),
        picture_layer_(PictureLayer::Create(&client_)) {
    picture_layer_->SetBounds(gfx::Size(150, 150));
    picture_layer_->SetIsDrawable(true);
  }

  void SetColorSpace(const gfx::ColorSpace& color_space) {
    color_space_ = color_space;
  }

  void WillBeginTest() override {
    LayerTreeHostTilesPixelTest::WillBeginTest();
    DCHECK(color_space_.IsValid());
    layer_tree_host()->SetDisplayColorSpaces(
        gfx::DisplayColorSpaces(color_space_));
  }

  void DidCommitAndDrawFrame() override {
    if (layer_tree_host()->SourceFrameNumber() == 1) {
      DoReadback();
    }
  }

 protected:
  PrimaryColorClient client_;
  scoped_refptr<PictureLayer> picture_layer_;
  gfx::ColorSpace color_space_;
};

std::vector<RasterTestConfig> const kTestCases = {
    {viz::RendererType::kSoftware, TestRasterType::kBitmap},
#if BUILDFLAG(ENABLE_GL_BACKEND_TESTS)
    {viz::RendererType::kSkiaGL, TestRasterType::kOneCopy},
    {viz::RendererType::kSkiaGL, TestRasterType::kGpu},
#endif  // BUILDFLAG(ENABLE_GL_BACKEND_TESTS)
#if BUILDFLAG(ENABLE_VULKAN_BACKEND_TESTS)
    {viz::RendererType::kSkiaVk, TestRasterType::kGpu},
#endif  // BUILDFLAG(ENABLE_VULKAN_BACKEND_TESTS)
#if BUILDFLAG(ENABLE_SKIA_GRAPHITE_TESTS)
    {viz::RendererType::kSkiaGraphiteDawn, TestRasterType::kGpu},
#if BUILDFLAG(IS_IOS)
    {viz::RendererType::kSkiaGraphiteMetal, TestRasterType::kGpu},
#endif  // BUILDFLAG(IS_IOS)
#endif  // BUILDFLAG(ENABLE_SKIA_GRAPHITE_TESTS)
};

INSTANTIATE_TEST_SUITE_P(All,
                         LayerTreeHostTilesTestPartialInvalidation,
                         ::testing::ValuesIn(kTestCases),
                         ::testing::PrintToStringParamName());

#if BUILDFLAG(IS_CHROMEOS_ASH) || defined(MEMORY_SANITIZER) || \
    defined(ADDRESS_SANITIZER)
// TODO(crbug.com/40116070): Flakes on all slower bots.
#define MAYBE_PartialRaster DISABLED_PartialRaster
#else
#define MAYBE_PartialRaster PartialRaster
#endif
TEST_P(LayerTreeHostTilesTestPartialInvalidation, MAYBE_PartialRaster) {
  use_partial_raster_ = true;
  base::FilePath expected_result =
      base::FilePath(FILE_PATH_LITERAL("blue_yellow_partial_flipped.png"));
  if (use_skia_graphite()) {
    expected_result = expected_result.InsertBeforeExtensionASCII("_graphite");
  }
  RunSingleThreadedPixelTest(picture_layer_, expected_result);
}
#undef MAYBE_PartialRaster

TEST_P(LayerTreeHostTilesTestPartialInvalidation, FullRaster) {
  base::FilePath expected_result =
      base::FilePath(FILE_PATH_LITERAL("blue_yellow_flipped.png"));
  if (use_skia_graphite()) {
    expected_result = expected_result.InsertBeforeExtensionASCII("_graphite");
  }
  RunSingleThreadedPixelTest(picture_layer_, expected_result);
}

std::vector<RasterTestConfig> const kTestCasesMultiThread = {
#if BUILDFLAG(ENABLE_GL_BACKEND_TESTS)
    {viz::RendererType::kSkiaGL, TestRasterType::kOneCopy},
#endif  // BUILDFLAG(ENABLE_GL_BACKEND_TESTS)
#if BUILDFLAG(ENABLE_VULKAN_BACKEND_TESTS)
    // TODO(rivr): Switch this to one copy raster once is is supported for
    // Vulkan in these tests.
    {viz::RendererType::kSkiaVk, TestRasterType::kGpu},
#endif  // BUILDFLAG(ENABLE_VULKAN_BACKEND_TESTS)
#if BUILDFLAG(ENABLE_SKIA_GRAPHITE_TESTS)
    {viz::RendererType::kSkiaGraphiteDawn, TestRasterType::kGpu},
#if BUILDFLAG(IS_IOS)
    {viz::RendererType::kSkiaGraphiteMetal, TestRasterType::kGpu},
#endif  // BUILDFLAG(IS_IOS)
#endif  // BUILDFLAG(ENABLE_SKIA_GRAPHITE_TESTS)
};

using LayerTreeHostTilesTestPartialInvalidationMultiThread =
    LayerTreeHostTilesTestPartialInvalidation;

INSTANTIATE_TEST_SUITE_P(All,
                         LayerTreeHostTilesTestPartialInvalidationMultiThread,
                         ::testing::ValuesIn(kTestCasesMultiThread),
                         ::testing::PrintToStringParamName());

// kTestCasesMultiThread is empty on some platforms.
GTEST_ALLOW_UNINSTANTIATED_PARAMETERIZED_TEST(
    LayerTreeHostTilesTestPartialInvalidationMultiThread);

#if (BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_CHROMEOS)) && defined(THREAD_SANITIZER)
// Flaky on Linux TSAN. https://crbug.com/707711
#define MAYBE_PartialRaster DISABLED_PartialRaster
#elif BUILDFLAG(IS_CHROMEOS_ASH) || defined(MEMORY_SANITIZER) || \
    defined(ADDRESS_SANITIZER)
// TODO(crbug.com/40116070): Flakes on all slower bots.
#define MAYBE_PartialRaster DISABLED_PartialRaster
#else
#define MAYBE_PartialRaster PartialRaster
#endif
TEST_P(LayerTreeHostTilesTestPartialInvalidationMultiThread,
       MAYBE_PartialRaster) {
  use_partial_raster_ = true;
  base::FilePath expected_result =
      base::FilePath(FILE_PATH_LITERAL("blue_yellow_partial_flipped.png"));
  if (use_skia_graphite()) {
    expected_result = expected_result.InsertBeforeExtensionASCII("_graphite");
  }
  RunPixelTest(picture_layer_, expected_result);
}
#undef MAYBE_PartialRaster

TEST_P(LayerTreeHostTilesTestPartialInvalidationMultiThread, FullRaster) {
  base::FilePath expected_result =
      base::FilePath(FILE_PATH_LITERAL("blue_yellow_flipped.png"));
  if (use_skia_graphite()) {
    expected_result = expected_result.InsertBeforeExtensionASCII("_graphite");
  }
  RunPixelTest(picture_layer_, expected_result);
}

INSTANTIATE_TEST_SUITE_P(All,
                         LayerTreeHostTilesTestRasterColorSpace,
                         ::testing::ValuesIn(kTestCases),
                         ::testing::PrintToStringParamName());

// These tests verify that no artifacts are introduced when the color space for
// rasterization doesn't contain the primary colors of sRGB.
// See crbug.com/1073962 for more details.
TEST_P(LayerTreeHostTilesTestRasterColorSpace, sRGB) {
  SetColorSpace(gfx::ColorSpace::CreateSRGB());

  RunPixelTest(picture_layer_,
               base::FilePath(FILE_PATH_LITERAL("primary_colors.png")));
}

TEST_P(LayerTreeHostTilesTestRasterColorSpace, GenericRGB) {
#if BUILDFLAG(IS_IOS)
  pixel_comparator_ = std::make_unique<FuzzyPixelOffByOneComparator>();
#endif
  SetColorSpace(gfx::ColorSpace(gfx::ColorSpace::PrimaryID::APPLE_GENERIC_RGB,
                                gfx::ColorSpace::TransferID::GAMMA18));

  // Software rasterizer ignores XYZD50 matrix
  const auto* target_png =
      renderer_type() == viz::RendererType::kSoftware
          ? FILE_PATH_LITERAL("primary_colors.png")
          : FILE_PATH_LITERAL("primary_colors_sRGB_in_AdobeRGB.png");
  RunPixelTest(picture_layer_, base::FilePath(target_png));
}

TEST_P(LayerTreeHostTilesTestRasterColorSpace, CustomColorSpace) {
#if BUILDFLAG(IS_FUCHSIA)
  pixel_comparator_ = std::make_unique<FuzzyPixelOffByOneComparator>();
#endif
  // Create a color space with a different blue point.
  SkColorSpacePrimaries primaries;
  skcms_Matrix3x3 to_XYZD50;
  primaries.fRX = 0.640f;
  primaries.fRY = 0.330f;
  primaries.fGX = 0.300f;
  primaries.fGY = 0.600f;
  primaries.fBX = 0.130f;
  primaries.fBY = 0.080f;
  primaries.fWX = 0.3127f;
  primaries.fWY = 0.3290f;
  primaries.toXYZD50(&to_XYZD50);
  SetColorSpace(gfx::ColorSpace::CreateCustom(
      to_XYZD50, gfx::ColorSpace::TransferID::SRGB));

  // Software rasterizer ignores XYZD50 matrix
  const auto* target_png = renderer_type() == viz::RendererType::kSoftware
                               ? FILE_PATH_LITERAL("primary_colors.png")
                               : FILE_PATH_LITERAL("primary_colors_icced.png");
  RunPixelTest(picture_layer_, base::FilePath(target_png));
}

// This test doesn't work on Vulkan because on our hardware we can't render to
// RGBA4444 format using either SwiftShader or native Vulkan. See
// crbug.com/987278 for details.
// TODO(crbug.com/40042400) : Re-enable after this is supported for OOPR.
#if BUILDFLAG(ENABLE_GL_BACKEND_TESTS)
class LayerTreeHostTilesTestPartialInvalidationLowBitDepth
    : public LayerTreeHostTilesTestPartialInvalidation {
 protected:
  void InitializeSettings(LayerTreeSettings* settings) override {
    LayerTreeHostTilesPixelTest::InitializeSettings(settings);
    settings->use_rgba_4444 = true;
    settings->unpremultiply_and_dither_low_bit_depth_tiles = true;
  }
};

INSTANTIATE_TEST_SUITE_P(All,
                         LayerTreeHostTilesTestPartialInvalidationLowBitDepth,
                         ::testing::Values(RasterTestConfig{
                             viz::RendererType::kSkiaGL, TestRasterType::kGpu}),
                         ::testing::PrintToStringParamName());

TEST_P(LayerTreeHostTilesTestPartialInvalidationLowBitDepth,
       DISABLED_PartialRaster) {
  use_partial_raster_ = true;
  RunSingleThreadedPixelTest(picture_layer_,
                             base::FilePath(FILE_PATH_LITERAL(
                                 "blue_yellow_partial_flipped_dither.png")));
}

TEST_P(LayerTreeHostTilesTestPartialInvalidationLowBitDepth,
       DISABLED_FullRaster) {
  RunSingleThreadedPixelTest(
      picture_layer_,
      base::FilePath(FILE_PATH_LITERAL("blue_yellow_flipped_dither.png")));
}
#endif  // BUILDFLAG(ENABLE_GL_BACKEND_TESTS)

}  // namespace
}  // namespace cc

#endif  // !BUILDFLAG(IS_ANDROID)
