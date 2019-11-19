// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <stdint.h>

#include "base/stl_util.h"
#include "build/build_config.h"
#include "cc/layers/picture_image_layer.h"
#include "cc/layers/solid_color_layer.h"
#include "cc/paint/paint_image.h"
#include "cc/paint/paint_image_builder.h"
#include "cc/paint/render_surface_filters.h"
#include "cc/paint/skia_paint_canvas.h"
#include "cc/test/layer_tree_pixel_resource_test.h"
#include "cc/test/pixel_comparator.h"
#include "cc/test/test_layer_tree_frame_sink.h"
#include "third_party/skia/include/core/SkImage.h"
#include "third_party/skia/include/core/SkSurface.h"

#if !defined(OS_ANDROID)

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

const int kCSSTestColorsCount = base::size(kCSSTestColors);

using RenderPassOptions = uint32_t;
const uint32_t kUseMasks = 1 << 0;
const uint32_t kUseAntialiasing = 1 << 1;
const uint32_t kUseColorMatrix = 1 << 2;
const uint32_t kForceShaders = 1 << 3;

class LayerTreeHostBlendingPixelTest
    : public LayerTreeHostPixelResourceTest,
      public ::testing::WithParamInterface<
          ::testing::tuple<PixelResourceTestCase, SkBlendMode>> {
 public:
  LayerTreeHostBlendingPixelTest()
      : force_antialiasing_(false), force_blending_with_shaders_(false) {
    pixel_comparator_ = std::make_unique<FuzzyPixelOffByOneComparator>(true);
  }

  PixelResourceTestCase resource_type() const {
    return ::testing::get<0>(GetParam());
  }
  SkBlendMode current_blend_mode() const {
    return ::testing::get<1>(GetParam());
  }

 protected:
  std::unique_ptr<TestLayerTreeFrameSink> CreateLayerTreeFrameSink(
      const viz::RendererSettings& renderer_settings,
      double refresh_rate,
      scoped_refptr<viz::ContextProvider> compositor_context_provider,
      scoped_refptr<viz::RasterContextProvider> worker_context_provider)
      override {
    viz::RendererSettings modified_renderer_settings = renderer_settings;
    modified_renderer_settings.force_antialiasing = force_antialiasing_;
    modified_renderer_settings.force_blending_with_shaders =
        force_blending_with_shaders_;
    return LayerTreeHostPixelResourceTest::CreateLayerTreeFrameSink(
        modified_renderer_settings, refresh_rate, compositor_context_provider,
        worker_context_provider);
  }

  sk_sp<SkSurface> CreateColorfulSurface(int width, int height) {
    // Draw the backdrop with horizontal lanes.
    const int kLaneWidth = width;
    const int kLaneHeight = height / kCSSTestColorsCount;
    sk_sp<SkSurface> backing_store =
        SkSurface::MakeRasterN32Premul(width, height);
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
    scoped_refptr<PictureImageLayer> layer = PictureImageLayer::Create();
    layer->SetIsDrawable(true);
    layer->SetBounds(gfx::Size(width, height));
    layer->SetImage(PaintImageBuilder::WithDefault()
                        .set_id(PaintImage::GetNextId())
                        .set_image(backing_store->makeImageSnapshot(),
                                   PaintImage::GetNextContentId())
                        .TakePaintImage(),
                    SkMatrix::I(), false);
    return layer;
  }

  void SetupMaskLayer(scoped_refptr<Layer> layer) {
    gfx::Size bounds = layer->bounds();
    scoped_refptr<PictureImageLayer> mask = PictureImageLayer::Create();
    mask->SetIsDrawable(true);
    mask->SetBounds(bounds);

    sk_sp<SkSurface> surface =
        SkSurface::MakeRasterN32Premul(bounds.width(), bounds.height());
    SkCanvas* canvas = surface->getCanvas();
    SkPaint paint;
    paint.setColor(SK_ColorWHITE);
    canvas->clear(SK_ColorTRANSPARENT);
    // This layer is a long skinny layer of size 2, so have the mask
    // cover the right half of it
    canvas->drawRect(
        SkRect::MakeXYWH(1, 0, bounds.width() - 1, bounds.height()), paint);
    mask->SetImage(PaintImageBuilder::WithDefault()
                       .set_id(PaintImage::GetNextId())
                       .set_image(surface->makeImageSnapshot(),
                                  PaintImage::GetNextContentId())
                       .TakePaintImage(),
                   SkMatrix::I(), false);
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
    SkCanvas canvas(expected);
    canvas.clear(SK_ColorWHITE);
    canvas.drawImage(surface->makeImageSnapshot(), 0, 0);

    return expected;
  }

  void RunBlendingWithRenderPass(RenderPassOptions flags) {
    const int kRootWidth = 2;
    const int kRootHeight = kRootWidth * kCSSTestColorsCount;
    InitializeFromTestCase(resource_type());

    // Force shaders only applies to gl renderer.
    if (renderer_type() != RENDERER_GL && flags & kForceShaders)
      return;

    SCOPED_TRACE(TestTypeToString(renderer_type()));
    SCOPED_TRACE(SkBlendMode_Name(current_blend_mode()));

    scoped_refptr<SolidColorLayer> root = CreateSolidColorLayer(
        gfx::Rect(kRootWidth, kRootHeight), SK_ColorWHITE);
    scoped_refptr<Layer> background =
        CreateColorfulBackdropLayer(kRootWidth, kRootHeight);

    background->SetForceRenderSurfaceForTesting(true);
    root->AddChild(background);

    CreateBlendingColorLayers(kRootWidth, kRootHeight, background.get(), flags);

    force_antialiasing_ = (flags & kUseAntialiasing);
    force_blending_with_shaders_ = (flags & kForceShaders);

    if ((renderer_type() == RENDERER_GL && force_antialiasing_) ||
        renderer_type() == RENDERER_SKIA_VK) {
      // Blending results might differ with one pixel.
      float percentage_pixels_error = 35.f;
      float percentage_pixels_small_error = 0.f;
      float average_error_allowed_in_bad_pixels = 1.f;
      int large_error_allowed = 1;
      int small_error_allowed = 0;

      pixel_comparator_ = std::make_unique<FuzzyPixelComparator>(
          false,  // discard_alpha
          percentage_pixels_error, percentage_pixels_small_error,
          average_error_allowed_in_bad_pixels, large_error_allowed,
          small_error_allowed);
    }

    RunPixelResourceTest(root, CreateBlendingWithRenderPassExpected(
                                   kRootWidth, kRootHeight, flags));
  }

  bool force_antialiasing_;
  bool force_blending_with_shaders_;
  SkColor misc_opaque_color_ = 0xffc86464;
};

std::vector<PixelResourceTestCase> const kTestCases = {
    {LayerTreeTest::RENDERER_SOFTWARE, SOFTWARE},
    {LayerTreeTest::RENDERER_GL, ZERO_COPY},
    {LayerTreeTest::RENDERER_SKIA_GL, GPU},
#if defined(ENABLE_CC_VULKAN_TESTS)
    {LayerTreeTest::RENDERER_SKIA_VK, GPU},
#endif
};

INSTANTIATE_TEST_SUITE_P(B,
                         LayerTreeHostBlendingPixelTest,
                         ::testing::Combine(::testing::ValuesIn(kTestCases),
                                            ::testing::ValuesIn(kBlendModes)));

TEST_P(LayerTreeHostBlendingPixelTest, BlendingWithRoot) {
  const int kRootWidth = 2;
  const int kRootHeight = 2;
  InitializeFromTestCase(resource_type());

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
  SkCanvas canvas(expected);
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
  InitializeFromTestCase(resource_type());

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
  SkCanvas canvas(expected);
  SkiaPaintCanvas paint_canvas(&canvas);
  PaintFlags grayscale;
  grayscale.setColor(kCSSOrange);

  sk_sp<PaintFilter> paint_filter = RenderSurfaceFilters::BuildImageFilter(
      filters, gfx::SizeF(kRootWidth, kRootHeight));
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
  InitializeFromTestCase(resource_type());

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
  SkCanvas canvas(expected);
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

TEST_P(LayerTreeHostBlendingPixelTest, BlendingWithRenderPassShaders) {
  RunBlendingWithRenderPass(kForceShaders);
}

TEST_P(LayerTreeHostBlendingPixelTest, BlendingWithRenderPassShadersAA) {
  RunBlendingWithRenderPass(kUseAntialiasing | kForceShaders);
}

TEST_P(LayerTreeHostBlendingPixelTest, BlendingWithRenderPassShadersWithMask) {
  RunBlendingWithRenderPass(kUseMasks | kForceShaders);
}

TEST_P(LayerTreeHostBlendingPixelTest,
       BlendingWithRenderPassShadersWithMaskAA) {
  RunBlendingWithRenderPass(kUseMasks | kUseAntialiasing | kForceShaders);
}

TEST_P(LayerTreeHostBlendingPixelTest,
       BlendingWithRenderPassShadersColorMatrix) {
  RunBlendingWithRenderPass(kUseColorMatrix | kForceShaders);
}

TEST_P(LayerTreeHostBlendingPixelTest,
       BlendingWithRenderPassShadersColorMatrixAA) {
  RunBlendingWithRenderPass(kUseAntialiasing | kUseColorMatrix | kForceShaders);
}

TEST_P(LayerTreeHostBlendingPixelTest,
       BlendingWithRenderPassShadersWithMaskColorMatrix) {
  RunBlendingWithRenderPass(kUseMasks | kUseColorMatrix | kForceShaders);
}

TEST_P(LayerTreeHostBlendingPixelTest,
       BlendingWithRenderPassShadersWithMaskColorMatrixAA) {
  RunBlendingWithRenderPass(kUseMasks | kUseAntialiasing | kUseColorMatrix |
                            kForceShaders);
}

}  // namespace
}  // namespace cc

#endif  // OS_ANDROID
