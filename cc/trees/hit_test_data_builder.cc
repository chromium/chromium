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

// In `HitTestDataBuilder::Build` we iterate all layers to find all layers that
// overlap OOPIFs, but when the number of layers is greater than
// |kAssumeOverlapThreshold|, it can be inefficient to accumulate layer bounds
// for overlap checking. As a result, we are conservative and make OOPIFs
// kHitTestAsk after the threshold is reached.
constexpr size_t kAssumeOverlapThreshold = 100;

gfx::Rect MapLayerBoundsToScreen(const LayerImpl* layer) {
  return MathUtil::MapEnclosingClippedRect(layer->ScreenSpaceTransform(),
                                           gfx::Rect(layer->bounds()));
}

uint32_t GetFlagsForSurfaceLayer(const SurfaceLayerImpl* layer) {
  uint32_t flags = viz::HitTestRegionFlags::kHitTestMouse |
                   viz::HitTestRegionFlags::kHitTestTouch;
  if (layer->range().IsValid()) {
    flags |= viz::HitTestRegionFlags::kHitTestChildSurface;
  } else {
    flags |= viz::HitTestRegionFlags::kHitTestMine;
  }
  if (layer->has_pointer_events_none()) {
    flags |= viz::HitTestRegionFlags::kHitTestIgnore;
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

  const float device_scale_factor = active_tree_->device_scale_factor();
  const EffectTree& effect_tree = active_tree_->property_trees()->effect_tree();

  for (const auto* layer : base::Reversed(*active_tree_)) {
    const SurfaceLayerImpl* surface_layer = EvaluateLayerAndTrackOverlap(layer);
    if (!surface_layer) {
      continue;
    }

    // Using the enclosing rect to ensure antialiased boundary pixels cause
    // pointer input to be routed to this layer.
    gfx::Rect hit_test_rect(gfx::ScaleToEnclosingRect(
        gfx::Rect(surface_layer->bounds()), device_scale_factor));

    uint32_t flags = GetFlagsForSurfaceLayer(surface_layer);
    uint32_t async_hit_test_reasons =
        viz::AsyncHitTestReasons::kNotAsyncHitTest;
    if (IsSurfaceOverlapped(surface_layer)) {
      flags |= viz::HitTestRegionFlags::kHitTestAsk;
      async_hit_test_reasons |= viz::AsyncHitTestReasons::kOverlappedRegion;
    }
    bool layer_hit_test_region_is_masked =
        effect_tree.HitTestMayBeAffectedByMask(
            surface_layer->effect_tree_index());
    if (surface_layer->is_clipped() || layer_hit_test_region_is_masked) {
      bool layer_hit_test_region_is_rectangle =
          !layer_hit_test_region_is_masked &&
          surface_layer->ScreenSpaceTransform().Preserves2dAxisAlignment() &&
          effect_tree.ClippedHitTestRegionIsRectangle(
              surface_layer->effect_tree_index());
      hit_test_rect =
          gfx::ScaleToEnclosingRect(surface_layer->visible_layer_rect(),
                                    device_scale_factor, device_scale_factor);
      if (!layer_hit_test_region_is_rectangle) {
        flags |= viz::HitTestRegionFlags::kHitTestAsk;
        async_hit_test_reasons |= viz::AsyncHitTestReasons::kIrregularClip;
      }
    }
    const auto& surface_id = surface_layer->range().end();
    hit_test_region_list->regions.emplace_back();
    PopulateHitTestRegion(&hit_test_region_list->regions.back(), layer, flags,
                          async_hit_test_reasons, gfx::RRectF(hit_test_rect),
                          surface_id, device_scale_factor);
  }

  return hit_test_region_list;
}

const SurfaceLayerImpl* HitTestDataBuilder::EvaluateLayerAndTrackOverlap(
    const LayerImpl* layer) {
  if (!layer->is_surface_layer()) {
    if (layer->HitTestable()) {
      TrackHitTestableNonSurfaceLayer(layer);
    }
    return nullptr;
  }

  const auto* surface_layer = static_cast<const SurfaceLayerImpl*>(layer);
  // We should not skip a non-hit-testable surface layer if
  // - it has pointer-events: none because viz hit test needs to know the
  //   information to ensure all descendant OOPIFs to ignore hit tests; or
  // - it draws content to track overlaps.
  if (!layer->HitTestable() && !layer->draws_content() &&
      !surface_layer->has_pointer_events_none()) {
    return nullptr;
  }
  // If a surface layer is created not by child frame compositor or the frame
  // owner has pointer-events: none property, the surface layer becomes not
  // hit testable. We should not generate data for it.
  if (!surface_layer->surface_hit_testable() ||
      !surface_layer->range().IsValid()) {
    // Track overlapping regions that do not have pointer-events: none.
    if (!surface_layer->has_pointer_events_none()) {
      TrackNonEmittedSurface(surface_layer);
    }
    return nullptr;
  }

  return surface_layer;
}

void HitTestDataBuilder::TrackHitTestableNonSurfaceLayer(
    const LayerImpl* layer) {
  ++num_hit_testable_non_surface_layers_;
  if (!ShouldAssumeOverlap()) {
    overlapping_region_.Union(MapLayerBoundsToScreen(layer));
  }
}

void HitTestDataBuilder::TrackNonEmittedSurface(
    const SurfaceLayerImpl* surface_layer) {
  if (!ShouldAssumeOverlap()) {
    overlapping_region_.Union(MapLayerBoundsToScreen(surface_layer));
  }
}

bool HitTestDataBuilder::IsSurfaceOverlapped(
    const SurfaceLayerImpl* surface_layer) const {
  return ShouldAssumeOverlap() ||
         overlapping_region_.Intersects(MapLayerBoundsToScreen(surface_layer));
}

bool HitTestDataBuilder::ShouldAssumeOverlap() const {
  return num_hit_testable_non_surface_layers_ > kAssumeOverlapThreshold;
}

}  // namespace cc
