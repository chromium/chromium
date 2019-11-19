// Copyright 2012 The Chromium Authors. All rights reserved.
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

  if (debug_state.show_touch_event_handler_rects)
    SaveTouchEventHandlerRects(tree_impl);

  if (debug_state.show_wheel_event_handler_rects)
    SaveWheelEventHandlerRects(tree_impl);

  if (debug_state.show_scroll_event_handler_rects)
    SaveScrollEventHandlerRects(tree_impl);

  if (debug_state.show_non_fast_scrollable_rects)
    SaveNonFastScrollableRects(tree_impl);

  if (debug_state.show_main_thread_scrolling_reason_rects)
    SaveMainThreadScrollingReasonRects(tree_impl);

  if (debug_state.show_layout_shift_regions)
    SaveLayoutShiftRects(hud_layer);

  if (debug_state.show_paint_rects)
    SavePaintRects(tree_impl);

  if (debug_state.show_property_changed_rects)
    SavePropertyChangedRects(tree_impl, hud_layer);

  if (debug_state.show_surface_damage_rects)
    SaveSurfaceDamageRects(render_surface_list);

  if (debug_state.show_screen_space_rects)
    SaveScreenSpaceRects(render_surface_list);
}

void DebugRectHistory::SaveLayoutShiftRects(HeadsUpDisplayLayerImpl* hud) {
  for (gfx::Rect rect : hud->LayoutShiftRects()) {
    debug_rects_.push_back(DebugRect(
        LAYOUT_SHIFT_RECT_TYPE,
        MathUtil::MapEnclosingClippedRect(hud->ScreenSpaceTransform(), rect)));
  }
}

void DebugRectHistory::SavePaintRects(LayerTreeImpl* tree_impl) {
  // We would like to visualize where any layer's paint rect (update rect) has
  // changed, regardless of whether this layer is skipped for actual drawing or
  // not. Therefore we traverse over all layers, not just the render surface
  // list.
  for (auto* layer : *tree_impl) {
    Region invalidation_region = layer->GetInvalidationRegionForDebugging();
    if (invalidation_region.IsEmpty() || !layer->DrawsContent())
      continue;

    for (gfx::Rect rect : invalidation_region) {
      debug_rects_.push_back(
          DebugRect(PAINT_RECT_TYPE, MathUtil::MapEnclosingClippedRect(
                                         layer->ScreenSpaceTransform(), rect)));
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

    debug_rects_.push_back(DebugRect(
        PROPERTY_CHANGED_RECT_TYPE,
        MathUtil::MapEnclosingClippedRect(layer->ScreenSpaceTransform(),
                                          gfx::Rect(layer->bounds()))));
  }
}

void DebugRectHistory::SaveSurfaceDamageRects(
    const RenderSurfaceList& render_surface_list) {
  for (size_t i = 0; i < render_surface_list.size(); ++i) {
    size_t surface_index = render_surface_list.size() - 1 - i;
    RenderSurfaceImpl* render_surface = render_surface_list[surface_index];
    DCHECK(render_surface);

    debug_rects_.push_back(DebugRect(
        SURFACE_DAMAGE_RECT_TYPE, MathUtil::MapEnclosingClippedRect(
                                      render_surface->screen_space_transform(),
                                      render_surface->GetDamageRect())));
  }
}

void DebugRectHistory::SaveScreenSpaceRects(
    const RenderSurfaceList& render_surface_list) {
  for (size_t i = 0; i < render_surface_list.size(); ++i) {
    size_t surface_index = render_surface_list.size() - 1 - i;
    RenderSurfaceImpl* render_surface = render_surface_list[surface_index];
    DCHECK(render_surface);

    debug_rects_.push_back(DebugRect(
        SCREEN_SPACE_RECT_TYPE, MathUtil::MapEnclosingClippedRect(
                                    render_surface->screen_space_transform(),
                                    render_surface->content_rect())));
  }
}

void DebugRectHistory::SaveTouchEventHandlerRects(LayerTreeImpl* tree_impl) {
  for (auto* layer : *tree_impl)
    SaveTouchEventHandlerRectsCallback(layer);
}

void DebugRectHistory::SaveTouchEventHandlerRectsCallback(LayerImpl* layer) {
  const TouchActionRegion& touch_action_region = layer->touch_action_region();
  for (int touch_action_index = kTouchActionNone;
       touch_action_index != kTouchActionMax; ++touch_action_index) {
    auto touch_action = static_cast<TouchAction>(touch_action_index);
    Region region = touch_action_region.GetRegionForTouchAction(touch_action);
    for (gfx::Rect rect : region) {
      debug_rects_.emplace_back(TOUCH_EVENT_HANDLER_RECT_TYPE,
                                MathUtil::MapEnclosingClippedRect(
                                    layer->ScreenSpaceTransform(), rect),
                                touch_action);
    }
  }
}

void DebugRectHistory::SaveWheelEventHandlerRects(LayerTreeImpl* tree_impl) {
  EventListenerProperties event_properties =
      tree_impl->event_listener_properties(EventListenerClass::kMouseWheel);
  if (event_properties == EventListenerProperties::kNone ||
      event_properties == EventListenerProperties::kPassive) {
    return;
  }

  // Since the wheel event handlers property is on the entire layer tree just
  // mark inner viewport if have listeners.
  ScrollNode* inner_scroll = tree_impl->InnerViewportScrollNode();
  if (!inner_scroll)
    return;
  debug_rects_.push_back(
      DebugRect(WHEEL_EVENT_HANDLER_RECT_TYPE,
                MathUtil::MapEnclosingClippedRect(
                    tree_impl->property_trees()->transform_tree.ToScreen(
                        inner_scroll->transform_id),
                    gfx::Rect(inner_scroll->bounds))));
}

void DebugRectHistory::SaveScrollEventHandlerRects(LayerTreeImpl* tree_impl) {
  for (auto* layer : *tree_impl)
    SaveScrollEventHandlerRectsCallback(layer);
}

void DebugRectHistory::SaveScrollEventHandlerRectsCallback(LayerImpl* layer) {
  if (!layer->layer_tree_impl()->have_scroll_event_handlers())
    return;

  debug_rects_.push_back(
      DebugRect(SCROLL_EVENT_HANDLER_RECT_TYPE,
                MathUtil::MapEnclosingClippedRect(layer->ScreenSpaceTransform(),
                                                  gfx::Rect(layer->bounds()))));
}

void DebugRectHistory::SaveNonFastScrollableRects(LayerTreeImpl* tree_impl) {
  for (auto* layer : *tree_impl)
    SaveNonFastScrollableRectsCallback(layer);
}

void DebugRectHistory::SaveNonFastScrollableRectsCallback(LayerImpl* layer) {
  for (gfx::Rect rect : layer->non_fast_scrollable_region()) {
    debug_rects_.push_back(DebugRect(NON_FAST_SCROLLABLE_RECT_TYPE,
                                     MathUtil::MapEnclosingClippedRect(
                                         layer->ScreenSpaceTransform(), rect)));
  }
}

void DebugRectHistory::SaveMainThreadScrollingReasonRects(
    LayerTreeImpl* tree_impl) {
  const auto& scroll_tree = tree_impl->property_trees()->scroll_tree;
  for (auto* layer : *tree_impl) {
    if (layer->scrollable()) {
      if (const auto* scroll_node =
              scroll_tree.Node(layer->scroll_tree_index())) {
        if (auto reasons = scroll_node->main_thread_scrolling_reasons) {
          debug_rects_.push_back(DebugRect(
              MAIN_THREAD_SCROLLING_REASON_RECT_TYPE,
              MathUtil::MapEnclosingClippedRect(layer->ScreenSpaceTransform(),
                                                gfx::Rect(layer->bounds())),
              kTouchActionNone, reasons));
        }
      }
    }
  }
}

}  // namespace cc
