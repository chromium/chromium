// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/trees/debug_rect_history.h"

#include <stddef.h>

#include "base/memory/ptr_util.h"
#include "cc/base/math_util.h"
#include "cc/layers/heads_up_display_layer_impl.h"
#include "cc/layers/layer_impl.h"
#include "cc/layers/layer_list_iterator.h"
#include "cc/layers/render_surface_impl.h"
#include "cc/paint/display_item_list.h"
#include "cc/trees/damage_tracker.h"
#include "cc/trees/layer_tree_host.h"
#include "cc/trees/layer_tree_impl.h"
#include "cc/trees/scroll_node.h"
#include "ui/gfx/geometry/rect_conversions.h"

namespace cc {

// static
std::unique_ptr<DebugRectHistory> DebugRectHistory::Create() {
  return base::WrapUnique(new DebugRectHistory());
}

DebugRectHistory::DebugRectHistory() = default;

DebugRectHistory::~DebugRectHistory() = default;

void DebugRectHistory::SaveDebugRectsForCurrentFrame(
    LayerTreeImpl* tree_impl,
    HeadsUpDisplayLayerImpl* hud_layer,
    const RenderSurfaceList& render_surface_list,
    const LayerTreeDebugState& debug_state) {
  // For now, clear all rects from previous frames. In the future we may want to
  // store all debug rects for a history of many frames.
  debug_rects_.clear();

  if (debug_state.show_touch_event_handler_rects) {
    SaveTouchEventHandlerRects(tree_impl);
  }
  if (debug_state.show_wheel_event_handler_rects) {
    SaveWheelEventHandlerRects(tree_impl);
  }
  if (debug_state.show_scroll_event_handler_rects) {
    SaveScrollEventHandlerRects(tree_impl);
  }
  if (debug_state.show_main_thread_scroll_hit_test_rects) {
    SaveMainThreadScrollHitTestRects(tree_impl);
  }
  if (debug_state.show_main_thread_scroll_repaint_rects) {
    SaveMainThreadScrollRepaintOrRasterInducingScrollRects(
        tree_impl, DebugRectType::kMainThreadScrollRepaint);
  }
  if (debug_state.show_raster_inducing_scroll_rects) {
    SaveMainThreadScrollRepaintOrRasterInducingScrollRects(
        tree_impl, DebugRectType::kRasterInducingScroll);
  }
  if (debug_state.show_layout_shift_regions) {
    SaveLayoutShiftRects(hud_layer);
  }
  if (debug_state.show_paint_rects) {
    SavePaintRects(tree_impl);
  }
  if (debug_state.show_property_changed_rects) {
    SavePropertyChangedRects(tree_impl, hud_layer);
  }
  if (debug_state.show_surface_damage_rects) {
    SaveSurfaceDamageRects(render_surface_list);
  }
  if (debug_state.show_screen_space_rects) {
    SaveScreenSpaceRects(render_surface_list);
  }
}

void DebugRectHistory::SaveLayoutShiftRects(HeadsUpDisplayLayerImpl* hud) {
  // We store the layout shift rects on the hud layer. If we don't have the hud
  // layer, then there is nothing to store.
  if (!hud)
    return;

  for (gfx::Rect rect : hud->LayoutShiftRects()) {
    debug_rects_.emplace_back(
        DebugRectType::kLayoutShift,
        MathUtil::MapEnclosingClippedRect(hud->ScreenSpaceTransform(), rect));
  }
  hud->ClearLayoutShiftRects();
}

void DebugRectHistory::SavePaintRects(LayerTreeImpl* tree_impl) {
  // We would like to visualize where any layer's paint rect (update rect) has
  // changed, regardless of whether this layer is skipped for actual drawing or
  // not. Therefore we traverse over all layers, not just the render surface
  // list.
  for (auto* layer : *tree_impl) {
    Region invalidation_region = layer->GetInvalidationRegionForDebugging();
    if (invalidation_region.IsEmpty() || !layer->draws_content())
      continue;

    for (gfx::Rect rect : invalidation_region) {
      debug_rects_.emplace_back(DebugRectType::kPaint,
                                MathUtil::MapEnclosingClippedRect(
                                    layer->ScreenSpaceTransform(), rect));
    }
  }
}

void DebugRectHistory::SavePropertyChangedRects(LayerTreeImpl* tree_impl,
                                                LayerImpl* hud_layer) {
  for (LayerImpl* layer : *tree_impl) {
    if (layer == hud_layer)
      continue;

    if (!layer->LayerPropertyChanged())
      continue;

    debug_rects_.emplace_back(
        DebugRectType::kPropertyChanged,
        MathUtil::MapEnclosingClippedRect(layer->ScreenSpaceTransform(),
                                          gfx::Rect(layer->bounds())));
  }
}

void DebugRectHistory::SaveSurfaceDamageRects(
    const RenderSurfaceList& render_surface_list) {
  for (size_t i = 0; i < render_surface_list.size(); ++i) {
    size_t surface_index = render_surface_list.size() - 1 - i;
    RenderSurfaceImpl* render_surface = render_surface_list[surface_index];
    DCHECK(render_surface);

    debug_rects_.emplace_back(DebugRectType::kSurfaceDamage,
                              MathUtil::MapEnclosingClippedRect(
                                  render_surface->screen_space_transform(),
                                  render_surface->GetDamageRect()));
  }
}

void DebugRectHistory::SaveScreenSpaceRects(
    const RenderSurfaceList& render_surface_list) {
  for (size_t i = 0; i < render_surface_list.size(); ++i) {
    size_t surface_index = render_surface_list.size() - 1 - i;
    RenderSurfaceImpl* render_surface = render_surface_list[surface_index];
    DCHECK(render_surface);

    debug_rects_.emplace_back(DebugRectType::kScreenSpace,
                              MathUtil::MapEnclosingClippedRect(
                                  render_surface->screen_space_transform(),
                                  render_surface->content_rect()));
  }
}

void DebugRectHistory::SaveTouchEventHandlerRects(LayerTreeImpl* tree_impl) {
  for (auto* layer : *tree_impl) {
    const TouchActionRegion& touch_action_region = layer->touch_action_region();
    for (int touch_action_index = static_cast<int>(TouchAction::kNone);
         touch_action_index != static_cast<int>(TouchAction::kMax);
         ++touch_action_index) {
      auto touch_action = static_cast<TouchAction>(touch_action_index);
      Region region = touch_action_region.GetRegionForTouchAction(touch_action);
      for (gfx::Rect rect : region) {
        debug_rects_.emplace_back(DebugRectType::kTouchEventHandler,
                                  MathUtil::MapEnclosingClippedRect(
                                      layer->ScreenSpaceTransform(), rect),
                                  touch_action);
      }
    }
  }
}

void DebugRectHistory::SaveWheelEventHandlerRects(LayerTreeImpl* tree_impl) {
  // TODO(crbug.com/40724301): Need behavior confirmation.
  // TODO(crbug.com/40724301): Need to check results in dev tools layer
  // view.
  for (auto* layer : *tree_impl) {
    const Region& region = layer->wheel_event_handler_region();
    for (gfx::Rect rect : region) {
      debug_rects_.emplace_back(DebugRectType::kWheelEventHandler,
                                MathUtil::MapEnclosingClippedRect(
                                    layer->ScreenSpaceTransform(), rect));
    }
  }
}

void DebugRectHistory::SaveScrollEventHandlerRects(LayerTreeImpl* tree_impl) {
  for (auto* layer : *tree_impl) {
    if (!layer->layer_tree_impl()->have_scroll_event_handlers()) {
      continue;
    }
    debug_rects_.emplace_back(
        DebugRectType::kScrollEventHandler,
        MathUtil::MapEnclosingClippedRect(layer->ScreenSpaceTransform(),
                                          gfx::Rect(layer->bounds())));
  }
}

void DebugRectHistory::SaveMainThreadScrollHitTestRects(
    LayerTreeImpl* tree_impl) {
  for (auto* layer : *tree_impl) {
    for (gfx::Rect rect : layer->main_thread_scroll_hit_test_region()) {
      debug_rects_.emplace_back(DebugRectType::kMainThreadScrollHitTest,
                                MathUtil::MapEnclosingClippedRect(
                                    layer->ScreenSpaceTransform(), rect));
    }
  }
}

void DebugRectHistory::SaveMainThreadScrollRepaintOrRasterInducingScrollRects(
    LayerTreeImpl* tree_impl,
    DebugRectType type) {
  const auto& scroll_tree = tree_impl->property_trees()->scroll_tree();
  const auto& transform_tree = tree_impl->property_trees()->transform_tree();
  for (auto& node : scroll_tree.nodes()) {
    if (type == DebugRectType::kMainThreadScrollRepaint
            ? scroll_tree.ShouldRealizeScrollsOnMain(node)
            : scroll_tree.CanRealizeScrollsOnPendingTree(node)) {
      if (const auto* transform_node = transform_tree.Node(node.transform_id);
          transform_node && transform_tree.Node(transform_node->parent_id)) {
        debug_rects_.emplace_back(
            type, MathUtil::MapEnclosingClippedRect(
                      // Skip the scroll translation node.
                      transform_tree.ToScreen(transform_node->parent_id),
                      gfx::Rect(node.container_origin,
                                scroll_tree.container_bounds(node.id))));
      }
    }
  }
}

}  // namespace cc
