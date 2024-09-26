// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/351564777): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#include <stddef.h>

#include "build/build_config.h"
#include "cc/layers/content_layer_client.h"
#include "cc/layers/picture_layer.h"
#include "cc/layers/solid_color_layer.h"
#include "cc/paint/paint_flags.h"
#include "cc/paint/paint_image.h"
#include "cc/paint/paint_image_builder.h"
#include "cc/paint/paint_op_buffer.h"
#include "cc/test/fake_content_layer_client.h"
#include "cc/test/fake_picture_layer.h"
#include "cc/test/layer_tree_pixel_resource_test.h"
#include "cc/test/pixel_comparator.h"
#include "cc/test/solid_color_content_layer_client.h"
#include "cc/test/test_layer_tree_frame_sink.h"
#include "components/viz/test/buildflags.h"
#include "third_party/skia/include/core/SkImage.h"

#if !BUILDFLAG(IS_ANDROID)

namespace cc {
namespace {

// TODO(penghuang): Fix vulkan with one copy or zero copy
// https://crbug.com/979703
std::vector<RasterTestConfig> const kTestCases = {
    {viz::RendererType::kSoftware, TestRasterType::kBitmap},
#if BUILDFLAG(ENABLE_GL_BACKEND_TESTS)
    {viz::RendererType::kSkiaGL, TestRasterType::kGpu},
    {viz::RendererType::kSkiaGL, TestRasterType::kOneCopy},

// Zero-copy raster is used in production only on Mac and by definition
// relies on SharedImages having scanout support, which is not always the
// case on other platforms and without which these tests would fail when
// run with zero-copy raster.
#if BUILDFLAG(IS_MAC)
    {viz::RendererType::kSkiaGL, TestRasterType::kZeroCopy},
#endif

#endif  // BUILDFLAG(ENABLE_GL_BACKEND_TESTS)
#if BUILDFLAG(ENABLE_VULKAN_BACKEND_TESTS)
    {viz::RendererType::kSkiaVk, TestRasterType::kGpu},
#endif  // BUILDFLAG(ENABLE_VULKAN_BACKEND_TESTS)
};

using LayerTreeHostMasksPixelTest = ParameterizedPixelResourceTest;

INSTANTIATE_TEST_SUITE_P(PixelResourceTest,
                         LayerTreeHostMasksPixelTest,
                         ::testing::ValuesIn(kTestCases),
                         ::testing::PrintToStringParamName());

class MaskContentLayerClient : public ContentLayerClient {
 public:
  explicit MaskContentLayerClient(const gfx::Size& bounds) : bounds_(bounds) {}
  ~MaskContentLayerClient() override = default;

  bool FillsBoundsCompletely() const override { return false; }

  scoped_refptr<DisplayItemList> PaintContentsToDisplayList() override {
    auto display_list = base::MakeRefCounted<DisplayItemList>();
    display_list->StartPaint();

    display_list->push<SaveOp>();
    display_list->push<ClipRectOp>(gfx::RectToSkRect(gfx::Rect(bounds_)),
                                   SkClipOp::kIntersect, false);
    SkColor4f color = SkColors::kTransparent;
    display_list->push<DrawColorOp>(color, SkBlendMode::kSrc);

    PaintFlags flags;
    flags.setStyle(PaintFlags::kStroke_Style);
    flags.setStrokeWidth(SkIntToScalar(2));
    flags.setColor(SkColors::kWhite);

    gfx::Rect inset_rect(bounds_);
    while (!inset_rect.IsEmpty()) {
      inset_rect.Inset(gfx::Insets::TLBR(3, 3, 2, 2));
      display_list->push<DrawRectOp>(gfx::RectToSkRect(inset_rect), flags);
      inset_rect.Inset(gfx::Insets::TLBR(3, 3, 2, 2));
    }

    display_list->push<RestoreOp>();
    display_list->EndPaintOfUnpaired(gfx::Rect(bounds_));
    display_list->Finalize();
    return display_list;
  }

 private:
  base::OnceClosure custom_setup_tree_;
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
  green->SetMaskLayer(mask);

  pixel_comparator_ =
      std::make_unique<AlphaDiscardingFuzzyPixelOffByOneComparator>();

  RunPixelResourceTest(background,
                       base::FilePath(FILE_PATH_LITERAL("mask_of_layer.png")));
}

class LayerTreeHostMaskPixelTestWithLayerList
    : public ParameterizedPixelResourceTest {
 protected:
  LayerTreeHostMaskPixelTestWithLayerList() : mask_bounds_(50, 50) {
    SetUseLayerLists();
  }

  // Setup three layers for testing masks: a white background, a green layer,
  // and a mask layer with kDstIn blend mode.
  void SetupTree() override {
    SetInitialRootBounds(gfx::Size(100, 100));
    ParameterizedPixelResourceTest::SetupTree();

    Layer* root = layer_tree_host()->root_layer();

    scoped_refptr<SolidColorLayer> background =
        CreateSolidColorLayer(gfx::Rect(100, 100), SK_ColorWHITE);
    CopyProperties(root, background.get());
    root->AddChild(background);

    scoped_refptr<SolidColorLayer> green =
        CreateSolidColorLayer(gfx::Rect(25, 25, 50, 50), kCSSGreen);
    CopyProperties(background.get(), green.get());
    auto& isolation_effect = CreateEffectNode(green.get());
    isolation_effect.render_surface_reason = RenderSurfaceReason::kTest;
    root->AddChild(green);

    DCHECK(mask_layer_) << "The test should create mask_layer_ before calling "
                        << "RunPixelResourceTestWithLayerList()";
    mask_layer_->SetOffsetToTransformParent(gfx::Vector2dF(25, 25));
    mask_layer_->SetBounds(mask_bounds_);
    mask_layer_->SetIsDrawable(true);
    CopyProperties(green.get(), mask_layer_.get());
    auto& mask_effect = CreateEffectNode(mask_layer_.get());
    mask_effect.blend_mode = SkBlendMode::kDstIn;
    root->AddChild(mask_layer_);
  }

  gfx::Size mask_bounds_;
  scoped_refptr<Layer> mask_layer_;
};

INSTANTIATE_TEST_SUITE_P(PixelResourceTest,
                         LayerTreeHostMaskPixelTestWithLayerList,
                         ::testing::ValuesIn(kTestCases),
                         ::testing::PrintToStringParamName());

TEST_P(LayerTreeHostMaskPixelTestWithLayerList, MaskWithEffect) {
  MaskContentLayerClient client(mask_bounds_);
  mask_layer_ = PictureLayer::Create(&client);

  pixel_comparator_ =
      std::make_unique<AlphaDiscardingFuzzyPixelOffByOneComparator>();
  RunPixelResourceTestWithLayerList(
      base::FilePath(FILE_PATH_LITERAL("mask_with_effect.png")));
}

// This tests that a solid color empty layer with mask effect works correctly.
TEST_P(LayerTreeHostMaskPixelTestWithLayerList,
       SolidColorLayerEmptyMaskWithEffect) {
  // Apply a mask that is empty and solid-color. This should result in
  // the green layer being entirely clipped out.
  mask_layer_ =
      CreateSolidColorLayer(gfx::Rect(25, 25, 50, 50), SK_ColorTRANSPARENT);

  RunPixelResourceTestWithLayerList(base::FilePath(
      FILE_PATH_LITERAL("solid_color_empty_mask_with_effect.png")));
}

class SolidColorEmptyMaskContentLayerClient : public ContentLayerClient {
 public:
  explicit SolidColorEmptyMaskContentLayerClient(const gfx::Size& bounds)
      : bounds_(bounds) {}
  ~SolidColorEmptyMaskContentLayerClient() override = default;

  bool FillsBoundsCompletely() const override { return false; }

  scoped_refptr<DisplayItemList> PaintContentsToDisplayList() override {
    // Intentionally return a solid color, empty mask display list. This
    // is a situation where all content should be masked out.
    auto display_list = base::MakeRefCounted<DisplayItemList>();
    display_list->Finalize();
    return display_list;
  }

 private:
  gfx::Size bounds_;
};

TEST_P(LayerTreeHostMaskPixelTestWithLayerList, SolidColorEmptyMaskWithEffect) {
  // Apply a mask that is empty and solid-color. This should result in
  // the green layer being entirely clipped out.
  SolidColorEmptyMaskContentLayerClient client(mask_bounds_);
  mask_layer_ = PictureLayer::Create(&client);

  RunPixelResourceTestWithLayerList(base::FilePath(
      FILE_PATH_LITERAL("solid_color_empty_mask_with_effect.png")));
}

// Same as SolidColorEmptyMaskWithEffect, except the mask has a render surface.
class LayerTreeHostMaskPixelTest_SolidColorEmptyMaskWithEffectAndRenderSurface
    : public LayerTreeHostMaskPixelTestWithLayerList {
 protected:
  void SetupTree() override {
    LayerTreeHostMaskPixelTestWithLayerList::SetupTree();

    auto* effect =
        layer_tree_host()->property_trees()->effect_tree_mutable().Node(
            mask_layer_->effect_tree_index());
    effect->render_surface_reason = RenderSurfaceReason::kTest;
  }
};

INSTANTIATE_TEST_SUITE_P(
    PixelResourceTest,
    LayerTreeHostMaskPixelTest_SolidColorEmptyMaskWithEffectAndRenderSurface,
    ::testing::ValuesIn(kTestCases),
    ::testing::PrintToStringParamName());

TEST_P(LayerTreeHostMaskPixelTest_SolidColorEmptyMaskWithEffectAndRenderSurface,
       Test) {
  // Apply a mask that is empty and solid-color. This should result in
  // the green layer being entirely clipped out.
  SolidColorEmptyMaskContentLayerClient client(mask_bounds_);
  mask_layer_ = PictureLayer::Create(&client);

  RunPixelResourceTestWithLayerList(base::FilePath(
      FILE_PATH_LITERAL("solid_color_empty_mask_with_effect.png")));
}

// Tests a situation in which there is no other content in the target
// render surface that the mask applies to. In this situation, the mask
// should have no effect on the rendered output.
class LayerTreeHostMaskPixelTest_MaskWithEffectNoContentToMask
    : public LayerTreeHostMaskPixelTestWithLayerList {
 protected:
  void SetupTree() override {
    LayerTreeHostMaskPixelTestWithLayerList::SetupTree();

    LayerList layers = layer_tree_host()->root_layer()->children();
    DCHECK_EQ(3u, layers.size());
    // Set background to red.
    layers[0]->SetBackgroundColor(SkColors::kRed);
    // Remove the green layer.
    layers.erase(layers.begin() + 1);
    layer_tree_host()->root_layer()->SetChildLayerList(layers);
  }
};

INSTANTIATE_TEST_SUITE_P(
    PixelResourceTest,
    LayerTreeHostMaskPixelTest_MaskWithEffectNoContentToMask,
    ::testing::ValuesIn(kTestCases),
    ::testing::PrintToStringParamName());

TEST_P(LayerTreeHostMaskPixelTest_MaskWithEffectNoContentToMask, Test) {
  MaskContentLayerClient client(mask_bounds_);
  mask_layer_ = PictureLayer::Create(&client);

  RunPixelResourceTestWithLayerList(
      base::FilePath(FILE_PATH_LITERAL("mask_with_effect_no_content.png")));
}

class LayerTreeHostMaskPixelTest_ScaledMaskWithEffect
    : public LayerTreeHostMaskPixelTestWithLayerList {
 protected:
  // Scale the mask with a non-integral transform. This will trigger the
  // AA path in the renderer.
  void SetupTree() override {
    LayerTreeHostMaskPixelTestWithLayerList::SetupTree();

    auto& transform = CreateTransformNode(mask_layer_.get());
    transform.local.Scale(1.5, 1.5);
  }
};

INSTANTIATE_TEST_SUITE_P(PixelResourceTest,
                         LayerTreeHostMaskPixelTest_ScaledMaskWithEffect,
                         ::testing::ValuesIn(kTestCases),
                         ::testing::PrintToStringParamName());

TEST_P(LayerTreeHostMaskPixelTest_ScaledMaskWithEffect, Test) {
  MaskContentLayerClient client(mask_bounds_);
  mask_layer_ = PictureLayer::Create(&client);

  pixel_comparator_ =
      std::make_unique<AlphaDiscardingFuzzyPixelOffByOneComparator>();

  RunPixelResourceTestWithLayerList(
      base::FilePath(FILE_PATH_LITERAL("scaled_mask_with_effect_.png"))
          .InsertBeforeExtensionASCII(GetRendererSuffix()));
}

TEST_P(LayerTreeHostMaskPixelTestWithLayerList, MaskWithEffectDifferentSize) {
  mask_bounds_ = gfx::Size(25, 25);
  MaskContentLayerClient client(mask_bounds_);
  mask_layer_ = PictureLayer::Create(&client);

  pixel_comparator_ =
      std::make_unique<AlphaDiscardingFuzzyPixelOffByOneComparator>();

  // The mask is half the size of thing it's masking. In layer-list mode,
  // the mask is not automatically scaled to match the other layer.
  RunPixelResourceTestWithLayerList(
      base::FilePath(FILE_PATH_LITERAL("mask_with_effect_different_size.png")));
}

TEST_P(LayerTreeHostMaskPixelTestWithLayerList, ImageMaskWithEffect) {
  MaskContentLayerClient mask_client(mask_bounds_);

  sk_sp<SkSurface> surface =
      SkSurfaces::Raster(SkImageInfo::MakeN32Premul(50, 50));
  SkCanvas* canvas = surface->getCanvas();
  scoped_refptr<DisplayItemList> mask_display_list =
      mask_client.PaintContentsToDisplayList();
  mask_display_list->Raster(canvas);

  FakeContentLayerClient layer_client;
  layer_client.set_bounds(mask_bounds_);
  layer_client.add_draw_image(surface->makeImageSnapshot(), gfx::Point());
  mask_layer_ = FakePictureLayer::Create(&layer_client);

  pixel_comparator_ =
      std::make_unique<AlphaDiscardingFuzzyPixelOffByOneComparator>();

  // The mask is half the size of thing it's masking. In layer-list mode,
  // the mask is not automatically scaled to match the other layer.
  RunPixelResourceTestWithLayerList(
      base::FilePath(FILE_PATH_LITERAL("image_mask_with_effect.png")));
}

TEST_P(LayerTreeHostMasksPixelTest, ImageMaskOfLayer) {
  scoped_refptr<SolidColorLayer> background =
      CreateSolidColorLayer(gfx::Rect(100, 100), SK_ColorWHITE);

  gfx::Size mask_bounds(50, 50);

  sk_sp<SkSurface> surface =
      SkSurfaces::Raster(SkImageInfo::MakeN32Premul(50, 50));
  SkCanvas* canvas = surface->getCanvas();
  MaskContentLayerClient client(mask_bounds);
  scoped_refptr<DisplayItemList> mask_display_list =
      client.PaintContentsToDisplayList();
  mask_display_list->Raster(canvas);

  FakeContentLayerClient mask_client;
  mask_client.set_bounds(mask_bounds);
  mask_client.add_draw_image(surface->makeImageSnapshot(), gfx::Point());
  scoped_refptr<FakePictureLayer> mask = FakePictureLayer::Create(&mask_client);
  mask->SetIsDrawable(true);
  mask->SetBounds(mask_bounds);

  scoped_refptr<SolidColorLayer> green = CreateSolidColorLayerWithBorder(
      gfx::Rect(25, 25, 50, 50), kCSSGreen, 1, SK_ColorBLACK);
  green->SetMaskLayer(mask);
  background->AddChild(green);

  pixel_comparator_ =
      std::make_unique<AlphaDiscardingFuzzyPixelOffByOneComparator>();

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
  green->SetMaskLayer(mask);

  pixel_comparator_ =
      std::make_unique<AlphaDiscardingFuzzyPixelOffByOneComparator>();

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
  mask->set_fixed_tile_size(gfx::Size(173, 135));
  green->SetMaskLayer(mask);

  pixel_comparator_ =
      std::make_unique<AlphaDiscardingFuzzyPixelOffByOneComparator>();

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
  scoped_refptr<DisplayItemList> PaintContentsToDisplayList() override {
    auto display_list = base::MakeRefCounted<DisplayItemList>();
    display_list->StartPaint();

    display_list->push<SaveOp>();
    display_list->push<ClipRectOp>(gfx::RectToSkRect(gfx::Rect(bounds_)),
                                   SkClipOp::kIntersect, false);
    SkColor4f color = SkColors::kTransparent;
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
    display_list->EndPaintOfUnpaired(gfx::Rect(bounds_));
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
  scoped_refptr<DisplayItemList> PaintContentsToDisplayList() override {
    auto display_list = base::MakeRefCounted<DisplayItemList>();
    display_list->StartPaint();

    display_list->push<SaveOp>();
    display_list->push<ClipRectOp>(gfx::RectToSkRect(gfx::Rect(bounds_)),
                                   SkClipOp::kIntersect, false);
    SkColor4f color = SkColors::kTransparent;
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
    display_list->EndPaintOfUnpaired(gfx::Rect(bounds_));
    display_list->Finalize();
    return display_list;
  }

 private:
  gfx::Size bounds_;
};

class LayerTreeHostMasksForBackdropFiltersPixelTestWithLayerList
    : public ParameterizedPixelResourceTest {
 protected:
  LayerTreeHostMasksForBackdropFiltersPixelTestWithLayerList()
      : bounds_(100, 100),
        picture_client_(bounds_, SK_ColorGREEN, true),
        mask_client_(bounds_) {
    SetUseLayerLists();
  }

  // Setup three layers for testing masks: a white background, a green layer,
  // and a mask layer with kDstIn blend mode.
  void SetupTree() override {
    SetInitialRootBounds(bounds_);
    ParameterizedPixelResourceTest::SetupTree();

    Layer* root = layer_tree_host()->root_layer();

    scoped_refptr<SolidColorLayer> background =
        CreateSolidColorLayer(gfx::Rect(bounds_), SK_ColorWHITE);
    CopyProperties(root, background.get());
    root->AddChild(background);

    scoped_refptr<PictureLayer> picture =
        PictureLayer::Create(&picture_client_);
    picture->SetBounds(bounds_);
    picture->SetIsDrawable(true);
    CopyProperties(background.get(), picture.get());
    root->AddChild(picture);

    scoped_refptr<SolidColorLayer> blur =
        CreateSolidColorLayer(gfx::Rect(bounds_), SK_ColorTRANSPARENT);
    CopyProperties(background.get(), blur.get());
    CreateEffectNode(blur.get())
        .backdrop_filters.Append(FilterOperation::CreateGrayscaleFilter(1.0));
    root->AddChild(blur);

    scoped_refptr<PictureLayer> mask = PictureLayer::Create(&mask_client_);
    SetupMaskProperties(blur.get(), mask.get());

    root->AddChild(mask);
  }

  const gfx::Size bounds_;
  CheckerContentLayerClient picture_client_;
  CircleContentLayerClient mask_client_;
};

INSTANTIATE_TEST_SUITE_P(
    PixelResourceTest,
    LayerTreeHostMasksForBackdropFiltersPixelTestWithLayerList,
    ::testing::ValuesIn(kTestCases),
    ::testing::PrintToStringParamName());

TEST_P(LayerTreeHostMasksForBackdropFiltersPixelTestWithLayerList, Test) {
  base::FilePath image_name =
      (use_accelerated_raster())
          ? base::FilePath(FILE_PATH_LITERAL("mask_of_backdrop_filter_gpu.png"))
          : base::FilePath(FILE_PATH_LITERAL("mask_of_backdrop_filter.png"));

  if (use_skia_vulkan() && use_accelerated_raster()) {
    // Vulkan with OOP raster has 3 pixels errors (the circle mask shape is
    // slightly different).
    pixel_comparator_ = std::make_unique<FuzzyPixelComparator>(
        FuzzyPixelComparator()
            .DiscardAlpha()
            .SetErrorPixelsPercentageLimit(0.031f)  // 3px / (100*100)
            .SetAbsErrorLimit(182));
  }

  RunPixelResourceTestWithLayerList(image_name);
}

using LayerTreeHostMasksForBackdropFiltersPixelTestWithLayerTree =
    ParameterizedPixelResourceTest;

INSTANTIATE_TEST_SUITE_P(
    PixelResourceTest,
    LayerTreeHostMasksForBackdropFiltersPixelTestWithLayerTree,
    ::testing::ValuesIn(kTestCases),
    ::testing::PrintToStringParamName());

TEST_P(LayerTreeHostMasksForBackdropFiltersPixelTestWithLayerTree, Test) {
  scoped_refptr<SolidColorLayer> background =
      CreateSolidColorLayer(gfx::Rect(100, 100), SK_ColorWHITE);

  gfx::Size picture_bounds(100, 100);
  CheckerContentLayerClient picture_client(picture_bounds, SK_ColorGREEN, true);
  scoped_refptr<PictureLayer> picture = PictureLayer::Create(&picture_client);
  picture->SetBounds(picture_bounds);
  picture->SetIsDrawable(true);

  scoped_refptr<SolidColorLayer> blur =
      CreateSolidColorLayer(gfx::Rect(100, 100), SK_ColorTRANSPARENT);
  background->AddChild(picture);
  background->AddChild(blur);

  FilterOperations filters;
  filters.Append(FilterOperation::CreateGrayscaleFilter(1.0));
  blur->SetBackdropFilters(filters);
  blur->ClearBackdropFilterBounds();

  gfx::Size mask_bounds(100, 100);
  CircleContentLayerClient mask_client(mask_bounds);
  scoped_refptr<PictureLayer> mask = PictureLayer::Create(&mask_client);
  mask->SetBounds(mask_bounds);
  mask->SetIsDrawable(true);
  mask->SetElementId(LayerIdToElementIdForTesting(mask->id()));

  blur->SetMaskLayer(mask);

  base::FilePath image_name =
      (use_accelerated_raster())
          ? base::FilePath(FILE_PATH_LITERAL("mask_of_backdrop_filter_gpu.png"))
          : base::FilePath(FILE_PATH_LITERAL("mask_of_backdrop_filter.png"));

  if (use_skia_vulkan() && use_accelerated_raster()) {
    // Vulkan with OOP raster has 3 pixels errors (the circle mask shape is
    // slightly different).
    pixel_comparator_ = std::make_unique<FuzzyPixelComparator>(
        FuzzyPixelComparator()
            .DiscardAlpha()
            .SetErrorPixelsPercentageLimit(0.031f)  // 3px / (100*100)
            .SetAbsErrorLimit(182));
  }

  RunPixelResourceTest(background, image_name);
}

TEST_P(LayerTreeHostMasksPixelTest, MaskOfLayerWithBlend) {
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
  picture_horizontal->SetMaskLayer(mask);

  pixel_comparator_ = std::make_unique<FuzzyPixelComparator>(
      FuzzyPixelComparator()
          .DiscardAlpha()
          .SetErrorPixelsPercentageLimit(0.04f)  // 0.04%, ~6px / (128*128)
          .SetAbsErrorLimit(255));

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

  scoped_refptr<DisplayItemList> PaintContentsToDisplayList() override {
    return display_list_;
  }
  bool FillsBoundsCompletely() const override { return false; }

 protected:
  explicit StaticPictureLayer(scoped_refptr<DisplayItemList> display_list)
      : PictureLayer(this), display_list_(std::move(display_list)) {}
  ~StaticPictureLayer() override = default;

 private:
  scoped_refptr<DisplayItemList> display_list_;
};

constexpr uint32_t kUseAntialiasing = 1 << 0;
constexpr uint32_t kForceShaders = 1 << 1;

struct MaskTestConfig {
  RasterTestConfig test_config;
  uint32_t flags;
};

void PrintTo(const MaskTestConfig& config, std::ostream* os) {
  PrintTo(config.test_config, os);
  *os << '_' << config.flags;
}

class LayerTreeHostMaskAsBlendingPixelTest
    : public LayerTreeHostPixelResourceTest,
      public ::testing::WithParamInterface<MaskTestConfig> {
 public:
  LayerTreeHostMaskAsBlendingPixelTest()
      : LayerTreeHostPixelResourceTest(GetParam().test_config),
        use_antialiasing_(GetParam().flags & kUseAntialiasing),
        force_shaders_(GetParam().flags & kForceShaders) {
    float percentage_pixels_large_error = 0.f;
    float percentage_pixels_small_error = 0.f;
    float average_error_allowed_in_bad_pixels = 0.f;
    int large_error_allowed = 0;
    int small_error_allowed = 0;
    if (!use_software_renderer()) {
      percentage_pixels_large_error = 4.0f;
#if BUILDFLAG(IS_IOS)
      // iOS has some pixels difference. Affected tests:
      // RotatedClippedCircle, RotatedClippedCircleUnderflow
      // crbug.com/1422694
      percentage_pixels_small_error = 2.7f;
#else
      percentage_pixels_small_error = 2.0f;
#endif  // BUILDFLAG(IS_IOS)
      average_error_allowed_in_bad_pixels = 2.1f;
      large_error_allowed = 11;
      small_error_allowed = 1;
    } else {
#if defined(ARCH_CPU_ARM64)
#if BUILDFLAG(IS_WIN) || BUILDFLAG(IS_FUCHSIA) || BUILDFLAG(IS_APPLE)
      // ARM Windows, macOS, iOS and Fuchsia have some pixels difference
      // Affected tests: RotatedClippedCircle, RotatedClippedCircleUnderflow
      // crbug.com/1030244, crbug.com/1048249, crbug.com/1128443
      percentage_pixels_large_error = 7.f;
      average_error_allowed_in_bad_pixels = 5.f;
      large_error_allowed = 20;
#else
      // Differences in floating point calculation on ARM means a small
      // percentage of pixels will be off by 1.
      percentage_pixels_large_error = 0.112f;
      average_error_allowed_in_bad_pixels = 1.f;
      large_error_allowed = 1;
#endif
#endif
    }

    pixel_comparator_ = std::make_unique<FuzzyPixelComparator>(
        FuzzyPixelComparator()
            .SetErrorPixelsPercentageLimit(percentage_pixels_large_error,
                                           percentage_pixels_small_error)
            .SetAvgAbsErrorLimit(average_error_allowed_in_bad_pixels)
            .SetAbsErrorLimit(large_error_allowed, small_error_allowed));
  }

  static scoped_refptr<Layer> CreateCheckerboardLayer(const gfx::Size& bounds) {
    constexpr int kGridSize = 8;
    static const SkColor4f color_even =
        SkColor4f::FromColor(SkColorSetRGB(153, 153, 153));
    static const SkColor4f color_odd =
        SkColor4f::FromColor(SkColorSetRGB(102, 102, 102));

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
        flags.setColor(test_colors[(i + j * 3) % std::size(test_colors)]);
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
  std::unique_ptr<TestLayerTreeFrameSink> CreateLayerTreeFrameSink(
      const viz::RendererSettings& renderer_settings,
      double refresh_rate,
      scoped_refptr<viz::RasterContextProvider> compositor_context_provider,
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

MaskTestConfig const kTestConfigs[] = {
    MaskTestConfig{{viz::RendererType::kSoftware, TestRasterType::kBitmap}, 0},

// Zero-copy raster is used in production only on Mac and by definition
// relies on SharedImages having scanout support, which is not always the
// case on other platforms and without which these tests would fail when
// run with zero-copy raster.
#if BUILDFLAG(ENABLE_GL_BACKEND_TESTS) && BUILDFLAG(IS_MAC)
    MaskTestConfig{{viz::RendererType::kSkiaGL, TestRasterType::kZeroCopy}, 0},
    MaskTestConfig{{viz::RendererType::kSkiaGL, TestRasterType::kZeroCopy},
                   kUseAntialiasing},
    MaskTestConfig{{viz::RendererType::kSkiaGL, TestRasterType::kZeroCopy},
                   kForceShaders},
    MaskTestConfig{{viz::RendererType::kSkiaGL, TestRasterType::kZeroCopy},
                   kUseAntialiasing | kForceShaders},
#endif  // BUILDFLAG(ENABLE_GL_BACKEND_TESTS) && BUILDFLAG(IS_MAC)
};

INSTANTIATE_TEST_SUITE_P(All,
                         LayerTreeHostMaskAsBlendingPixelTest,
                         ::testing::ValuesIn(kTestConfigs),
                         ::testing::PrintToStringParamName());

TEST_P(LayerTreeHostMaskAsBlendingPixelTest, PixelAlignedNoop) {
  // This test verifies the degenerate case of a no-op mask doesn't affect
  // the contents in any way.
  scoped_refptr<Layer> root = CreateCheckerboardLayer(gfx::Size(400, 300));

  scoped_refptr<Layer> mask_isolation = Layer::Create();
  mask_isolation->SetPosition(gfx::PointF(20, 20));
  mask_isolation->SetBounds(gfx::Size(350, 250));
  mask_isolation->SetMasksToBounds(true);
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
  mask_isolation->SetForceRenderSurfaceForTesting(true);
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
  mask_isolation->SetForceRenderSurfaceForTesting(true);
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
      (raster_type() == TestRasterType::kBitmap)
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
      (raster_type() == TestRasterType::kBitmap)
          ? base::FilePath(FILE_PATH_LITERAL(
                "mask_as_blending_rotated_circle_underflow.png"))
          : base::FilePath(FILE_PATH_LITERAL(
                "mask_as_blending_rotated_circle_underflow_gl.png"));
  RunPixelResourceTest(root, image_name);
}

class LayerTreeHostMasksForBackdropFiltersAndBlendPixelTest
    : public ParameterizedPixelResourceTest {
 protected:
  LayerTreeHostMasksForBackdropFiltersAndBlendPixelTest()
      : bounds_(128, 128),
        picture_client_vertical_(bounds_, SK_ColorGREEN, true),
        picture_client_horizontal_(bounds_, SK_ColorMAGENTA, false),
        mask_client_(bounds_) {
    SetUseLayerLists();
  }

  void SetupTree() override {
    SetInitialRootBounds(bounds_);
    ParameterizedPixelResourceTest::SetupTree();

    Layer* root = layer_tree_host()->root_layer();

    scoped_refptr<SolidColorLayer> background =
        CreateSolidColorLayer(gfx::Rect(bounds_), SK_ColorWHITE);
    CopyProperties(root, background.get());
    root->AddChild(background);

    scoped_refptr<PictureLayer> picture_vertical =
        PictureLayer::Create(&picture_client_vertical_);
    picture_vertical->SetBounds(bounds_);
    picture_vertical->SetIsDrawable(true);
    CopyProperties(background.get(), picture_vertical.get());
    root->AddChild(picture_vertical);

    scoped_refptr<PictureLayer> picture_horizontal =
        PictureLayer::Create(&picture_client_horizontal_);
    picture_horizontal->SetBounds(bounds_);
    picture_horizontal->SetIsDrawable(true);
    picture_horizontal->SetContentsOpaque(false);
    CopyProperties(background.get(), picture_horizontal.get());
    auto& effect_node = CreateEffectNode(picture_horizontal.get());
    effect_node.backdrop_filters.Append(
        FilterOperation::CreateGrayscaleFilter(1.0));
    effect_node.blend_mode = SkBlendMode::kMultiply;
    root->AddChild(picture_horizontal);

    scoped_refptr<PictureLayer> mask = PictureLayer::Create(&mask_client_);
    mask->SetBounds(bounds_);
    SetupMaskProperties(picture_horizontal.get(), mask.get());
    root->AddChild(mask);
  }

  const gfx::Size bounds_;
  CheckerContentLayerClient picture_client_vertical_;
  CheckerContentLayerClient picture_client_horizontal_;
  CircleContentLayerClient mask_client_;
};

INSTANTIATE_TEST_SUITE_P(DISABLED_PixelResourceTest,
                         LayerTreeHostMasksForBackdropFiltersAndBlendPixelTest,
                         ::testing::ValuesIn(kTestCases),
                         ::testing::PrintToStringParamName());

TEST_P(LayerTreeHostMasksForBackdropFiltersAndBlendPixelTest, Test) {
  base::FilePath result_path(
      FILE_PATH_LITERAL("mask_of_backdrop_filter_and_blend_.png"));
  if (!use_accelerated_raster()) {
    result_path = result_path.InsertBeforeExtensionASCII("sw");
  } else {
    result_path = result_path.InsertBeforeExtensionASCII(GetRendererSuffix());
  }
  RunPixelResourceTestWithLayerList(result_path);
}

}  // namespace
}  // namespace cc

#endif  // !BUILDFLAG(IS_ANDROID)
