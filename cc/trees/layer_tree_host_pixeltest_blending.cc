// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/351564777): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#include <stdint.h>

#include "build/build_config.h"
#include "cc/layers/solid_color_layer.h"
#include "cc/paint/paint_image.h"
#include "cc/paint/paint_image_builder.h"
#include "cc/paint/render_surface_filters.h"
#include "cc/paint/skia_paint_canvas.h"
#include "cc/test/fake_content_layer_client.h"
#include "cc/test/fake_picture_layer.h"
#include "cc/test/layer_tree_pixel_resource_test.h"
#include "cc/test/pixel_comparator.h"
#include "cc/test/test_layer_tree_frame_sink.h"
#include "components/viz/test/buildflags.h"
#include "third_party/skia/include/core/SkImage.h"
#include "third_party/skia/include/core/SkSurface.h"

#if !BUILDFLAG(IS_ANDROID)

namespace cc {
namespace {

SkBlendMode const kBlendModes[] = {
    SkBlendMode::kSrcOver,   SkBlendMode::kScreen,
    SkBlendMode::kOverlay,   SkBlendMode::kDarken,
    SkBlendMode::kLighten,   SkBlendMode::kColorDodge,
    SkBlendMode::kColorBurn, SkBlendMode::kHardLight,
    SkBlendMode::kSoftLight, SkBlendMode::kDifference,
    SkBlendMode::kExclusion, SkBlendMode::kMultiply,
    SkBlendMode::kHue,       SkBlendMode::kSaturation,
    SkBlendMode::kColor,     SkBlendMode::kLuminosity};

SkColor kCSSTestColors[] = {
    0xffff0000,  // red
    0xff00ff00,  // lime
    0xff0000ff,  // blue
    0xff00ffff,  // aqua
    0xffff00ff,  // fuchsia
    0xffffff00,  // yellow
    0xff008000,  // green
    0xff800000,  // maroon
    0xff000080,  // navy
    0xff800080,  // purple
    0xff808000,  // olive
    0xff008080,  // teal
    0xfffa8072,  // salmon
    0xffc0c0c0,  // silver
    0xff000000,  // black
    0xff808080,  // gray
    0x80000000,  // black with transparency
    0xffffffff,  // white
    0x80ffffff,  // white with transparency
    0x00000000   // transparent
};

const int kCSSTestColorsCount = std::size(kCSSTestColors);

using RenderPassOptions = uint32_t;
const uint32_t kUseMasks = 1 << 0;
const uint32_t kUseAntialiasing = 1 << 1;
const uint32_t kUseColorMatrix = 1 << 2;

class LayerTreeHostBlendingPixelTest
    : public LayerTreeHostPixelResourceTest,
      public ::testing::WithParamInterface<
          ::testing::tuple<RasterTestConfig, SkBlendMode>> {
 public:
  LayerTreeHostBlendingPixelTest()
      : LayerTreeHostPixelResourceTest(resource_type()),
        force_antialiasing_(false) {
    pixel_comparator_ =
        std::make_unique<AlphaDiscardingFuzzyPixelOffByOneComparator>();
  }

  RasterTestConfig resource_type() const {
    return ::testing::get<0>(GetParam());
  }
  SkBlendMode current_blend_mode() const {
    return ::testing::get<1>(GetParam());
  }

 protected:
  std::unique_ptr<TestLayerTreeFrameSink> CreateLayerTreeFrameSink(
      const viz::RendererSettings& renderer_settings,
      double refresh_rate,
      scoped_refptr<viz::RasterContextProvider> compositor_context_provider,
      scoped_refptr<viz::RasterContextProvider> worker_context_provider)
      override {
    viz::RendererSettings modified_renderer_settings = renderer_settings;
    modified_renderer_settings.force_antialiasing = force_antialiasing_;
    return LayerTreeHostPixelResourceTest::CreateLayerTreeFrameSink(
        modified_renderer_settings, refresh_rate, compositor_context_provider,
        worker_context_provider);
  }

  sk_sp<SkSurface> CreateColorfulSurface(int width, int height) {
    // Draw the backdrop with horizontal lanes.
    const int kLaneWidth = width;
    const int kLaneHeight = height / kCSSTestColorsCount;
    sk_sp<SkSurface> backing_store =
        SkSurfaces::Raster(SkImageInfo::MakeN32Premul(width, height));
    SkCanvas* canvas = backing_store->getCanvas();
    canvas->clear(SK_ColorTRANSPARENT);
    for (int i = 0; i < kCSSTestColorsCount; ++i) {
      SkPaint paint;
      paint.setColor(kCSSTestColors[i]);
      canvas->drawRect(
          SkRect::MakeXYWH(0, i * kLaneHeight, kLaneWidth, kLaneHeight), paint);
    }
    return backing_store;
  }

  scoped_refptr<Layer> CreateColorfulBackdropLayer(int width, int height) {
    sk_sp<SkSurface> backing_store = CreateColorfulSurface(width, height);
    gfx::Size bounds(width, height);
    backdrop_client_.set_bounds(bounds);
    backdrop_client_.add_draw_image(backing_store->makeImageSnapshot(),
                                    gfx::Point());
    scoped_refptr<FakePictureLayer> layer =
        FakePictureLayer::Create(&backdrop_client_);
    layer->SetIsDrawable(true);
    layer->SetBounds(bounds);
    return layer;
  }

  void SetupMaskLayer(scoped_refptr<Layer> layer) {
    gfx::Size bounds = layer->bounds();

    sk_sp<SkSurface> surface = SkSurfaces::Raster(
        SkImageInfo::MakeN32Premul(bounds.width(), bounds.height()));
    SkCanvas* canvas = surface->getCanvas();
    SkPaint paint;
    paint.setColor(SK_ColorWHITE);
    canvas->clear(SK_ColorTRANSPARENT);
    // This layer is a long skinny layer of size 2, so have the mask
    // cover the right half of it
    canvas->drawRect(
        SkRect::MakeXYWH(1, 0, bounds.width() - 1, bounds.height()), paint);

    mask_client_.set_bounds(bounds);
    mask_client_.add_draw_image(surface->makeImageSnapshot(), gfx::Point());

    scoped_refptr<FakePictureLayer> mask =
        FakePictureLayer::Create(&mask_client_);
    mask->SetIsDrawable(true);
    mask->SetBounds(bounds);
    layer->SetMaskLayer(mask);
  }

  void SetupColorMatrix(scoped_refptr<Layer> layer) {
    FilterOperations filter_operations;
    filter_operations.Append(FilterOperation::CreateSepiaFilter(.001f));
    layer->SetFilters(filter_operations);
  }

  void CreateBlendingColorLayers(int lane_width,
                                 int lane_height,
                                 scoped_refptr<Layer> background,
                                 RenderPassOptions flags) {
    gfx::Rect child_rect(lane_width, lane_height);

    scoped_refptr<SolidColorLayer> lane =
        CreateSolidColorLayer(child_rect, misc_opaque_color_);
    lane->SetBlendMode(current_blend_mode());
    lane->SetForceRenderSurfaceForTesting(true);

    // Layers with kDstIn blend mode with a mask is not supported.
    if (flags & kUseMasks)
      SetupMaskLayer(lane);
    if (flags & kUseColorMatrix) {
      SetupColorMatrix(lane);
    }
    background->AddChild(lane);
  }

  SkBitmap CreateBlendingWithRenderPassExpected(int width,
                                                int height,
                                                RenderPassOptions flags) {
    // Should match RunBlendingWithRenderPass.
    sk_sp<SkSurface> surface = CreateColorfulSurface(width, height);

    SkPaint paint;
    paint.setBlendMode(current_blend_mode());
    paint.setColor(misc_opaque_color_);

    SkRect rect;
    if (flags & kUseMasks) {
      rect = SkRect::MakeXYWH(1, 0, width - 1, height);
    } else {
      rect = SkRect::MakeWH(width, height);
    }
    surface->getCanvas()->drawRect(rect, paint);

    SkBitmap expected;
    expected.allocN32Pixels(width, height);
    SkCanvas canvas(expected, SkSurfaceProps{});
    canvas.clear(SK_ColorWHITE);
    canvas.drawImage(surface->makeImageSnapshot(), 0, 0);

    return expected;
  }

  void RunBlendingWithRenderPass(RenderPassOptions flags) {
    const int kRootWidth = 2;
    const int kRootHeight = kRootWidth * kCSSTestColorsCount;

    SCOPED_TRACE(TestTypeToString());
    SCOPED_TRACE(SkBlendMode_Name(current_blend_mode()));

    scoped_refptr<SolidColorLayer> root = CreateSolidColorLayer(
        gfx::Rect(kRootWidth, kRootHeight), SK_ColorWHITE);
    scoped_refptr<Layer> background =
        CreateColorfulBackdropLayer(kRootWidth, kRootHeight);

    background->SetForceRenderSurfaceForTesting(true);
    root->AddChild(background);

    CreateBlendingColorLayers(kRootWidth, kRootHeight, background.get(), flags);

    force_antialiasing_ = (flags & kUseAntialiasing);

    if (renderer_type_ == viz::RendererType::kSkiaVk) {
      // Blending results might differ with one pixel.
      pixel_comparator_ = std::make_unique<FuzzyPixelComparator>(
          FuzzyPixelComparator()
              .SetErrorPixelsPercentageLimit(35.f)
              .SetAbsErrorLimit(1));
    }

    RunPixelResourceTest(root, CreateBlendingWithRenderPassExpected(
                                   kRootWidth, kRootHeight, flags));
  }

  bool force_antialiasing_;
  FakeContentLayerClient mask_client_;
  FakeContentLayerClient backdrop_client_;
  SkColor misc_opaque_color_ = 0xffc86464;
};

std::vector<RasterTestConfig> const kTestCases = {
    {viz::RendererType::kSoftware, TestRasterType::kBitmap},
#if BUILDFLAG(ENABLE_GL_BACKEND_TESTS)
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

INSTANTIATE_TEST_SUITE_P(
    B,
    LayerTreeHostBlendingPixelTest,
    ::testing::Combine(::testing::ValuesIn(kTestCases),
                       ::testing::ValuesIn(kBlendModes)),
    // Print a parameter label for blending tests. Use this instead of
    // PrintTupleToStringParamName() because the PrintTo(SkBlendMode)
    // implementation wasn't being used on some platforms (crbug.com/1123758).
    [](const testing::TestParamInfo<
        testing::tuple<RasterTestConfig, SkBlendMode>>& info) -> std::string {
      std::stringstream ss;
      PrintTo(testing::get<0>(info.param), &ss);
      ss << "_" << SkBlendMode_Name(testing::get<1>(info.param));
      return ss.str();
    });

TEST_P(LayerTreeHostBlendingPixelTest, BlendingWithRoot) {
  const int kRootWidth = 2;
  const int kRootHeight = 2;

  scoped_refptr<SolidColorLayer> background =
      CreateSolidColorLayer(gfx::Rect(kRootWidth, kRootHeight), kCSSOrange);

  // Orange child layers will blend with the green background
  gfx::Rect child_rect(0, 0, kRootWidth, kRootHeight);
  scoped_refptr<SolidColorLayer> green_lane =
      CreateSolidColorLayer(child_rect, kCSSGreen);
  background->AddChild(green_lane);
  green_lane->SetBlendMode(current_blend_mode());

  SkBitmap expected;
  expected.allocN32Pixels(kRootWidth, kRootHeight);
  SkCanvas canvas(expected, SkSurfaceProps{});
  canvas.drawColor(kCSSOrange);
  SkPaint paint;
  paint.setBlendMode(current_blend_mode());
  paint.setColor(kCSSGreen);
  canvas.drawRect(SkRect::MakeWH(kRootWidth, kRootHeight), paint);

  RunPixelResourceTest(background, expected);
}

TEST_P(LayerTreeHostBlendingPixelTest, BlendingWithBackdropFilter) {
  const int kRootWidth = 2;
  const int kRootHeight = 2;

  scoped_refptr<SolidColorLayer> background =
      CreateSolidColorLayer(gfx::Rect(kRootWidth, kRootHeight), kCSSOrange);

  // Orange child layers have a backdrop filter set and they will blend with
  // the green background
  gfx::Rect child_rect(0, 0, kRootWidth, kRootHeight);
  scoped_refptr<SolidColorLayer> green_lane =
      CreateSolidColorLayer(child_rect, kCSSGreen);
  background->AddChild(green_lane);
  FilterOperations filters;
  filters.Append(FilterOperation::CreateGrayscaleFilter(.75));
  green_lane->SetBackdropFilters(filters);
  green_lane->ClearBackdropFilterBounds();
  green_lane->SetBlendMode(current_blend_mode());

  SkBitmap expected;
  expected.allocN32Pixels(kRootWidth, kRootHeight);
  SkCanvas canvas(expected, SkSurfaceProps{});
  SkiaPaintCanvas paint_canvas(&canvas);
  PaintFlags grayscale;
  grayscale.setColor(kCSSOrange);

  sk_sp<PaintFilter> paint_filter =
      RenderSurfaceFilters::BuildImageFilter(filters);
  grayscale.setImageFilter(paint_filter);
  paint_canvas.drawRect(SkRect::MakeWH(kRootWidth, kRootHeight), grayscale);

  PaintFlags blend_green;
  blend_green.setBlendMode(current_blend_mode());
  blend_green.setColor(kCSSGreen);
  paint_canvas.drawRect(SkRect::MakeWH(kRootWidth, kRootHeight), blend_green);

  RunPixelResourceTest(background, expected);
}

TEST_P(LayerTreeHostBlendingPixelTest, BlendingWithTransparent) {
  const int kRootWidth = 2;
  const int kRootHeight = 2;

  // Intermediate layer here that should be ignored because of the isolated
  // group.
  scoped_refptr<SolidColorLayer> root =
      CreateSolidColorLayer(gfx::Rect(kRootWidth, kRootHeight), kCSSBrown);

  scoped_refptr<SolidColorLayer> background =
      CreateSolidColorLayer(gfx::Rect(kRootWidth, kRootHeight), kCSSOrange);

  root->AddChild(background);
  background->SetForceRenderSurfaceForTesting(true);

  // Orange child layers will blend with the green background
  gfx::Rect child_rect(kRootWidth, kRootHeight);
  scoped_refptr<SolidColorLayer> green_lane =
      CreateSolidColorLayer(child_rect, kCSSGreen);
  background->AddChild(green_lane);
  green_lane->SetBlendMode(current_blend_mode());

  SkBitmap expected;
  expected.allocN32Pixels(kRootWidth, kRootHeight);
  SkCanvas canvas(expected, SkSurfaceProps{});
  canvas.drawColor(kCSSOrange);
  SkPaint paint;
  paint.setBlendMode(current_blend_mode());
  paint.setColor(kCSSGreen);
  canvas.drawRect(SkRect::MakeWH(kRootWidth, kRootHeight), paint);

  RunPixelResourceTest(root, expected);
}

TEST_P(LayerTreeHostBlendingPixelTest, BlendingWithRenderPass) {
  RunBlendingWithRenderPass(0);
}

TEST_P(LayerTreeHostBlendingPixelTest, BlendingWithRenderPassAA) {
  RunBlendingWithRenderPass(kUseAntialiasing);
}

TEST_P(LayerTreeHostBlendingPixelTest, BlendingWithRenderPassColorMatrix) {
  RunBlendingWithRenderPass(kUseColorMatrix);
}

TEST_P(LayerTreeHostBlendingPixelTest, BlendingWithRenderPassWithMask) {
  RunBlendingWithRenderPass(kUseMasks);
}

TEST_P(LayerTreeHostBlendingPixelTest, BlendingWithRenderPassColorMatrixAA) {
  RunBlendingWithRenderPass(kUseAntialiasing | kUseColorMatrix);
}

TEST_P(LayerTreeHostBlendingPixelTest, BlendingWithRenderPassWithMaskAA) {
  RunBlendingWithRenderPass(kUseMasks | kUseAntialiasing);
}

TEST_P(LayerTreeHostBlendingPixelTest,
       BlendingWithRenderPassWithMaskColorMatrix) {
  RunBlendingWithRenderPass(kUseMasks | kUseColorMatrix);
}

TEST_P(LayerTreeHostBlendingPixelTest,
       BlendingWithRenderPassWithMaskColorMatrixAA) {
  RunBlendingWithRenderPass(kUseMasks | kUseAntialiasing | kUseColorMatrix);
}

}  // namespace
}  // namespace cc

#endif  // BUILDFLAG(IS_ANDROID)
