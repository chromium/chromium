// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <stddef.h>

#include "build/build_config.h"
#include "cc/input/scrollbar.h"
#include "cc/layers/nine_patch_thumb_scrollbar_layer.h"
#include "cc/layers/painted_scrollbar_layer.h"
#include "cc/layers/solid_color_layer.h"
#include "cc/paint/paint_canvas.h"
#include "cc/paint/paint_flags.h"
#include "cc/test/fake_scrollbar.h"
#include "cc/test/layer_tree_pixel_test.h"
#include "cc/test/pixel_comparator.h"
#include "cc/trees/layer_tree_impl.h"
#include "components/viz/test/test_in_process_context_provider.h"

#if !BUILDFLAG(IS_ANDROID)

namespace cc {
namespace {

class LayerTreeHostScrollbarsPixelTest
    : public LayerTreePixelTest,
      public ::testing::WithParamInterface<viz::RendererType> {
 protected:
  LayerTreeHostScrollbarsPixelTest() : LayerTreePixelTest(renderer_type()) {}

  viz::RendererType renderer_type() const { return GetParam(); }

  void SetupTree() override {
    SetInitialDeviceScaleFactor(device_scale_factor_);
    LayerTreePixelTest::SetupTree();
  }

  float device_scale_factor_ = 1.f;
};

class PaintedScrollbar : public FakeScrollbar {
 public:
  explicit PaintedScrollbar(const gfx::Size& size) {
    set_should_paint(true);
    set_has_thumb(false);
    set_track_rect(gfx::Rect(size));
  }

  void set_paint_scale(int scale) { paint_scale_ = scale; }

 private:
  void Paint(PaintCanvas& canvas, const gfx::Rect& rect) override {
    PaintFlags flags;
    flags.setStyle(PaintFlags::kStroke_Style);
    flags.setStrokeWidth(SkIntToScalar(paint_scale_));
    flags.setColor(color_);
    gfx::Rect inset_rect = rect;
    while (!inset_rect.IsEmpty()) {
      int big_rect = paint_scale_ + 2;
      int small_rect = paint_scale_;
      inset_rect.Inset(
          gfx::Insets::TLBR(big_rect, big_rect, small_rect, small_rect));
      canvas.drawRect(RectToSkRect(inset_rect), flags);
      inset_rect.Inset(
          gfx::Insets::TLBR(big_rect, big_rect, small_rect, small_rect));
    }
  }

  ~PaintedScrollbar() override = default;

  int paint_scale_ = 4;
  SkColor color_ = SK_ColorGREEN;
};

INSTANTIATE_TEST_SUITE_P(All,
                         LayerTreeHostScrollbarsPixelTest,
                         ::testing::ValuesIn(viz::GetGpuRendererTypes()),
                         ::testing::PrintToStringParamName());

// viz::GetGpuRendererTypes() can return an empty list on some platforms.
GTEST_ALLOW_UNINSTANTIATED_PARAMETERIZED_TEST(LayerTreeHostScrollbarsPixelTest);

TEST_P(LayerTreeHostScrollbarsPixelTest, NoScale) {
  scoped_refptr<SolidColorLayer> background =
      CreateSolidColorLayer(gfx::Rect(200, 200), SK_ColorWHITE);

  auto scrollbar = base::MakeRefCounted<PaintedScrollbar>(gfx::Size(200, 200));
  scoped_refptr<PaintedScrollbarLayer> layer =
      PaintedScrollbarLayer::Create(std::move(scrollbar));
  layer->SetIsDrawable(true);
  layer->SetBounds(gfx::Size(200, 200));
  background->AddChild(layer);

  RunPixelTest(background, base::FilePath(FILE_PATH_LITERAL("spiral.png")));
}

TEST_P(LayerTreeHostScrollbarsPixelTest, DeviceScaleFactor) {
  // With a device scale of 2, the scrollbar should still be rendered
  // pixel-perfect, not show scaling artifacts
  device_scale_factor_ = 2.f;

  scoped_refptr<SolidColorLayer> background =
      CreateSolidColorLayer(gfx::Rect(100, 100), SK_ColorWHITE);

  auto scrollbar = base::MakeRefCounted<PaintedScrollbar>(gfx::Size(100, 100));
  scoped_refptr<PaintedScrollbarLayer> layer =
      PaintedScrollbarLayer::Create(std::move(scrollbar));
  layer->SetIsDrawable(true);
  layer->SetBounds(gfx::Size(100, 100));
  background->AddChild(layer);

  RunPixelTest(background,
               base::FilePath(FILE_PATH_LITERAL("spiral_double_scale.png")));
}

TEST_P(LayerTreeHostScrollbarsPixelTest, TransformScale) {
  scoped_refptr<SolidColorLayer> background =
      CreateSolidColorLayer(gfx::Rect(200, 200), SK_ColorWHITE);

  auto scrollbar = base::MakeRefCounted<PaintedScrollbar>(gfx::Size(100, 100));
  scoped_refptr<PaintedScrollbarLayer> layer =
      PaintedScrollbarLayer::Create(std::move(scrollbar));
  layer->SetIsDrawable(true);
  layer->SetBounds(gfx::Size(100, 100));
  background->AddChild(layer);

  // This has a scale of 2, it should still be rendered pixel-perfect, not show
  // scaling artifacts
  gfx::Transform scale_transform;
  scale_transform.Scale(2.0, 2.0);
  layer->SetTransform(scale_transform);

  RunPixelTest(background,
               base::FilePath(FILE_PATH_LITERAL("spiral_double_scale.png")));
}

// Disabled on TSan due to frequent timeouts. crbug.com/848994
// TODO(crbug.com/40256786): currently do not pass on iOS.
#if defined(THREAD_SANITIZER) || BUILDFLAG(IS_IOS)
#define MAYBE_HugeTransformScale DISABLED_HugeTransformScale
#else
#define MAYBE_HugeTransformScale HugeTransformScale
#endif
TEST_P(LayerTreeHostScrollbarsPixelTest, MAYBE_HugeTransformScale) {
  scoped_refptr<SolidColorLayer> background =
      CreateSolidColorLayer(gfx::Rect(400, 400), SK_ColorWHITE);

  auto scrollbar = base::MakeRefCounted<PaintedScrollbar>(gfx::Size(10, 400));
  scrollbar->set_paint_scale(1);
  scoped_refptr<PaintedScrollbarLayer> layer =
      PaintedScrollbarLayer::Create(std::move(scrollbar));
  layer->SetIsDrawable(true);
  layer->SetBounds(gfx::Size(10, 400));
  background->AddChild(layer);

  // We want a scale that creates a texture taller than the max texture size. If
  // there's no clamping, the texture will be invalid and we'll just get black.
  double scale = 64.0;
  ASSERT_GT(scale * layer->bounds().height(), max_texture_size_);

  // Let's show the bottom right of the layer, so we know the texture wasn't
  // just cut off.
  layer->SetPosition(
      gfx::PointF(-10.f * scale + 400.f, -400.f * scale + 400.f));

  gfx::Transform scale_transform;
  scale_transform.Scale(scale, scale);
  layer->SetTransform(scale_transform);

  pixel_comparator_ =
      std::make_unique<AlphaDiscardingFuzzyPixelOffByOneComparator>();

  base::FilePath expected_result =
      base::FilePath(FILE_PATH_LITERAL("spiral_64_scale.png"));
  if (use_skia_graphite()) {
    expected_result = expected_result.InsertBeforeExtensionASCII("_graphite");
  }
  if (use_skia_vulkan()) {
    expected_result = expected_result.InsertBeforeExtensionASCII("_vk");
  }
  RunPixelTest(background, expected_result);
}

class LayerTreeHostOverlayScrollbarsPixelTest
    : public LayerTreeHostScrollbarsPixelTest {
 protected:
  LayerTreeHostOverlayScrollbarsPixelTest() = default;

  void DidActivateTreeOnThread(LayerTreeHostImpl* host_impl) override {
    LayerImpl* layer = host_impl->active_tree()->LayerById(scrollbar_layer_id_);
    ToScrollbarLayer(layer)->SetThumbThicknessScaleFactor(thickness_scale_);
  }

  int scrollbar_layer_id_;
  float thickness_scale_;
};

class NinePatchThumbScrollbar : public FakeScrollbar {
 public:
  NinePatchThumbScrollbar() {
    set_should_paint(true);
    set_has_thumb(true);
    set_orientation(ScrollbarOrientation::kVertical);
    set_is_overlay(true);
    set_thumb_size(gfx::Size(15, 50));
    set_track_rect(gfx::Rect(0, 0, 15, 400));
  }

  bool UsesNinePatchThumbResource() const override { return true; }
  gfx::Size NinePatchThumbCanvasSize() const override {
    return gfx::Size(7, 7);
  }
  gfx::Rect NinePatchThumbAperture() const override {
    return gfx::Rect(3, 3, 1, 1);
  }

 private:
  void Paint(PaintCanvas& canvas, const gfx::Rect& rect) override {
    // The outside of the rect will be painted with a 1 pixel black, red, then
    // blue border. The inside will be solid blue. This will allow the test to
    // ensure that scaling the thumb doesn't scale the border at all.  Note
    // that the inside of the border must be the same color as the center tile
    // to prevent an interpolation from being applied.
    PaintFlags flags;
    flags.setStyle(PaintFlags::kFill_Style);
    flags.setStrokeWidth(SkIntToScalar(1));
    flags.setColor(SK_ColorBLACK);

    gfx::Rect inset_rect = rect;
    canvas.drawRect(RectToSkRect(inset_rect), flags);

    flags.setColor(SK_ColorRED);
    inset_rect.Inset(1);
    canvas.drawRect(RectToSkRect(inset_rect), flags);

    flags.setColor(SK_ColorBLUE);
    inset_rect.Inset(1);
    canvas.drawRect(RectToSkRect(inset_rect), flags);
  }

  ~NinePatchThumbScrollbar() override = default;
};

INSTANTIATE_TEST_SUITE_P(All,
                         LayerTreeHostOverlayScrollbarsPixelTest,
                         ::testing::ValuesIn(viz::GetGpuRendererTypes()),
                         ::testing::PrintToStringParamName());

// viz::GetGpuRendererTypes() can return an empty list on some platforms.
GTEST_ALLOW_UNINSTANTIATED_PARAMETERIZED_TEST(
    LayerTreeHostOverlayScrollbarsPixelTest);

// Simulate increasing the thickness of a NinePatchThumbScrollbar. Ensure that
// the scrollbar border remains crisp.
TEST_P(LayerTreeHostOverlayScrollbarsPixelTest, NinePatchScrollbarScaledUp) {
  scoped_refptr<SolidColorLayer> background =
      CreateSolidColorLayer(gfx::Rect(400, 400), SK_ColorWHITE);

  auto scrollbar = base::MakeRefCounted<NinePatchThumbScrollbar>();
  scoped_refptr<NinePatchThumbScrollbarLayer> layer =
      NinePatchThumbScrollbarLayer::Create(std::move(scrollbar));

  scrollbar_layer_id_ = layer->id();
  thickness_scale_ = 5.f;

  layer->SetIsDrawable(true);
  layer->SetBounds(gfx::Size(10, 300));
  background->AddChild(layer);

  layer->SetPosition(gfx::PointF(185, 10));

  RunPixelTest(
      background,
      base::FilePath(FILE_PATH_LITERAL("overlay_scrollbar_scaled_up.png")));
}

// Simulate decreasing the thickness of a NinePatchThumbScrollbar. Ensure that
// the scrollbar border remains crisp.
TEST_P(LayerTreeHostOverlayScrollbarsPixelTest, NinePatchScrollbarScaledDown) {
  scoped_refptr<SolidColorLayer> background =
      CreateSolidColorLayer(gfx::Rect(400, 400), SK_ColorWHITE);

  auto scrollbar = base::MakeRefCounted<NinePatchThumbScrollbar>();
  scoped_refptr<NinePatchThumbScrollbarLayer> layer =
      NinePatchThumbScrollbarLayer::Create(std::move(scrollbar));

  scrollbar_layer_id_ = layer->id();
  thickness_scale_ = 0.4f;

  layer->SetIsDrawable(true);
  layer->SetBounds(gfx::Size(10, 300));
  background->AddChild(layer);

  layer->SetPosition(gfx::PointF(185, 10));

  RunPixelTest(
      background,
      base::FilePath(FILE_PATH_LITERAL("overlay_scrollbar_scaled_down.png")));
}

}  // namespace
}  // namespace cc

#endif  // BUILDFLAG(IS_ANDROID)
