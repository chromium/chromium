// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/test/layer_test_common.h"

#include <memory>
#include <utility>
#include <vector>

#include "cc/base/math_util.h"
#include "cc/base/region.h"
#include "cc/test/property_tree_test_utils.h"
#include "cc/trees/draw_property_utils.h"
#include "cc/trees/layer_tree_impl.h"
#include "cc/trees/property_tree_builder.h"
#include "components/viz/common/quads/compositor_render_pass.h"
#include "components/viz/common/quads/draw_quad.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/gfx/geometry/rect_conversions.h"

namespace cc {

LayerTreeSettings CommitToActiveTreeLayerListSettings() {
  LayerTreeSettings settings;
  settings.commit_to_active_tree = true;
  settings.use_layer_lists = true;
  return settings;
}

LayerTreeSettings CommitToActiveTreeLayerTreeSettings() {
  LayerTreeSettings settings;
  settings.commit_to_active_tree = true;
  settings.use_layer_lists = false;
  return settings;
}

LayerTreeSettings CommitToPendingTreeLayerListSettings() {
  LayerTreeSettings settings;
  settings.commit_to_active_tree = false;
  settings.use_layer_lists = true;
  return settings;
}

LayerTreeSettings CommitToPendingTreeLayerTreeSettings() {
  LayerTreeSettings settings;
  settings.commit_to_active_tree = false;
  settings.use_layer_lists = false;
  return settings;
}

// Align with expected and actual output.
static const char* kQuadString = "    Quad: ";

static bool CanRectFBeSafelyRoundedToRect(const gfx::RectF& r) {
  // Ensure that range of float values is not beyond integer range.
  if (!r.IsExpressibleAsRect())
    return false;

  // Ensure that the values are actually integers.
  gfx::RectF floored_rect(std::floor(r.x()), std::floor(r.y()),
                          std::floor(r.width()), std::floor(r.height()));
  return floored_rect == r;
}

void VerifyQuadsExactlyCoverRect(const viz::QuadList& quads,
                                 const gfx::Rect& rect) {
  Region remaining = rect;

  for (auto iter = quads.cbegin(); iter != quads.cend(); ++iter) {
    EXPECT_TRUE(iter->rect.Contains(iter->visible_rect));

    gfx::RectF quad_rectf = MathUtil::MapClippedRect(
        iter->shared_quad_state->quad_to_target_transform,
        gfx::RectF(iter->visible_rect));

    // Before testing for exact coverage in the integer world, assert that
    // rounding will not round the rect incorrectly.
    ASSERT_TRUE(CanRectFBeSafelyRoundedToRect(quad_rectf));

    gfx::Rect quad_rect = gfx::ToEnclosingRect(quad_rectf);

    EXPECT_TRUE(rect.Contains(quad_rect))
        << kQuadString << iter.index() << " rect: " << rect.ToString()
        << " quad: " << quad_rect.ToString();
    EXPECT_TRUE(remaining.Contains(quad_rect))
        << kQuadString << iter.index() << " remaining: " << remaining.ToString()
        << " quad: " << quad_rect.ToString();
    remaining.Subtract(quad_rect);
  }

  EXPECT_TRUE(remaining.IsEmpty());
}

// static
void VerifyQuadsAreOccluded(const viz::QuadList& quads,
                            const gfx::Rect& occluded,
                            size_t* partially_occluded_count) {
  // No quad should exist if it's fully occluded.
  for (auto* quad : quads) {
    gfx::Rect target_visible_rect = MathUtil::MapEnclosingClippedRect(
        quad->shared_quad_state->quad_to_target_transform, quad->visible_rect);
    EXPECT_FALSE(occluded.Contains(target_visible_rect));
  }

  // Quads that are fully occluded on one axis only should be shrunken.
  for (auto* quad : quads) {
    gfx::Rect target_rect = MathUtil::MapEnclosingClippedRect(
        quad->shared_quad_state->quad_to_target_transform, quad->rect);
    if (!quad->shared_quad_state->quad_to_target_transform
             .IsIdentityOrIntegerTranslation()) {
      DCHECK(quad->shared_quad_state->quad_to_target_transform
                 .IsPositiveScaleOrTranslation())
          << quad->shared_quad_state->quad_to_target_transform.ToString();
      gfx::RectF target_rectf = MathUtil::MapClippedRect(
          quad->shared_quad_state->quad_to_target_transform,
          gfx::RectF(quad->rect));
      // Scale transforms allowed, as long as the final transformed rect
      // ends up on integer boundaries for ease of testing.
      ASSERT_EQ(target_rectf, gfx::RectF(target_rect));
    }

    bool fully_occluded_horizontal = target_rect.x() >= occluded.x() &&
                                     target_rect.right() <= occluded.right();
    bool fully_occluded_vertical = target_rect.y() >= occluded.y() &&
                                   target_rect.bottom() <= occluded.bottom();
    bool should_be_occluded =
        target_rect.Intersects(occluded) &&
        (fully_occluded_vertical || fully_occluded_horizontal);
    if (!should_be_occluded) {
      EXPECT_EQ(quad->rect.ToString(), quad->visible_rect.ToString());
    } else {
      EXPECT_NE(quad->rect.ToString(), quad->visible_rect.ToString());
      EXPECT_TRUE(quad->rect.Contains(quad->visible_rect));
      ++(*partially_occluded_count);
    }
  }
}

void PrepareForUpdateDrawProperties(LayerTreeImpl* layer_tree_impl) {
  if (!layer_tree_impl->settings().use_layer_lists)
    return;

  // TODO(wangxianzhu): We should DCHECK(!needs_rebuild) after we remove all
  // unnecessary setting of the flag in layer list mode.
  auto* property_trees = layer_tree_impl->property_trees();
  property_trees->set_needs_rebuild(false);

  // The following are needed for tests that modify impl-side property trees.
  // In production code impl-side property trees are pushed from the main
  // thread and the following are done in other ways.
  std::vector<std::unique_ptr<RenderSurfaceImpl>> old_render_surfaces;
  property_trees->effect_tree_mutable().TakeRenderSurfaces(
      &old_render_surfaces);
  property_trees->effect_tree_mutable().CreateOrReuseRenderSurfaces(
      &old_render_surfaces, layer_tree_impl);
  layer_tree_impl->MoveChangeTrackingToLayers();
  property_trees->ResetCachedData();
}

void UpdateDrawProperties(LayerTreeImpl* layer_tree_impl,
                          LayerImplList* output_update_layer_list) {
  PrepareForUpdateDrawProperties(layer_tree_impl);
  layer_tree_impl->UpdateDrawProperties(
      /*update_tiles=*/true, /*update_image_animation_controller=*/true,
      output_update_layer_list);
}

void UpdateDrawProperties(LayerTreeHost* layer_tree_host,
                          LayerList* output_update_layer_list) {
  LayerList update_layer_list;
  if (layer_tree_host->IsUsingLayerLists()) {
    // TODO(wangxianzhu): We should DCHECK(!needs_rebuild) after we remove all
    // unnecessary setting of the flag in layer list mode.
    layer_tree_host->property_trees()->set_needs_rebuild(false);
  } else {
    PropertyTreeBuilder::BuildPropertyTrees(layer_tree_host);
  }

  draw_property_utils::UpdatePropertyTrees(layer_tree_host);
  draw_property_utils::FindLayersThatNeedUpdates(layer_tree_host,
                                                 &update_layer_list);

  if (output_update_layer_list)
    *output_update_layer_list = std::move(update_layer_list);
}

void SetDeviceScaleAndUpdateViewportRect(LayerTreeImpl* layer_tree_impl,
                                         float device_scale_factor) {
  layer_tree_impl->SetDeviceScaleFactor(device_scale_factor);
  gfx::Size root_bounds = layer_tree_impl->root_layer()->bounds();
  layer_tree_impl->SetDeviceViewportRect(
      gfx::Rect(root_bounds.width() * device_scale_factor,
                root_bounds.height() * device_scale_factor));
}

void SetDeviceScaleAndUpdateViewportRect(LayerTreeHost* layer_tree_host,
                                         float device_scale_factor) {
  gfx::Size root_bounds = layer_tree_host->root_layer()->bounds();
  gfx::Rect viewport_rect(root_bounds.width() * device_scale_factor,
                          root_bounds.height() * device_scale_factor);
  layer_tree_host->SetViewportRectAndScale(viewport_rect, device_scale_factor,
                                           viz::LocalSurfaceId());
}

}  // namespace cc
