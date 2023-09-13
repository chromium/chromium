// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "build/build_config.h"
#include "cc/layers/mirror_layer.h"
#include "cc/layers/solid_color_layer.h"
#include "cc/test/layer_tree_pixel_test.h"
#include "cc/test/pixel_comparator.h"
#include "ui/gfx/geometry/size.h"
#include "ui/gfx/geometry/transform_util.h"

#if !BUILDFLAG(IS_ANDROID)

namespace cc {
namespace {

class LayerTreeHostMirrorPixelTest
    : public LayerTreePixelTest,
      public ::testing::WithParamInterface<viz::RendererType> {
 protected:
  LayerTreeHostMirrorPixelTest() : LayerTreePixelTest(renderer_type()) {}

  viz::RendererType renderer_type() const { return GetParam(); }
};

INSTANTIATE_TEST_SUITE_P(All,
                         LayerTreeHostMirrorPixelTest,
                         ::testing::ValuesIn(viz::GetRendererTypes()),
                         ::testing::PrintToStringParamName());

// Verifies that a mirror layer with a scale mirrors another layer correctly.
TEST_P(LayerTreeHostMirrorPixelTest, MirrorLayer) {
  const float scale = 2.f;
  gfx::Rect background_bounds(120, 180);
  gfx::Rect mirrored_bounds(10, 10, 50, 50);
  gfx::Rect mirror_bounds(10, 70, 100, 100);

  auto background = CreateSolidColorLayer(background_bounds, SK_ColorWHITE);

  auto mirrored_layer = CreateSolidColorLayerWithBorder(
      mirrored_bounds, SK_ColorGREEN, 5, SK_ColorBLUE);

  auto mirror_layer = MirrorLayer::Create(mirrored_layer);
  mirror_layer->SetIsDrawable(true);
  mirror_layer->SetBounds(mirror_bounds.size());
  mirror_layer->SetPosition(gfx::PointF(mirror_bounds.origin()));
  mirror_layer->SetTransform(gfx::GetScaleTransform(gfx::Point(), scale));
  background->AddChild(mirrored_layer);
  background->AddChild(mirror_layer);

  if (use_software_renderer()) {
    pixel_comparator_ = std::make_unique<FuzzyPixelComparator>(
        FuzzyPixelComparator()
            .DiscardAlpha()
            .SetErrorPixelsPercentageLimit(3.f)
            .SetAvgAbsErrorLimit(65.f)
            .SetAbsErrorLimit(120));
  }

  if (use_skia_vulkan() || use_skia_graphite()) {
    pixel_comparator_ =
        std::make_unique<AlphaDiscardingFuzzyPixelOffByOneComparator>();
  }

  RunPixelTest(background,
               base::FilePath(FILE_PATH_LITERAL("mirror_layer.png")));
}

}  // namespace
}  // namespace cc

#endif  // !BUILDFLAG(IS_ANDROID)
