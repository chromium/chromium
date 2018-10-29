// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <stddef.h>

#include "build/build_config.h"
#include "cc/layers/content_layer_client.h"
#include "cc/layers/picture_image_layer.h"
#include "cc/layers/picture_layer.h"
#include "cc/layers/solid_color_layer.h"
#include "cc/paint/paint_flags.h"
#include "cc/paint/paint_image.h"
#include "cc/paint/paint_image_builder.h"
#include "cc/paint/paint_op_buffer.h"
#include "cc/test/fake_picture_layer.h"
#include "cc/test/layer_tree_pixel_resource_test.h"
#include "cc/test/pixel_comparator.h"
#include "cc/test/solid_color_content_layer_client.h"
#include "components/viz/test/test_layer_tree_frame_sink.h"
#include "third_party/skia/include/core/SkImage.h"

#if !defined(OS_ANDROID)

namespace cc {
namespace {

using LayerTreeHostMasksPixelTest = ParameterizedPixelResourceTest;

INSTANTIATE_PIXEL_RESOURCE_TEST_CASE_P(LayerTreeHostMasksPixelTest);

class MaskContentLayerClient : public ContentLayerClient {
 public:
  explicit MaskContentLayerClient(const gfx::Size& bounds) : bounds_(bounds) {}
  ~MaskContentLayerClient() override = default;

  bool FillsBoundsCompletely() const override { return false; }
  size_t GetApproximateUnsharedMemoryUsage() const override { return 0; }

  gfx::Rect PaintableRegion() override { return gfx::Rect(bounds_); }

  scoped_refptr<DisplayItemList> PaintContentsToDisplayList(
      PaintingControlSetting picture_control) override {
    auto display_list = base::MakeRefCounted<DisplayItemList>();
    display_list->StartPaint();

    display_list->push<SaveOp>();
    display_list->push<ClipRectOp>(gfx::RectToSkRect(PaintableRegion()),
                                   SkClipOp::kIntersect, false);
    SkColor color = SK_ColorTRANSPARENT;
    display_list->push<DrawColorOp>(color, SkBlendMode::kSrc);

    PaintFlags flags;
    flags.setStyle(PaintFlags::kStroke_Style);
    flags.setStrokeWidth(SkIntToScalar(2));
    flags.setColor(SK_ColorWHITE);

    gfx::Rect inset_rect(bounds_);
    while (!inset_rect.IsEmpty()) {
      inset_rect.Inset(3, 3, 2, 2);
      display_list->push<DrawRectOp>(gfx::RectToSkRect(inset_rect), flags);
      inset_rect.Inset(3, 3, 2, 2);
    }

    display_list->push<RestoreOp>();
    display_list->EndPaintOfUnpaired(PaintableRegion());
    display_list->Finalize();
    return display_list;
  }

 private:
  gfx::Size bounds_;
};

TEST_P(LayerTreeHostMasksPixelTest, MaskOfLayer) {
  scoped_refptr<SolidColorLayer> background =
      CreateSolidColorLayer(gfx::Rect(100, 100), SK_ColorWHITE);

  scoped_refptr<SolidColorLayer> green = CreateSolidColorLayerWithBorder(
      gfx::Rect(25, 25, 50, 50), kCSSGreen, 1, SK_ColorBLACK);
  background->AddChild(green);

  gfx::Size mask_bounds(50, 50);
  MaskContentLayerClient client(mask_bounds);
  scoped_refptr<PictureLayer> mask = PictureLayer::Create(&client);
  mask->SetBounds(mask_bounds);
  mask->SetIsDrawable(true);
  mask->SetLayerMaskType(mask_type_);
  green->SetMaskLayer(mask.get());

  RunPixelResourceTest(background,
                       base::FilePath(FILE_PATH_LITERAL("mask_of_layer.png")));
}

TEST_P(LayerTreeHostMasksPixelTest, ImageMaskOfLayer) {
  scoped_refptr<SolidColorLayer> background =
      CreateSolidColorLayer(gfx::Rect(100, 100), SK_ColorWHITE);

  gfx::Size mask_bounds(50, 50);

  scoped_refptr<PictureImageLayer> mask = PictureImageLayer::Create();
  mask->SetIsDrawable(true);
  mask->SetLayerMaskType(mask_type_);
  mask->SetBounds(mask_bounds);

  sk_sp<SkSurface> surface = SkSurface::MakeRasterN32Premul(200, 200);
  SkCanvas* canvas = surface->getCanvas();
  canvas->scale(SkIntToScalar(4), SkIntToScalar(4));
  MaskContentLayerClient client(mask_bounds);
  scoped_refptr<DisplayItemList> mask_display_list =
      client.PaintContentsToDisplayList(
          ContentLayerClient::PAINTING_BEHAVIOR_NORMAL);
  mask_display_list->Raster(canvas);
  mask->SetImage(PaintImageBuilder::WithDefault()
                     .set_id(PaintImage::GetNextId())
                     .set_image(surface->makeImageSnapshot(),
                                PaintImage::GetNextContentId())
                     .TakePaintImage(),
                 SkMatrix::I(), false);

  scoped_refptr<SolidColorLayer> green = CreateSolidColorLayerWithBorder(
      gfx::Rect(25, 25, 50, 50), kCSSGreen, 1, SK_ColorBLACK);
  green->SetMaskLayer(mask.get());
  background->AddChild(green);

  RunPixelResourceTest(
      background, base::FilePath(FILE_PATH_LITERAL("image_mask_of_layer.png")));
}

TEST_P(LayerTreeHostMasksPixelTest, MaskOfClippedLayer) {
  scoped_refptr<SolidColorLayer> background =
      CreateSolidColorLayer(gfx::Rect(100, 100), SK_ColorWHITE);

  // Clip to the top half of the green layer.
  scoped_refptr<Layer> clip = Layer::Create();
  clip->SetPosition(gfx::PointF());
  clip->SetBounds(gfx::Size(100, 50));
  clip->SetMasksToBounds(true);
  background->AddChild(clip);

  scoped_refptr<SolidColorLayer> green = CreateSolidColorLayerWithBorder(
      gfx::Rect(25, 25, 50, 50), kCSSGreen, 1, SK_ColorBLACK);
  clip->AddChild(green);

  gfx::Size mask_bounds(50, 50);
  MaskContentLayerClient client(mask_bounds);
  scoped_refptr<PictureLayer> mask = PictureLayer::Create(&client);
  mask->SetBounds(mask_bounds);
  mask->SetIsDrawable(true);
  mask->SetLayerMaskType(mask_type_);
  green->SetMaskLayer(mask.get());

  RunPixelResourceTest(
      background,
      base::FilePath(FILE_PATH_LITERAL("mask_of_clipped_layer.png")));
}

TEST_P(LayerTreeHostMasksPixelTest, MaskOfLayerNonExactTextureSize) {
  scoped_refptr<SolidColorLayer> background =
      CreateSolidColorLayer(gfx::Rect(100, 100), SK_ColorWHITE);

  scoped_refptr<SolidColorLayer> green = CreateSolidColorLayerWithBorder(
      gfx::Rect(0, 0, 100, 100), kCSSGreen, 1, SK_ColorBLACK);
  background->AddChild(green);

  gfx::Size mask_bounds(100, 100);
  MaskContentLayerClient client(mask_bounds);
  scoped_refptr<FakePictureLayer> mask = FakePictureLayer::Create(&client);
  mask->SetBounds(mask_bounds);
  mask->SetIsDrawable(true);
  mask->SetLayerMaskType(mask_type_);
  mask->set_fixed_tile_size(gfx::Size(173, 135));
  green->SetMaskLayer(mask.get());

  RunPixelResourceTest(background,
                       base::FilePath(FILE_PATH_LITERAL(
                           "mask_with_non_exact_texture_size.png")));
}

class CheckerContentLayerClient : public ContentLayerClient {
 public:
  CheckerContentLayerClient(const gfx::Size& bounds,
                           SkColor color,
                           bool vertical)
      : bounds_(bounds), color_(color), vertical_(vertical) {}
  ~CheckerContentLayerClient() override = default;
  bool FillsBoundsCompletely() const override { return false; }
  size_t GetApproximateUnsharedMemoryUsage() const override { return 0; }
  gfx::Rect PaintableRegion() override { return gfx::Rect(bounds_); }
  scoped_refptr<DisplayItemList> PaintContentsToDisplayList(
      PaintingControlSetting picture_control) override {
    auto display_list = base::MakeRefCounted<DisplayItemList>();
    display_list->StartPaint();

    display_list->push<SaveOp>();
    display_list->push<ClipRectOp>(gfx::RectToSkRect(PaintableRegion()),
                                   SkClipOp::kIntersect, false);
    SkColor color = SK_ColorTRANSPARENT;
    display_list->push<DrawColorOp>(color, SkBlendMode::kSrc);

    PaintFlags flags;
    flags.setStyle(PaintFlags::kStroke_Style);
    flags.setStrokeWidth(SkIntToScalar(4));
    flags.setColor(color_);
    if (vertical_) {
      for (int i = 4; i < bounds_.width(); i += 16) {
        gfx::PointF p1(i, 0.f);
        gfx::PointF p2(i, bounds_.height());
        display_list->push<DrawLineOp>(p1.x(), p1.y(), p2.x(), p2.y(), flags);
      }
    } else {
      for (int i = 4; i < bounds_.height(); i += 16) {
        gfx::PointF p1(0.f, i);
        gfx::PointF p2(bounds_.width(), i);
        display_list->push<DrawLineOp>(p1.x(), p1.y(), p2.x(), p2.y(), flags);
      }
    }

    display_list->push<RestoreOp>();
    display_list->EndPaintOfUnpaired(PaintableRegion());
    display_list->Finalize();
    return display_list;
  }

 private:
  gfx::Size bounds_;
  SkColor color_;
  bool vertical_;
};

class CircleContentLayerClient : public ContentLayerClient {
 public:
  explicit CircleContentLayerClient(const gfx::Size& bounds)
      : bounds_(bounds) {}
  ~CircleContentLayerClient() override = default;
  bool FillsBoundsCompletely() const override { return false; }
  size_t GetApproximateUnsharedMemoryUsage() const override { return 0; }
  gfx::Rect PaintableRegion() override { return gfx::Rect(bounds_); }
  scoped_refptr<DisplayItemList> PaintContentsToDisplayList(
      PaintingControlSetting picture_control) override {
    auto display_list = base::MakeRefCounted<DisplayItemList>();
    display_list->StartPaint();

    display_list->push<SaveOp>();
    display_list->push<ClipRectOp>(gfx::RectToSkRect(PaintableRegion()),
                                   SkClipOp::kIntersect, false);
    SkColor color = SK_ColorTRANSPARENT;
    display_list->push<DrawColorOp>(color, SkBlendMode::kSrc);

    PaintFlags flags;
    flags.setStyle(PaintFlags::kFill_Style);
    flags.setColor(SK_ColorWHITE);
    float radius = bounds_.width() / 4.f;
    float circle_x = bounds_.width() / 2.f;
    float circle_y = bounds_.height() / 2.f;
    display_list->push<DrawOvalOp>(
        SkRect::MakeLTRB(circle_x - radius, circle_y - radius,
                         circle_x + radius, circle_y + radius),
        flags);
    display_list->push<RestoreOp>();
    display_list->EndPaintOfUnpaired(PaintableRegion());
    display_list->Finalize();
    return display_list;
  }

 private:
  gfx::Size bounds_;
};

using LayerTreeHostMasksForBackdropFiltersPixelTest =
    ParameterizedPixelResourceTest;

INSTANTIATE_TEST_CASE_P(
    PixelResourceTest,
    LayerTreeHostMasksForBackdropFiltersPixelTest,
    ::testing::Combine(
        ::testing::Values(SOFTWARE, GPU, ONE_COPY, ZERO_COPY),
        ::testing::Values(Layer::LayerMaskType::SINGLE_TEXTURE_MASK,
                          Layer::LayerMaskType::MULTI_TEXTURE_MASK)));

TEST_P(LayerTreeHostMasksForBackdropFiltersPixelTest,
       MaskOfLayerWithBackdropFilter) {
  scoped_refptr<SolidColorLayer> background = CreateSolidColorLayer(
      gfx::Rect(100, 100), SK_ColorWHITE);

  gfx::Size picture_bounds(100, 100);
  CheckerContentLayerClient picture_client(picture_bounds, SK_ColorGREEN, true);
  scoped_refptr<PictureLayer> picture = PictureLayer::Create(&picture_client);
  picture->SetBounds(picture_bounds);
  picture->SetIsDrawable(true);

  scoped_refptr<SolidColorLayer> blur = CreateSolidColorLayer(
      gfx::Rect(100, 100), SK_ColorTRANSPARENT);
  background->AddChild(picture);
  background->AddChild(blur);

  FilterOperations filters;
  filters.Append(FilterOperation::CreateGrayscaleFilter(1.0));
  blur->SetBackdropFilters(filters);

  gfx::Size mask_bounds(100, 100);
  CircleContentLayerClient mask_client(mask_bounds);
  scoped_refptr<PictureLayer> mask = PictureLayer::Create(&mask_client);
  mask->SetBounds(mask_bounds);
  mask->SetIsDrawable(true);
  mask->SetLayerMaskType(mask_type_);
  blur->SetMaskLayer(mask.get());
  CHECK_EQ(Layer::LayerMaskType::SINGLE_TEXTURE_MASK, mask->mask_type());

  float percentage_pixels_large_error = 2.5f;  // 2.5%, ~250px / (100*100)
  float percentage_pixels_small_error = 0.0f;
  float average_error_allowed_in_bad_pixels = 100.0f;
  int large_error_allowed = 256;
  int small_error_allowed = 0;
  pixel_comparator_ = std::make_unique<FuzzyPixelComparator>(
      true,  // discard_alpha
      percentage_pixels_large_error,
      percentage_pixels_small_error,
      average_error_allowed_in_bad_pixels,
      large_error_allowed,
      small_error_allowed);

  base::FilePath image_name =
      (test_case_ == GPU)
          ? base::FilePath(FILE_PATH_LITERAL("mask_of_backdrop_filter_gpu.png"))
          : base::FilePath(FILE_PATH_LITERAL("mask_of_backdrop_filter.png"));
  RunPixelResourceTest(background, image_name);
}

TEST_P(LayerTreeHostMasksForBackdropFiltersPixelTest, MaskOfLayerWithBlend) {
  scoped_refptr<SolidColorLayer> background = CreateSolidColorLayer(
      gfx::Rect(128, 128), SK_ColorWHITE);

  gfx::Size picture_bounds(128, 128);
  CheckerContentLayerClient picture_client_vertical(
      picture_bounds, SK_ColorGREEN, true);
  scoped_refptr<PictureLayer> picture_vertical =
      PictureLayer::Create(&picture_client_vertical);
  picture_vertical->SetBounds(picture_bounds);
  picture_vertical->SetIsDrawable(true);

  CheckerContentLayerClient picture_client_horizontal(
      picture_bounds, SK_ColorMAGENTA, false);
  scoped_refptr<PictureLayer> picture_horizontal =
      PictureLayer::Create(&picture_client_horizontal);
  picture_horizontal->SetBounds(picture_bounds);
  picture_horizontal->SetIsDrawable(true);
  picture_horizontal->SetContentsOpaque(false);
  picture_horizontal->SetBlendMode(SkBlendMode::kMultiply);

  background->AddChild(picture_vertical);
  background->AddChild(picture_horizontal);

  gfx::Size mask_bounds(128, 128);
  CircleContentLayerClient mask_client(mask_bounds);
  scoped_refptr<PictureLayer> mask = PictureLayer::Create(&mask_client);
  mask->SetBounds(mask_bounds);
  mask->SetIsDrawable(true);
  mask->SetLayerMaskType(mask_type_);
  picture_horizontal->SetMaskLayer(mask.get());

  float percentage_pixels_large_error = 0.04f;  // 0.04%, ~6px / (128*128)
  float percentage_pixels_small_error = 0.0f;
  float average_error_allowed_in_bad_pixels = 256.0f;
  int large_error_allowed = 256;
  int small_error_allowed = 0;
  pixel_comparator_ = std::make_unique<FuzzyPixelComparator>(
      true,  // discard_alpha
      percentage_pixels_large_error,
      percentage_pixels_small_error,
      average_error_allowed_in_bad_pixels,
      large_error_allowed,
      small_error_allowed);

  RunPixelResourceTest(background,
                       base::FilePath(
                           FILE_PATH_LITERAL("mask_of_layer_with_blend.png")));
}

class StaticPictureLayer : private ContentLayerClient, public PictureLayer {
 public:
  static scoped_refptr<StaticPictureLayer> Create(
      scoped_refptr<DisplayItemList> display_list) {
    return base::WrapRefCounted(
        new StaticPictureLayer(std::move(display_list)));
  }

  gfx::Rect PaintableRegion() override { return gfx::Rect(bounds()); }
  scoped_refptr<DisplayItemList> PaintContentsToDisplayList(
      PaintingControlSetting) override {
    return display_list_;
  }
  bool FillsBoundsCompletely() const override { return false; }
  size_t GetApproximateUnsharedMemoryUsage() const override { return 0; }

 protected:
  explicit StaticPictureLayer(scoped_refptr<DisplayItemList> display_list)
      : PictureLayer(this), display_list_(std::move(display_list)) {}
  ~StaticPictureLayer() override = default;

 private:
  scoped_refptr<DisplayItemList> display_list_;
};

class LayerTreeHostMaskAsBlendingPixelTest
    : public LayerTreeHostPixelResourceTest,
      public ::testing::WithParamInterface<int> {
 public:
  LayerTreeHostMaskAsBlendingPixelTest()
      : LayerTreeHostPixelResourceTest(
            GetParam() ? ZERO_COPY : SOFTWARE,
            Layer::LayerMaskType::SINGLE_TEXTURE_MASK),
        use_antialiasing_(GetParam() == 2 || GetParam() == 4),
        force_shaders_(GetParam() == 3 || GetParam() == 4) {
    float percentage_pixels_small_error = 0.f;
    float percentage_pixels_error = 0.f;
    float average_error_allowed_in_bad_pixels = 0.f;
    int large_error_allowed = 0;
    int small_error_allowed = 0;
    if (use_antialiasing_) {
      percentage_pixels_small_error = 0.9f;
      percentage_pixels_error = 6.7f;
      average_error_allowed_in_bad_pixels = 3.5f;
      large_error_allowed = 15;
      small_error_allowed = 1;
    } else if (test_type_ != PIXEL_TEST_SOFTWARE) {
      percentage_pixels_small_error = 0.9f;
      percentage_pixels_error = 6.5f;
      average_error_allowed_in_bad_pixels = 3.5f;
      large_error_allowed = 15;
      small_error_allowed = 1;
    } else {
#if defined(ARCH_CPU_ARM64)
      // Differences in floating point calculation on ARM means a small
      // percentage of pixels will be off by 1.
      percentage_pixels_error = 0.112f;
      average_error_allowed_in_bad_pixels = 1.f;
      large_error_allowed = 1;
#endif
    }

    pixel_comparator_ = std::make_unique<FuzzyPixelComparator>(
        false,  // discard_alpha
        percentage_pixels_error, percentage_pixels_small_error,
        average_error_allowed_in_bad_pixels, large_error_allowed,
        small_error_allowed);
  }

  static scoped_refptr<Layer> CreateCheckerboardLayer(const gfx::Size& bounds) {
    constexpr int kGridSize = 8;
    static const SkColor color_even = SkColorSetRGB(153, 153, 153);
    static const SkColor color_odd = SkColorSetRGB(102, 102, 102);

    auto display_list = base::MakeRefCounted<DisplayItemList>();
    display_list->StartPaint();
    display_list->push<DrawColorOp>(color_even, SkBlendMode::kSrc);
    PaintFlags flags;
    flags.setColor(color_odd);
    for (int j = 0; j < (bounds.height() + kGridSize - 1) / kGridSize; j++) {
      for (int i = 0; i < (bounds.width() + kGridSize - 1) / kGridSize; i++) {
        bool is_odd_grid = (i ^ j) & 1;
        if (!is_odd_grid)
          continue;
        display_list->push<DrawRectOp>(
            SkRect::MakeXYWH(i * kGridSize, j * kGridSize, kGridSize,
                             kGridSize),
            flags);
      }
    }
    display_list->EndPaintOfUnpaired(gfx::Rect(bounds));
    display_list->Finalize();

    scoped_refptr<Layer> layer =
        StaticPictureLayer::Create(std::move(display_list));
    layer->SetIsDrawable(true);
    layer->SetBounds(bounds);
    return layer;
  }

  static scoped_refptr<Layer> CreateTestPatternLayer(const gfx::Size& bounds,
                                                     int grid_size) {
    // Creates a layer consists of solid grids. The grids are in a mix of
    // different transparency and colors (1 transparent, 3 semi-transparent,
    // and 3 opaque).
    static SkColor test_colors[7] = {
        SkColorSetARGB(128, 255, 0, 0), SkColorSetARGB(255, 0, 0, 255),
        SkColorSetARGB(128, 0, 255, 0), SkColorSetARGB(128, 0, 0, 255),
        SkColorSetARGB(255, 0, 255, 0), SkColorSetARGB(0, 0, 0, 0),
        SkColorSetARGB(255, 255, 0, 0)};

    auto display_list = base::MakeRefCounted<DisplayItemList>();
    display_list->StartPaint();
    for (int j = 0; j < (bounds.height() + grid_size - 1) / grid_size; j++) {
      for (int i = 0; i < (bounds.width() + grid_size - 1) / grid_size; i++) {
        PaintFlags flags;
        flags.setColor(test_colors[(i + j * 3) % arraysize(test_colors)]);
        display_list->push<DrawRectOp>(
            SkRect::MakeXYWH(i * grid_size, j * grid_size, grid_size,
                             grid_size),
            flags);
      }
    }
    display_list->EndPaintOfUnpaired(gfx::Rect(bounds));
    display_list->Finalize();

    scoped_refptr<Layer> layer =
        StaticPictureLayer::Create(std::move(display_list));
    layer->SetIsDrawable(true);
    layer->SetBounds(bounds);
    return layer;
  }

 protected:
  std::unique_ptr<viz::TestLayerTreeFrameSink> CreateLayerTreeFrameSink(
      const viz::RendererSettings& renderer_settings,
      double refresh_rate,
      scoped_refptr<viz::ContextProvider> compositor_context_provider,
      scoped_refptr<viz::RasterContextProvider> worker_context_provider)
      override {
    viz::RendererSettings modified_renderer_settings = renderer_settings;
    modified_renderer_settings.force_antialiasing = use_antialiasing_;
    modified_renderer_settings.force_blending_with_shaders = force_shaders_;
    return LayerTreeHostPixelResourceTest::CreateLayerTreeFrameSink(
        modified_renderer_settings, refresh_rate,
        std::move(compositor_context_provider),
        std::move(worker_context_provider));
  }

  bool use_antialiasing_;
  bool force_shaders_;
};

INSTANTIATE_TEST_CASE_P(All,
                        LayerTreeHostMaskAsBlendingPixelTest,
                        ::testing::Range(0, 5));
// Instantiate 5 test modes of the following:
// 0: SOFTWARE (golden sample)
// 1: GL
// 2: GL + AA
// 3: GL + Forced Shaders
// 4: GL + Forced Shaders + AA

TEST_P(LayerTreeHostMaskAsBlendingPixelTest, PixelAlignedNoop) {
  // This test verifies the degenerate case of a no-op mask doesn't affect
  // the contents in any way.
  scoped_refptr<Layer> root = CreateCheckerboardLayer(gfx::Size(400, 300));

  scoped_refptr<Layer> mask_isolation = Layer::Create();
  mask_isolation->SetPosition(gfx::PointF(20, 20));
  mask_isolation->SetBounds(gfx::Size(350, 250));
  mask_isolation->SetMasksToBounds(true);
  mask_isolation->SetIsRootForIsolatedGroup(true);
  root->AddChild(mask_isolation);

  scoped_refptr<Layer> content =
      CreateTestPatternLayer(gfx::Size(400, 300), 25);
  content->SetPosition(gfx::PointF(-40, -40));
  mask_isolation->AddChild(content);

  scoped_refptr<Layer> mask_layer =
      CreateSolidColorLayer(gfx::Rect(350, 250), kCSSBlack);
  mask_layer->SetBlendMode(SkBlendMode::kDstIn);
  mask_isolation->AddChild(mask_layer);

  RunPixelResourceTest(
      root, base::FilePath(FILE_PATH_LITERAL("mask_as_blending_noop.png")));
}

TEST_P(LayerTreeHostMaskAsBlendingPixelTest, PixelAlignedClippedCircle) {
  // This test verifies a simple pixel aligned mask applies correctly.
  scoped_refptr<Layer> root = CreateCheckerboardLayer(gfx::Size(400, 300));

  scoped_refptr<Layer> mask_isolation = Layer::Create();
  mask_isolation->SetPosition(gfx::PointF(20, 20));
  mask_isolation->SetBounds(gfx::Size(350, 250));
  mask_isolation->SetMasksToBounds(true);
  mask_isolation->SetIsRootForIsolatedGroup(true);
  root->AddChild(mask_isolation);

  scoped_refptr<Layer> content =
      CreateTestPatternLayer(gfx::Size(400, 300), 25);
  content->SetPosition(gfx::PointF(-40, -40));
  mask_isolation->AddChild(content);

  auto display_list = base::MakeRefCounted<DisplayItemList>();
  display_list->StartPaint();
  PaintFlags flags;
  flags.setColor(kCSSBlack);
  flags.setAntiAlias(true);
  display_list->push<DrawOvalOp>(SkRect::MakeXYWH(-5, -55, 360, 360), flags);
  display_list->EndPaintOfUnpaired(gfx::Rect(-5, -55, 360, 360));
  display_list->Finalize();
  scoped_refptr<Layer> mask_layer =
      StaticPictureLayer::Create(std::move(display_list));
  mask_layer->SetIsDrawable(true);
  mask_layer->SetBounds(gfx::Size(350, 250));
  mask_layer->SetBlendMode(SkBlendMode::kDstIn);
  mask_isolation->AddChild(mask_layer);

  RunPixelResourceTest(
      root, base::FilePath(FILE_PATH_LITERAL("mask_as_blending_circle.png")));
}

TEST_P(LayerTreeHostMaskAsBlendingPixelTest,
       PixelAlignedClippedCircleUnderflow) {
  // This test verifies a simple pixel aligned mask applies correctly when
  // the content is smaller than the mask.
  scoped_refptr<Layer> root = CreateCheckerboardLayer(gfx::Size(400, 300));

  scoped_refptr<Layer> mask_isolation = Layer::Create();
  mask_isolation->SetPosition(gfx::PointF(20, 20));
  mask_isolation->SetBounds(gfx::Size(350, 250));
  mask_isolation->SetMasksToBounds(true);
  mask_isolation->SetIsRootForIsolatedGroup(true);
  root->AddChild(mask_isolation);

  scoped_refptr<Layer> content =
      CreateTestPatternLayer(gfx::Size(330, 230), 25);
  content->SetPosition(gfx::PointF(10, 10));
  mask_isolation->AddChild(content);

  auto display_list = base::MakeRefCounted<DisplayItemList>();
  display_list->StartPaint();
  PaintFlags flags;
  flags.setColor(kCSSBlack);
  flags.setAntiAlias(true);
  display_list->push<DrawOvalOp>(SkRect::MakeXYWH(-5, -55, 360, 360), flags);
  display_list->EndPaintOfUnpaired(gfx::Rect(-5, -55, 360, 360));
  display_list->Finalize();
  scoped_refptr<Layer> mask_layer =
      StaticPictureLayer::Create(std::move(display_list));
  mask_layer->SetIsDrawable(true);
  mask_layer->SetBounds(gfx::Size(350, 250));
  mask_layer->SetBlendMode(SkBlendMode::kDstIn);
  mask_isolation->AddChild(mask_layer);

  RunPixelResourceTest(root, base::FilePath(FILE_PATH_LITERAL(
                                 "mask_as_blending_circle_underflow.png")));
}

TEST_P(LayerTreeHostMaskAsBlendingPixelTest, RotatedClippedCircle) {
  // This test verifies a simple pixel aligned mask that is not pixel aligned
  // to its target surface is rendered correctly.
  scoped_refptr<Layer> root = CreateCheckerboardLayer(gfx::Size(400, 300));

  scoped_refptr<Layer> mask_isolation = Layer::Create();
  mask_isolation->SetPosition(gfx::PointF(20, 20));
  {
    gfx::Transform rotate;
    rotate.Rotate(5.f);
    mask_isolation->SetTransform(rotate);
  }
  mask_isolation->SetBounds(gfx::Size(350, 250));
  mask_isolation->SetMasksToBounds(true);
  mask_isolation->SetIsRootForIsolatedGroup(true);
  root->AddChild(mask_isolation);

  scoped_refptr<Layer> content =
      CreateTestPatternLayer(gfx::Size(400, 300), 25);
  content->SetPosition(gfx::PointF(-40, -40));
  mask_isolation->AddChild(content);

  auto display_list = base::MakeRefCounted<DisplayItemList>();
  display_list->StartPaint();
  PaintFlags flags;
  flags.setColor(kCSSBlack);
  flags.setAntiAlias(true);
  display_list->push<DrawOvalOp>(SkRect::MakeXYWH(-5, -55, 360, 360), flags);
  display_list->EndPaintOfUnpaired(gfx::Rect(-5, -55, 360, 360));
  display_list->Finalize();
  scoped_refptr<Layer> mask_layer =
      StaticPictureLayer::Create(std::move(display_list));
  mask_layer->SetIsDrawable(true);
  mask_layer->SetBounds(gfx::Size(350, 250));
  mask_layer->SetBlendMode(SkBlendMode::kDstIn);
  mask_isolation->AddChild(mask_layer);

  base::FilePath image_name =
      (test_type_ == PIXEL_TEST_SOFTWARE)
          ? base::FilePath(
                FILE_PATH_LITERAL("mask_as_blending_rotated_circle.png"))
          : base::FilePath(
                FILE_PATH_LITERAL("mask_as_blending_rotated_circle_gl.png"));
  RunPixelResourceTest(root, image_name);
}

TEST_P(LayerTreeHostMaskAsBlendingPixelTest, RotatedClippedCircleUnderflow) {
  // This test verifies a simple pixel aligned mask that is not pixel aligned
  // to its target surface, and has the content smaller than the mask, is
  // rendered correctly.
  scoped_refptr<Layer> root = CreateCheckerboardLayer(gfx::Size(400, 300));

  scoped_refptr<Layer> mask_isolation = Layer::Create();
  mask_isolation->SetPosition(gfx::PointF(20, 20));
  {
    gfx::Transform rotate;
    rotate.Rotate(5.f);
    mask_isolation->SetTransform(rotate);
  }
  mask_isolation->SetBounds(gfx::Size(350, 250));
  mask_isolation->SetMasksToBounds(true);
  mask_isolation->SetIsRootForIsolatedGroup(true);
  root->AddChild(mask_isolation);

  scoped_refptr<Layer> content =
      CreateTestPatternLayer(gfx::Size(330, 230), 25);
  content->SetPosition(gfx::PointF(10, 10));
  mask_isolation->AddChild(content);

  auto display_list = base::MakeRefCounted<DisplayItemList>();
  display_list->StartPaint();
  PaintFlags flags;
  flags.setColor(kCSSBlack);
  flags.setAntiAlias(true);
  display_list->push<DrawOvalOp>(SkRect::MakeXYWH(-5, -55, 360, 360), flags);
  display_list->EndPaintOfUnpaired(gfx::Rect(-5, -55, 360, 360));
  display_list->Finalize();
  scoped_refptr<Layer> mask_layer =
      StaticPictureLayer::Create(std::move(display_list));
  mask_layer->SetIsDrawable(true);
  mask_layer->SetBounds(gfx::Size(350, 250));
  mask_layer->SetBlendMode(SkBlendMode::kDstIn);
  mask_isolation->AddChild(mask_layer);

  base::FilePath image_name =
      (test_type_ == PIXEL_TEST_SOFTWARE)
          ? base::FilePath(FILE_PATH_LITERAL(
                "mask_as_blending_rotated_circle_underflow.png"))
          : base::FilePath(FILE_PATH_LITERAL(
                "mask_as_blending_rotated_circle_underflow_gl.png"));
  RunPixelResourceTest(root, image_name);
}

TEST_P(LayerTreeHostMasksForBackdropFiltersPixelTest,
       MaskOfLayerWithBackdropFilterAndBlend) {
  scoped_refptr<SolidColorLayer> background =
      CreateSolidColorLayer(gfx::Rect(128, 128), SK_ColorWHITE);

  gfx::Size picture_bounds(128, 128);
  CheckerContentLayerClient picture_client_vertical(picture_bounds,
                                                    SK_ColorGREEN, true);
  scoped_refptr<PictureLayer> picture_vertical =
      PictureLayer::Create(&picture_client_vertical);
  picture_vertical->SetBounds(picture_bounds);
  picture_vertical->SetIsDrawable(true);

  CheckerContentLayerClient picture_client_horizontal(picture_bounds,
                                                      SK_ColorMAGENTA, false);
  scoped_refptr<PictureLayer> picture_horizontal =
      PictureLayer::Create(&picture_client_horizontal);
  picture_horizontal->SetBounds(picture_bounds);
  picture_horizontal->SetIsDrawable(true);
  picture_horizontal->SetContentsOpaque(false);
  picture_horizontal->SetBlendMode(SkBlendMode::kMultiply);

  FilterOperations filters;
  filters.Append(FilterOperation::CreateGrayscaleFilter(1.0));
  picture_horizontal->SetBackdropFilters(filters);

  background->AddChild(picture_vertical);
  background->AddChild(picture_horizontal);

  gfx::Size mask_bounds(128, 128);
  CircleContentLayerClient mask_client(mask_bounds);
  scoped_refptr<PictureLayer> mask = PictureLayer::Create(&mask_client);
  mask->SetBounds(mask_bounds);
  mask->SetIsDrawable(true);
  mask->SetLayerMaskType(mask_type_);
  picture_horizontal->SetMaskLayer(mask.get());

  float percentage_pixels_large_error = 0.062f;  // 0.062%, ~10px / (128*128)
  float percentage_pixels_small_error = 0.0f;
  float average_error_allowed_in_bad_pixels = 200.0f;
  int large_error_allowed = 256;
  int small_error_allowed = 0;
  pixel_comparator_ = std::make_unique<FuzzyPixelComparator>(
      true,  // discard_alpha
      percentage_pixels_large_error, percentage_pixels_small_error,
      average_error_allowed_in_bad_pixels, large_error_allowed,
      small_error_allowed);

  RunPixelResourceTest(background,
                       base::FilePath(FILE_PATH_LITERAL(
                           "mask_of_backdrop_filter_and_blend.png")));
}

}  // namespace
}  // namespace cc

#endif  // !defined(OS_ANDROID)
