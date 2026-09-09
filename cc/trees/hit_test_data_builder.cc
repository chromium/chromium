// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/trees/hit_test_data_builder.h"

#include <cstddef>
#include <cstdint>
#include <optional>
#include <utility>

#include "base/check_op.h"
#include "base/containers/adapters.h"
#include "base/memory/raw_ref.h"
#include "cc/base/math_util.h"
#include "cc/base/region.h"
#include "cc/layers/layer_impl.h"
#include "cc/layers/surface_layer_impl.h"
#include "cc/trees/layer_tree_impl.h"
#include "cc/trees/property_tree.h"
#include "components/viz/common/hit_test/hit_test_region_list.h"
#include "components/viz/common/surfaces/frame_sink_id.h"
#include "components/viz/common/surfaces/surface_id.h"
#include "third_party/skia/include/core/SkScalar.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/gfx/geometry/rrect_f.h"
#include "ui/gfx/geometry/transform.h"

namespace cc {
namespace {

// In HitTestDataBuilder::Build we iterate all layers to find all layers that
// overlap OOPIFs, but when the number of layers is greater than
// |kAssumeOverlapThreshold|, it can be inefficient to accumulate layer bounds
// for overlap checking. As a result, we are conservative and make OOPIFs
// kHitTestAsk after the threshold is reached.
constexpr size_t kAssumeOverlapThreshold = 100;

uint32_t GetFlagsForSurfaceLayer(const SurfaceLayerImpl* layer) {
  uint32_t flags = viz::HitTestRegionFlags::kHitTestMouse |
                   viz::HitTestRegionFlags::kHitTestTouch;
  if (layer->range().IsValid()) {
    flags |= viz::HitTestRegionFlags::kHitTestChildSurface;
  } else {
    flags |= viz::HitTestRegionFlags::kHitTestMine;
  }
  return flags;
}

void PopulateHitTestRegion(viz::HitTestRegion* hit_test_region,
                           const LayerImpl* layer,
                           uint32_t flags,
                           uint32_t async_hit_test_reasons,
                           const gfx::RRectF& rect,
                           const viz::SurfaceId& surface_id,
                           float device_scale_factor) {
  hit_test_region->frame_sink_id = surface_id.frame_sink_id();
  hit_test_region->flags = flags;
  hit_test_region->async_hit_test_reasons = async_hit_test_reasons;
  DCHECK_EQ(!!async_hit_test_reasons,
            !!(flags & viz::HitTestRegionFlags::kHitTestAsk));

  hit_test_region->rect = rect;
  // The transform of hit test region maps a point from parent hit test region
  // to the local space. This is the inverse of screen space transform. Because
  // hit test query wants the point in target to be in Pixel space, we
  // counterscale the transform here. Note that the rect is scaled by dsf, so
  // the point and the rect are still in the same space.
  gfx::Transform surface_to_root_transform = layer->ScreenSpaceTransform();
  surface_to_root_transform.Scale(SK_Scalar1 / device_scale_factor,
                                  SK_Scalar1 / device_scale_factor);
  surface_to_root_transform.Flatten();
  // TODO(sunxd): Avoid losing precision by not using inverse if possible.
  // Note: |transform| is set to the identity if |surface_to_root_transform| is
  // not invertible, which is what we want.
  hit_test_region->transform = surface_to_root_transform.InverseOrIdentity();
}

}  // namespace

HitTestDataBuilder::HitTestDataBuilder(const LayerTreeImpl& active_tree)
    : active_tree_(active_tree) {}

std::optional<viz::HitTestRegionList> HitTestDataBuilder::Build() && {
  std::optional<viz::HitTestRegionList> hit_test_region_list(std::in_place);
  hit_test_region_list->flags = viz::HitTestRegionFlags::kHitTestMine |
                                viz::HitTestRegionFlags::kHitTestMouse |
                                viz::HitTestRegionFlags::kHitTestTouch;
  hit_test_region_list->bounds = active_tree_->GetDeviceViewport();
  hit_test_region_list->transform = active_tree_->DrawTransform();

  float device_scale_factor = active_tree_->device_scale_factor();

  Region overlapping_region;
  size_t num_iterated_layers = 0;
  // If the layer tree contains more than 100 layers, we stop accumulating
  // layers in |overlapping_region| to save compositor frame submitting time,
  // as a result we do async hit test on any surface layers that may be
  // overlapped by other layers.
  bool assume_overlap = false;
  for (const auto* layer : base::Reversed(*active_tree_)) {
    if (layer->is_surface_layer()) {
      const auto* surface_layer = static_cast<const SurfaceLayerImpl*>(layer);
      // We should not skip a non-hit-testable surface layer if
      // - it has pointer-events: none because viz hit test needs to know the
      //   information to ensure all descendant OOPIFs to ignore hit tests; or
      // - it draws content to track overlaps.
      if (!layer->HitTestable() && !layer->draws_content() &&
          !surface_layer->has_pointer_events_none()) {
        continue;
      }
      // If a surface layer is created not by child frame compositor or the
      // frame owner has pointer-events: none property, the surface layer
      // becomes not hit testable. We should not generate data for it.
      if (!surface_layer->surface_hit_testable() ||
          !surface_layer->range().IsValid()) {
        // We collect any overlapped regions that does not have
        // pointer-events: none.
        if (!surface_layer->has_pointer_events_none() && !assume_overlap) {
          overlapping_region.Union(MathUtil::MapEnclosingClippedRect(
              layer->ScreenSpaceTransform(),
              gfx::Rect(surface_layer->bounds())));
        }
        continue;
      }

      // Using the enclosing rect to ensure antialiased boundary pixels cause
      // pointer input to be routed to this layer.
      gfx::Rect content_rect(gfx::ScaleToEnclosingRect(
          gfx::Rect(surface_layer->bounds()), device_scale_factor));

      gfx::Rect layer_screen_space_rect = MathUtil::MapEnclosingClippedRect(
          surface_layer->ScreenSpaceTransform(),
          gfx::Rect(surface_layer->bounds()));
      auto flag = GetFlagsForSurfaceLayer(surface_layer);
      uint32_t async_hit_test_reasons =
          viz::AsyncHitTestReasons::kNotAsyncHitTest;
      if (surface_layer->has_pointer_events_none()) {
        flag |= viz::HitTestRegionFlags::kHitTestIgnore;
      }
      if (assume_overlap ||
          overlapping_region.Intersects(layer_screen_space_rect)) {
        flag |= viz::HitTestRegionFlags::kHitTestAsk;
        async_hit_test_reasons |= viz::AsyncHitTestReasons::kOverlappedRegion;
      }
      bool layer_hit_test_region_is_masked =
          active_tree_->property_trees()
              ->effect_tree()
              .HitTestMayBeAffectedByMask(surface_layer->effect_tree_index());
      if (surface_layer->is_clipped() || layer_hit_test_region_is_masked) {
        bool layer_hit_test_region_is_rectangle =
            !layer_hit_test_region_is_masked &&
            surface_layer->ScreenSpaceTransform().Preserves2dAxisAlignment() &&
            active_tree_->property_trees()
                ->effect_tree()
                .ClippedHitTestRegionIsRectangle(
                    surface_layer->effect_tree_index());
        content_rect =
            gfx::ScaleToEnclosingRect(surface_layer->visible_layer_rect(),
                                      device_scale_factor, device_scale_factor);
        if (!layer_hit_test_region_is_rectangle) {
          flag |= viz::HitTestRegionFlags::kHitTestAsk;
          async_hit_test_reasons |= viz::AsyncHitTestReasons::kIrregularClip;
        }
      }
      const auto& surface_id = surface_layer->range().end();
      hit_test_region_list->regions.emplace_back();
      PopulateHitTestRegion(&hit_test_region_list->regions.back(), layer, flag,
                            async_hit_test_reasons, gfx::RRectF(content_rect),
                            surface_id, device_scale_factor);
      continue;
    }

    if (!layer->HitTestable()) {
      continue;
    }

    // TODO(sunxd): Submit all overlapping layer bounds as hit test regions.
    // Also investigate if we can use visible layer rect as overlapping
    // regions.
    num_iterated_layers++;
    if (num_iterated_layers > kAssumeOverlapThreshold) {
      assume_overlap = true;
    }
    if (!assume_overlap) {
      overlapping_region.Union(MathUtil::MapEnclosingClippedRect(
          layer->ScreenSpaceTransform(), gfx::Rect(layer->bounds())));
    }
  }

  return hit_test_region_list;
}

}  // namespace cc
