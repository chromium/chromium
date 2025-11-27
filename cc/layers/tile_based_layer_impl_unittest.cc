// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/layers/tile_based_layer_impl.h"

#include <memory>
#include <utility>
#include <vector>

#include "base/check_deref.h"
#include "cc/debug/layer_tree_debug_state.h"
#include "cc/layers/append_quads_context.h"
#include "cc/layers/append_quads_data.h"
#include "cc/test/layer_test_common.h"
#include "cc/test/test_layer_tree_host_base.h"
#include "cc/tiles/tile_draw_info.h"
#include "cc/tiles/tile_priority.h"
#include "cc/tiles/tiling_coverage_iterator.h"
#include "cc/tiles/tiling_set_coverage_iterator.h"
#include "components/viz/common/quads/solid_color_draw_quad.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace cc {
namespace {

class FakeTile {
 public:
  FakeTile() = default;
  ~FakeTile() = default;

  TileDrawInfo::Mode draw_mode() { return TileDrawInfo::SOLID_COLOR_MODE; }

  bool IsReadyToDraw() const { return true; }
};

class FakeTilingCoverageIterator;

class FakeTiling {
 public:
  using Tile = FakeTile;
  using CoverageIterator = FakeTilingCoverageIterator;

  FakeTiling() = default;
  ~FakeTiling() = default;

  CoverageIterator Cover(const gfx::Rect& coverage_rect,
                         float coverage_scale) const;

  Tile* TileAt(const TileIndex& index) const { return nullptr; }
  float contents_scale_key() const { return 1.0f; }
  const TilingData* tiling_data() const { return nullptr; }
  gfx::Size raster_size() const { return gfx::Size{100, 100}; }
  const gfx::AxisTransform2d& raster_transform() const {
    return raster_transform_;
  }

 private:
  gfx::AxisTransform2d raster_transform_;
};

class FakeTilingCoverageIterator : public TilingCoverageIterator<FakeTiling> {
 public:
  using TilingCoverageIterator<FakeTiling>::TilingCoverageIterator;
};

class TestTileBasedLayerImpl : public TileBasedLayerImpl<FakeTiling> {
 public:
  TestTileBasedLayerImpl(LayerTreeImpl* tree_impl, int id)
      : TileBasedLayerImpl<FakeTiling>(tree_impl, id) {}

 private:
  // TileBasedLayerImpl:
  void AppendQuadsSpecialization(const AppendQuadsContext& context,
                                 viz::CompositorRenderPass* render_pass,
                                 AppendQuadsData* append_quads_data,
                                 viz::SharedQuadState* shared_quad_state,
                                 const Occlusion& scaled_occlusion,
                                 const gfx::Vector2d& quad_offset) override {}
  float GetMaximumContentsScaleForUseInAppendQuads() override { return 1.f; }
  bool IsDirectlyCompositedImage() const override { return false; }
  TilingResolution GetTilingResolutionForDebugBorders(
      const FakeTiling* tiling) const override {
    return TilingResolution::kHigh;
  }
  void AppendQuadsForResourcelessSoftwareDraw(
      const AppendQuadsContext& context,
      viz::CompositorRenderPass* render_pass,
      AppendQuadsData* append_quads_data,
      viz::SharedQuadState* shared_quad_state,
      const Occlusion& scaled_occlusion) override {}
  TilingSetCoverageIterator<FakeTiling> Cover(
      const gfx::Rect& coverage_rect,
      float coverage_scale,
      float ideal_contents_scale) override {
    return TilingSetCoverageIterator<FakeTiling>(
        empty_tilings_, coverage_rect, coverage_scale, ideal_contents_scale);
  }
  float GetIdealContentsScaleKey() const override { return 1.f; }

  // Note: TilingSetCoverageIterator stores its passed-in container as a const
  // ref, so it's necessary to pass in an object that outlives the
  // TilingSetCoverageIterator instance.
  std::vector<std::unique_ptr<FakeTiling>> empty_tilings_;
};

FakeTiling::CoverageIterator FakeTiling::Cover(const gfx::Rect& coverage_rect,
                                               float coverage_scale) const {
  return CoverageIterator();
}

class TileBasedLayerImplTest : public TestLayerTreeHostBase {};

// Verifies that `is_backdrop_filter_mask()` returns false by default.
TEST_F(TileBasedLayerImplTest, IsBackdropFilterMask_DefaultsToFalse) {
  auto layer = std::make_unique<TestTileBasedLayerImpl>(
      host_impl()->active_tree(), /*id=*/1);
  EXPECT_FALSE(layer->is_backdrop_filter_mask());
}

// Verifies that `is_backdrop_filter_mask()` reflects calls to
// `SetIsBackdropFilterMask()`.
TEST_F(TileBasedLayerImplTest, SetIsBackdropFilterMask_GetterReflectsSetter) {
  auto layer = std::make_unique<TestTileBasedLayerImpl>(
      host_impl()->active_tree(), /*id=*/1);

  layer->SetIsBackdropFilterMask(true);
  EXPECT_TRUE(layer->is_backdrop_filter_mask());

  layer->SetIsBackdropFilterMask(false);
  EXPECT_FALSE(layer->is_backdrop_filter_mask());
}

// Verifies that calling `SetIsBackdropFilterMask` with the same value multiple
// times doesn't change the value.
TEST_F(TileBasedLayerImplTest, SetIsBackdropFilterMask_RedundantCalls) {
  auto layer = std::make_unique<TestTileBasedLayerImpl>(
      host_impl()->active_tree(), /*id=*/1);

  layer->SetIsBackdropFilterMask(true);
  EXPECT_TRUE(layer->is_backdrop_filter_mask());
  layer->SetIsBackdropFilterMask(true);
  EXPECT_TRUE(layer->is_backdrop_filter_mask());

  layer->SetIsBackdropFilterMask(false);
  EXPECT_FALSE(layer->is_backdrop_filter_mask());
  layer->SetIsBackdropFilterMask(false);
  EXPECT_FALSE(layer->is_backdrop_filter_mask());
}

// Verifies that calling `SetIsBackdropFilterMask` with a new value marks the
// layer for push properties in the pending tree.
TEST_F(TileBasedLayerImplTest,
       SetIsBackdropFilterMask_CallsSetNeedsPushProperties) {
  // NOTE: LayerImpl::SetNeedsPushProperties() is a no-op in the active tree, so
  // we need to put the layer in the pending tree here.
  SetupPendingTree();
  auto layer = std::make_unique<TestTileBasedLayerImpl>(
      host_impl()->pending_tree(), /*id=*/42);
  auto* raw_layer = layer.get();
  host_impl()->pending_tree()->AddLayer(std::move(layer));

  // Clear state before running the test.
  host_impl()->pending_tree()->ClearLayersThatShouldPushProperties();
  raw_layer->ResetChangeTracking();

  raw_layer->SetIsBackdropFilterMask(true);

  const auto& layers =
      host_impl()->pending_tree()->LayersThatShouldPushProperties();
  ASSERT_EQ(layers.size(), 1u);
  EXPECT_NE(layers.find(raw_layer), layers.end());
}

// Verifies that calling `SetIsBackdropFilterMask` with the same value as it
// currently has does not mark the layer for push properties in the pending
// tree.
TEST_F(
    TileBasedLayerImplTest,
    SetIsBackdropFilterMask_DoesNotCallSetNeedsPushPropertiesIfValueUnchanged) {
  // NOTE: LayerImpl::SetNeedsPushProperties() is a no-op in the active tree, so
  // we need to put the layer in the pending tree here.
  SetupPendingTree();
  auto layer = std::make_unique<TestTileBasedLayerImpl>(
      host_impl()->pending_tree(), /*id=*/42);
  auto* raw_layer = layer.get();
  host_impl()->pending_tree()->AddLayer(std::move(layer));

  // Clear state before running the test.
  host_impl()->pending_tree()->ClearLayersThatShouldPushProperties();
  raw_layer->ResetChangeTracking();

  raw_layer->SetIsBackdropFilterMask(false);

  const auto& layers =
      host_impl()->pending_tree()->LayersThatShouldPushProperties();
  EXPECT_EQ(layers.size(), 0u);
}

// Tests that AppendQuads() does not append any quads for a layer serving as
// a backdrop filter mask.
TEST_F(TileBasedLayerImplTest,
       AppendQuadsDoesNotAppendQuadsForBackdropFilterMask) {
  constexpr gfx::Size kLayerBounds(1300, 1900);
  constexpr gfx::Rect kLayerRect(kLayerBounds);
  constexpr float kOpacity = 1.0;

  auto layer = std::make_unique<TestTileBasedLayerImpl>(
      host_impl()->active_tree(), /*id=*/42);
  auto* raw_layer = layer.get();
  host_impl()->active_tree()->AddLayer(std::move(layer));

  raw_layer->SetIsBackdropFilterMask(true);

  // For the production code to actually append a quad, the layer must have
  // non-zero size and not be completely transparent; ensure that these
  // preconditions are satisfied to avoid this test passing trivially.
  raw_layer->SetBounds(kLayerBounds);
  raw_layer->draw_properties().visible_layer_rect = kLayerRect;
  raw_layer->draw_properties().opacity = kOpacity;

  SetupRootProperties(host_impl()->active_tree()->root_layer());

  auto render_pass = viz::CompositorRenderPass::Create();
  AppendQuadsData data;
  raw_layer->AppendQuads(AppendQuadsContext{DRAW_MODE_SOFTWARE, {}, false},
                         render_pass.get(), &data);

  EXPECT_EQ(render_pass->quad_list.size(), 0u);
}

// Tests that AppendQuads() does not append any quads for a layer serving as
// a backdrop filter mask with a solid color set.
TEST_F(TileBasedLayerImplTest,
       AppendQuadsDoesNotAppendQuadsForBackdropFilterMaskWithSolidColor) {
  constexpr gfx::Size kLayerBounds(1300, 1900);
  constexpr gfx::Rect kLayerRect(kLayerBounds);
  constexpr SkColor4f kLayerColor = SkColors::kRed;
  constexpr float kOpacity = 1.0;

  auto layer = std::make_unique<TestTileBasedLayerImpl>(
      host_impl()->active_tree(), /*id=*/42);
  auto* raw_layer = layer.get();
  host_impl()->active_tree()->AddLayer(std::move(layer));

  raw_layer->SetIsBackdropFilterMask(true);
  raw_layer->SetSolidColor(kLayerColor);

  // For the production code to actually append a quad, the layer must have
  // non-zero size and not be completely transparent; ensure that these
  // preconditions are satisfied to avoid this test passing trivially.
  raw_layer->SetBounds(kLayerBounds);
  raw_layer->draw_properties().visible_layer_rect = kLayerRect;
  raw_layer->draw_properties().opacity = kOpacity;

  SetupRootProperties(host_impl()->active_tree()->root_layer());

  auto render_pass = viz::CompositorRenderPass::Create();
  AppendQuadsData data;
  raw_layer->AppendQuads(AppendQuadsContext{DRAW_MODE_SOFTWARE, {}, false},
                         render_pass.get(), &data);

  EXPECT_EQ(render_pass->quad_list.size(), 0u);
}

TEST_F(TileBasedLayerImplTest, SettingSolidColorResultsInSolidColorQuad) {
  constexpr gfx::Size kLayerBounds(1300, 1900);
  constexpr gfx::Rect kLayerRect(kLayerBounds);
  constexpr SkColor4f kLayerColor = SkColors::kRed;
  constexpr float kOpacity = 1.0;

  auto layer = std::make_unique<TestTileBasedLayerImpl>(
      host_impl()->active_tree(), /*id=*/42);
  auto* raw_layer = layer.get();
  host_impl()->active_tree()->AddLayer(std::move(layer));

  raw_layer->SetSolidColor(kLayerColor);

  // For the production code to actually append a quad, the layer must have
  // non-zero size and not be completely transparent.
  raw_layer->SetBounds(kLayerBounds);
  raw_layer->draw_properties().visible_layer_rect = kLayerRect;
  raw_layer->draw_properties().opacity = kOpacity;

  SetupRootProperties(host_impl()->active_tree()->root_layer());

  auto render_pass = viz::CompositorRenderPass::Create();
  AppendQuadsData data;
  raw_layer->AppendQuads(AppendQuadsContext{DRAW_MODE_SOFTWARE, {}, false},
                         render_pass.get(), &data);

  EXPECT_EQ(render_pass->quad_list.size(), 1u);
  EXPECT_EQ(render_pass->quad_list.front()->rect, kLayerRect);
  EXPECT_EQ(render_pass->quad_list.front()->visible_rect, kLayerRect);
  EXPECT_EQ(render_pass->quad_list.front()->shared_quad_state->opacity,
            kOpacity);
  EXPECT_EQ(render_pass->quad_list.front()->material,
            viz::DrawQuad::Material::kSolidColor);
  EXPECT_EQ(
      viz::SolidColorDrawQuad::MaterialCast(render_pass->quad_list.front())
          ->color,
      kLayerColor);
}

// Validates that TileBasedLayerImpl::AppendQuads() calls
// LayerImpl::AppendDebugBorderQuad() when debug borders are enabled.
TEST_F(TileBasedLayerImplTest, AppendQuadsAppendsDebugBorders) {
  LayerTreeDebugState debug_state;
  debug_state.show_debug_borders.set(DebugBorderType::LAYER);
  host_impl()->SetDebugState(debug_state);

  auto layer = std::make_unique<TestTileBasedLayerImpl>(
      host_impl()->active_tree(), /*id=*/42);
  auto* raw_layer = layer.get();
  host_impl()->active_tree()->AddLayer(std::move(layer));

  SetupRootProperties(host_impl()->active_tree()->root_layer());

  auto render_pass = viz::CompositorRenderPass::Create();
  AppendQuadsData data;
  raw_layer->AppendQuads(AppendQuadsContext{DRAW_MODE_SOFTWARE, {}, false},
                         render_pass.get(), &data);

  // AppendQuads() should have inserted a quad for the debug border.
  EXPECT_EQ(render_pass->quad_list.size(), 1u);
  EXPECT_EQ(render_pass->quad_list.front()->material,
            viz::DrawQuad::Material::kDebugBorder);
}

TEST_F(TileBasedLayerImplTest,
       AppendQuadsDoesNotSetClipRectWhenNotDirectlyCompositedImage) {
  constexpr gfx::Size kLayerBounds(100, 200);
  constexpr gfx::Rect kLayerRect(kLayerBounds);

  auto layer = std::make_unique<TestTileBasedLayerImpl>(
      host_impl()->active_tree(), /*id=*/1);
  auto* raw_layer = layer.get();
  host_impl()->active_tree()->AddLayer(std::move(layer));

  raw_layer->SetBounds(kLayerBounds);
  raw_layer->draw_properties().visible_layer_rect = kLayerRect;

  SetupRootProperties(host_impl()->active_tree()->root_layer());

  auto render_pass = viz::CompositorRenderPass::Create();
  AppendQuadsData data;
  raw_layer->AppendQuads(AppendQuadsContext{DRAW_MODE_SOFTWARE, {}, false},
                         render_pass.get(), &data);

  ASSERT_EQ(render_pass->shared_quad_state_list.size(), 1u);
  EXPECT_FALSE(render_pass->shared_quad_state_list.front()->clip_rect);
}

class DirectlyCompositedTileBasedLayerImpl : public TestTileBasedLayerImpl {
 public:
  DirectlyCompositedTileBasedLayerImpl(LayerTreeImpl* tree_impl, int id)
      : TestTileBasedLayerImpl(tree_impl, id) {}

 private:
  bool IsDirectlyCompositedImage() const override { return true; }
  float GetIdealContentsScaleKey() const override { return 1.f; }
};

TEST_F(TileBasedLayerImplTest,
       AppendQuadsSetsClipRectForDirectlyCompositedImage) {
  constexpr gfx::Size kLayerBounds(100, 200);
  constexpr gfx::Rect kLayerRect(kLayerBounds);

  auto layer = std::make_unique<DirectlyCompositedTileBasedLayerImpl>(
      host_impl()->active_tree(), /*id=*/1);
  auto* raw_layer = layer.get();
  host_impl()->active_tree()->AddLayer(std::move(layer));

  raw_layer->SetBounds(kLayerBounds);
  raw_layer->draw_properties().visible_layer_rect = kLayerRect;

  SetupRootProperties(host_impl()->active_tree()->root_layer());

  auto render_pass = viz::CompositorRenderPass::Create();
  AppendQuadsData data;
  raw_layer->AppendQuads(AppendQuadsContext{DRAW_MODE_SOFTWARE, {}, false},
                         render_pass.get(), &data);

  ASSERT_EQ(render_pass->shared_quad_state_list.size(), 1u);
  EXPECT_EQ(render_pass->shared_quad_state_list.front()->clip_rect, kLayerRect);
}

class OcclusionTestTileBasedLayerImpl : public TestTileBasedLayerImpl {
 public:
  OcclusionTestTileBasedLayerImpl(LayerTreeImpl* tree_impl, int id)
      : TestTileBasedLayerImpl(tree_impl, id) {}

  const Occlusion& scaled_occlusion() const { return scaled_occlusion_; }
  void set_max_contents_scale(float scale) { max_contents_scale_ = scale; }

 private:
  void AppendQuadsSpecialization(const AppendQuadsContext& context,
                                 viz::CompositorRenderPass* render_pass,
                                 AppendQuadsData* append_quads_data,
                                 viz::SharedQuadState* shared_quad_state,
                                 const Occlusion& scaled_occlusion,
                                 const gfx::Vector2d& quad_offset) override {
    scaled_occlusion_ = scaled_occlusion;
    // Create a dummy quad to avoid tripping debug checks.
    auto* quad =
        render_pass->CreateAndAppendDrawQuad<viz::SolidColorDrawQuad>();
    quad->SetNew(shared_quad_state, gfx::Rect(1, 1), gfx::Rect(1, 1),
                 SkColors::kTransparent, false);
  }
  float GetMaximumContentsScaleForUseInAppendQuads() override {
    return max_contents_scale_;
  }
  float GetIdealContentsScaleKey() const override { return 1.f; }

  Occlusion scaled_occlusion_;
  float max_contents_scale_ = 1.f;
};

class ResourcelessSoftwareDrawTileBasedLayerImpl
    : public TestTileBasedLayerImpl {
 public:
  ResourcelessSoftwareDrawTileBasedLayerImpl(LayerTreeImpl* tree_impl, int id)
      : TestTileBasedLayerImpl(tree_impl, id) {}

  bool append_quads_for_resourceless_software_draw_called() const {
    return append_quads_for_resourceless_software_draw_called_;
  }

 private:
  void AppendQuadsForResourcelessSoftwareDraw(
      const AppendQuadsContext& context,
      viz::CompositorRenderPass* render_pass,
      AppendQuadsData* append_quads_data,
      viz::SharedQuadState* shared_quad_state,
      const Occlusion& scaled_occlusion) override {
    append_quads_for_resourceless_software_draw_called_ = true;
    // Create a dummy quad to avoid tripping debug checks.
    auto* quad =
        render_pass->CreateAndAppendDrawQuad<viz::SolidColorDrawQuad>();
    quad->SetNew(shared_quad_state, gfx::Rect(1, 1), gfx::Rect(1, 1),
                 SkColors::kTransparent, false);
  }
  float GetIdealContentsScaleKey() const override { return 1.f; }

  bool append_quads_for_resourceless_software_draw_called_ = false;
};

TEST_F(TileBasedLayerImplTest, AppendQuadsScalesOcclusion) {
  const float scale = 2.0f;
  const gfx::Rect layer_rect(0, 0, 10, 10);
  const gfx::Rect occluded_rect_in_content_space(0, 0, 5, 10);

  auto layer = std::make_unique<OcclusionTestTileBasedLayerImpl>(
      host_impl()->active_tree(), /*id=*/1);
  auto* raw_layer = layer.get();
  host_impl()->active_tree()->AddLayer(std::move(layer));

  raw_layer->set_max_contents_scale(scale);
  raw_layer->SetBounds(layer_rect.size());
  raw_layer->draw_properties().visible_layer_rect = layer_rect;

  // Set a screen space transform to simulate the layer being scaled. This is
  // crucial because scaled_occlusion is computed using this transform.
  gfx::Transform screen_space_transform;
  screen_space_transform.Scale(scale, scale);
  raw_layer->draw_properties().screen_space_transform = screen_space_transform;

  // Define an occlusion in the layer's content space. This will be scaled
  // by AppendQuads to produce the scaled_occlusion.
  raw_layer->draw_properties().occlusion_in_content_space = Occlusion(
      gfx::Transform(), SimpleEnclosedRegion(occluded_rect_in_content_space),
      SimpleEnclosedRegion());

  SetupRootProperties(host_impl()->active_tree()->root_layer());

  auto render_pass = viz::CompositorRenderPass::Create();
  AppendQuadsData data;
  raw_layer->AppendQuads(AppendQuadsContext{DRAW_MODE_SOFTWARE, {}, true},
                         render_pass.get(), &data);

  // The expected occluded rectangle is the original content-space occluded
  // rectangle scaled to the layer's target space (which is screen space in this
  // test due to the screen space transform).
  const gfx::Rect expected_occluded_rect =
      gfx::ScaleToEnclosingRect(occluded_rect_in_content_space, scale);

  auto enclosing_rect = gfx::ScaleToEnclosingRect(layer_rect, scale);
  EXPECT_EQ(
      raw_layer->scaled_occlusion().GetUnoccludedContentRect(enclosing_rect),
      gfx::SubtractRects(enclosing_rect, expected_occluded_rect));
}

TEST_F(TileBasedLayerImplTest,
       AppendQuadsInvokesAppendQuadsForResourcelessSoftwareDraw) {
  constexpr gfx::Size kLayerBounds(100, 200);
  constexpr gfx::Rect kLayerRect(kLayerBounds);

  auto layer = std::make_unique<ResourcelessSoftwareDrawTileBasedLayerImpl>(
      host_impl()->active_tree(), /*id=*/1);
  auto* raw_layer = layer.get();
  host_impl()->active_tree()->AddLayer(std::move(layer));

  raw_layer->SetBounds(kLayerBounds);
  raw_layer->draw_properties().visible_layer_rect = kLayerRect;

  SetupRootProperties(host_impl()->active_tree()->root_layer());

  auto render_pass = viz::CompositorRenderPass::Create();
  AppendQuadsData data;
  raw_layer->AppendQuads(
      AppendQuadsContext{DRAW_MODE_RESOURCELESS_SOFTWARE, {}, false},
      render_pass.get(), &data);

  EXPECT_TRUE(raw_layer->append_quads_for_resourceless_software_draw_called());
}

TEST_F(
    TileBasedLayerImplTest,
    AppendQuadsDoesNotInvokeAppendQuadsForResourcelessSoftwareDrawWhenNotExpected) {
  constexpr gfx::Size kLayerBounds(100, 200);
  constexpr gfx::Rect kLayerRect(kLayerBounds);

  auto layer = std::make_unique<ResourcelessSoftwareDrawTileBasedLayerImpl>(
      host_impl()->active_tree(), /*id=*/1);
  auto* raw_layer = layer.get();
  host_impl()->active_tree()->AddLayer(std::move(layer));

  raw_layer->SetBounds(kLayerBounds);
  raw_layer->draw_properties().visible_layer_rect = kLayerRect;

  SetupRootProperties(host_impl()->active_tree()->root_layer());

  auto render_pass = viz::CompositorRenderPass::Create();
  AppendQuadsData data;
  raw_layer->AppendQuads(AppendQuadsContext{DRAW_MODE_SOFTWARE, {}, false},
                         render_pass.get(), &data);

  EXPECT_FALSE(raw_layer->append_quads_for_resourceless_software_draw_called());
}

class QuadOffsetTestTileBasedLayerImpl : public TestTileBasedLayerImpl {
 public:
  QuadOffsetTestTileBasedLayerImpl(LayerTreeImpl* tree_impl, int id)
      : TestTileBasedLayerImpl(tree_impl, id) {}

  const gfx::Vector2d& quad_offset() const { return quad_offset_; }

 private:
  void AppendQuadsSpecialization(const AppendQuadsContext& context,
                                 viz::CompositorRenderPass* render_pass,
                                 AppendQuadsData* append_quads_data,
                                 viz::SharedQuadState* shared_quad_state,
                                 const Occlusion& scaled_occlusion,
                                 const gfx::Vector2d& quad_offset) override {
    quad_offset_ = quad_offset;
    // Create a dummy quad to avoid tripping debug checks.
    auto* quad =
        render_pass->CreateAndAppendDrawQuad<viz::SolidColorDrawQuad>();
    quad->SetNew(shared_quad_state, gfx::Rect(1, 1), gfx::Rect(1, 1),
                 SkColors::kTransparent, false);
  }
  float GetIdealContentsScaleKey() const override { return 1.f; }

  gfx::Vector2d quad_offset_;
};

TEST_F(TileBasedLayerImplTest, AppendQuadsComputesQuadOffset) {
  const gfx::Size layer_bounds(100, 100);
  const gfx::Rect visible_layer_rect(10, 20, 50, 60);

  auto layer = std::make_unique<QuadOffsetTestTileBasedLayerImpl>(
      host_impl()->active_tree(), /*id=*/1);
  auto* raw_layer = layer.get();
  host_impl()->active_tree()->AddLayer(std::move(layer));

  raw_layer->SetBounds(layer_bounds);
  raw_layer->draw_properties().visible_layer_rect = visible_layer_rect;

  SetupRootProperties(host_impl()->active_tree()->root_layer());

  auto render_pass = viz::CompositorRenderPass::Create();
  AppendQuadsData data;
  raw_layer->AppendQuads(AppendQuadsContext{DRAW_MODE_SOFTWARE, {}, false},
                         render_pass.get(), &data);

  // The quad_offset should be the additive inverse of the visible_layer_rect's
  // origin.
  const gfx::Vector2d expected_quad_offset(-visible_layer_rect.x(),
                                           -visible_layer_rect.y());
  EXPECT_EQ(raw_layer->quad_offset(), expected_quad_offset);

  ASSERT_EQ(render_pass->shared_quad_state_list.size(), 1u);
  const viz::SharedQuadState* shared_quad_state =
      render_pass->shared_quad_state_list.front();

  // The quad_to_target_transform should be translated by the additive inverse
  // of the quad_offset, as the quads themselves are shifted by quad_offset.
  gfx::Transform expected_transform =
      raw_layer->draw_properties().target_space_transform;
  expected_transform.Translate(-expected_quad_offset);
  EXPECT_EQ(shared_quad_state->quad_to_target_transform, expected_transform);

  // The quad_layer_rect should be offset by the quad_offset.
  gfx::Rect expected_quad_layer_rect = gfx::Rect(layer_bounds);
  expected_quad_layer_rect.Offset(expected_quad_offset);
  EXPECT_EQ(shared_quad_state->quad_layer_rect, expected_quad_layer_rect);

  // The visible_quad_layer_rect should be offset by the quad_offset.
  gfx::Rect expected_visible_quad_layer_rect = visible_layer_rect;
  expected_visible_quad_layer_rect.Offset(expected_quad_offset);
  EXPECT_EQ(shared_quad_state->visible_quad_layer_rect,
            expected_visible_quad_layer_rect);
}

class QuadOffsetOrderTestTileBasedLayerImpl : public TestTileBasedLayerImpl {
 public:
  QuadOffsetOrderTestTileBasedLayerImpl(LayerTreeImpl* tree_impl, int id)
      : TestTileBasedLayerImpl(tree_impl, id) {}

  const viz::SharedQuadState* shared_quad_state_at_specialization() const {
    return shared_quad_state_at_specialization_.get();
  }

 private:
  void AppendQuadsSpecialization(const AppendQuadsContext& context,
                                 viz::CompositorRenderPass* render_pass,
                                 AppendQuadsData* append_quads_data,
                                 viz::SharedQuadState* shared_quad_state,
                                 const Occlusion& scaled_occlusion,
                                 const gfx::Vector2d& quad_offset) override {
    shared_quad_state_at_specialization_ =
        std::make_unique<viz::SharedQuadState>(*shared_quad_state);
    // Create a dummy quad to avoid tripping debug checks.
    auto* quad =
        render_pass->CreateAndAppendDrawQuad<viz::SolidColorDrawQuad>();
    quad->SetNew(shared_quad_state, gfx::Rect(1, 1), gfx::Rect(1, 1),
                 SkColors::kTransparent, false);
  }
  float GetIdealContentsScaleKey() const override { return 1.f; }

  std::unique_ptr<viz::SharedQuadState> shared_quad_state_at_specialization_;
};

// Verifies that AppendQuads() updates the shared quad state for the computed
// quad offset only *after* invoking AppendQuadsSpecialization(). This is part
// of the method's contract and is necessary AppendQuadsSpecialization()
// implementations need to operate on the original values of the shared quad
// state (e.g., to find which tiles to draw).
TEST_F(
    TileBasedLayerImplTest,
    AppendQuadsUpdatesSharedQuadStateWithOffsetOnlyAfterCallingSpecialization) {
  const gfx::Size layer_bounds(100, 100);
  const gfx::Rect visible_layer_rect(10, 20, 50, 60);

  auto layer = std::make_unique<QuadOffsetOrderTestTileBasedLayerImpl>(
      host_impl()->active_tree(), /*id=*/1);
  auto* raw_layer = layer.get();
  host_impl()->active_tree()->AddLayer(std::move(layer));

  raw_layer->SetBounds(layer_bounds);
  raw_layer->draw_properties().visible_layer_rect = visible_layer_rect;

  SetupRootProperties(host_impl()->active_tree()->root_layer());

  auto render_pass = viz::CompositorRenderPass::Create();
  AppendQuadsData data;
  raw_layer->AppendQuads(AppendQuadsContext{DRAW_MODE_SOFTWARE, {}, false},
                         render_pass.get(), &data);

  const viz::SharedQuadState* sqs_at_specialization =
      raw_layer->shared_quad_state_at_specialization();
  ASSERT_TRUE(sqs_at_specialization);

  // The SharedQuadState should not have been adjusted by the quad offset at the
  // time of being passed into AppendQuadsSpecialization().
  EXPECT_EQ(sqs_at_specialization->quad_to_target_transform,
            raw_layer->draw_properties().target_space_transform);
  EXPECT_EQ(sqs_at_specialization->quad_layer_rect, gfx::Rect(layer_bounds));
  EXPECT_EQ(sqs_at_specialization->visible_quad_layer_rect, visible_layer_rect);

  // Now check the final SQS to ensure the offset was applied later.
  ASSERT_EQ(render_pass->shared_quad_state_list.size(), 1u);
  const viz::SharedQuadState* final_sqs =
      render_pass->shared_quad_state_list.front();

  const gfx::Vector2d expected_quad_offset(-visible_layer_rect.x(),
                                           -visible_layer_rect.y());

  gfx::Transform expected_transform =
      raw_layer->draw_properties().target_space_transform;
  expected_transform.Translate(-expected_quad_offset);
  EXPECT_EQ(final_sqs->quad_to_target_transform, expected_transform);

  gfx::Rect expected_quad_layer_rect = gfx::Rect(layer_bounds);
  expected_quad_layer_rect.Offset(expected_quad_offset);
  EXPECT_EQ(final_sqs->quad_layer_rect, expected_quad_layer_rect);

  gfx::Rect expected_visible_quad_layer_rect = visible_layer_rect;
  expected_visible_quad_layer_rect.Offset(expected_quad_offset);
  EXPECT_EQ(final_sqs->visible_quad_layer_rect,
            expected_visible_quad_layer_rect);
}

}  // namespace
}  // namespace cc
