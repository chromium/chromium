// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CC_TREES_DEBUG_RECT_HISTORY_H_
#define CC_TREES_DEBUG_RECT_HISTORY_H_

#include <memory>
#include <vector>

#include "cc/input/touch_action.h"
#include "cc/layers/layer_collections.h"
#include "ui/gfx/geometry/rect.h"

namespace cc {

class LayerImpl;
class LayerTreeDebugState;
class LayerTreeImpl;
class HeadsUpDisplayLayerImpl;

// There are various types of debug rects:
//
// - Paint rects (update rects): regions of a layer that needed to be
// re-uploaded to the texture resource; in most cases implying that they had to
// be repainted, too.
//
// - Property-changed rects: enclosing bounds of layers that cause changes to
// the screen even if the layer did not change internally. (For example, if the
// layer's opacity or position changes.)
//
// - Surface damage rects: the aggregate damage on a target surface that is
// caused by all layers and surfaces that contribute to it. This includes (1)
// paint rects, (2) property- changed rects, and (3) newly exposed areas.
//
// - Screen space rects: this is the region the contents occupy in screen space.
//
// - Layout shift rects: regions of an animation frame that were shifted while
// the page is loading content.
enum DebugRectType {
  PAINT_RECT_TYPE,
  PROPERTY_CHANGED_RECT_TYPE,
  SURFACE_DAMAGE_RECT_TYPE,
  SCREEN_SPACE_RECT_TYPE,
  TOUCH_EVENT_HANDLER_RECT_TYPE,
  WHEEL_EVENT_HANDLER_RECT_TYPE,
  SCROLL_EVENT_HANDLER_RECT_TYPE,
  NON_FAST_SCROLLABLE_RECT_TYPE,
  MAIN_THREAD_SCROLLING_REASON_RECT_TYPE,
  ANIMATION_BOUNDS_RECT_TYPE,
  LAYOUT_SHIFT_RECT_TYPE,
};

struct DebugRect {
  DebugRect(DebugRectType new_type,
            const gfx::Rect& new_rect,
            TouchAction new_touch_action = kTouchActionNone,
            uint32_t main_thread_scrolling_reasons = 0)
      : type(new_type),
        rect(new_rect),
        touch_action(new_touch_action),
        main_thread_scrolling_reasons(main_thread_scrolling_reasons) {
    if (type != TOUCH_EVENT_HANDLER_RECT_TYPE)
      DCHECK_EQ(touch_action, kTouchActionNone);
    if (type != MAIN_THREAD_SCROLLING_REASON_RECT_TYPE)
      DCHECK(!main_thread_scrolling_reasons);
  }
  DebugRectType type;
  gfx::Rect rect;
  // Valid when |type| is |TOUCH_EVENT_HANDLER_RECT_TYPE|, otherwise default to
  // |kTouchActionNone|.
  TouchAction touch_action;
  // Valid when |type| is |MAIN_THREAD_SCROLLING_REASON_RECT_TYPE|, otherwise 0.
  uint32_t main_thread_scrolling_reasons;
};

// This class maintains a history of rects of various types that can be used
// for debugging purposes. The overhead of collecting rects is performed only if
// the appropriate LayerTreeSettings are enabled.
class DebugRectHistory {
 public:
  static std::unique_ptr<DebugRectHistory> Create();

  DebugRectHistory(const DebugRectHistory&) = delete;
  ~DebugRectHistory();

  DebugRectHistory& operator=(const DebugRectHistory&) = delete;

  // Note: Saving debug rects must happen before layers' change tracking is
  // reset.
  void SaveDebugRectsForCurrentFrame(
      LayerTreeImpl* tree_impl,
      HeadsUpDisplayLayerImpl* hud_layer,
      const RenderSurfaceList& render_surface_list,
      const LayerTreeDebugState& debug_state);

  const std::vector<DebugRect>& debug_rects() { return debug_rects_; }

 private:
  DebugRectHistory();

  void SaveLayoutShiftRects(HeadsUpDisplayLayerImpl* hud);
  void SavePaintRects(LayerTreeImpl* tree_impl);
  void SavePropertyChangedRects(LayerTreeImpl* tree_impl, LayerImpl* hud_layer);
  void SaveSurfaceDamageRects(const RenderSurfaceList& render_surface_list);
  void SaveScreenSpaceRects(const RenderSurfaceList& render_surface_list);
  void SaveTouchEventHandlerRects(LayerTreeImpl* layer);
  void SaveTouchEventHandlerRectsCallback(LayerImpl* layer);
  void SaveWheelEventHandlerRects(LayerTreeImpl* tree_impl);
  void SaveScrollEventHandlerRects(LayerTreeImpl* layer);
  void SaveScrollEventHandlerRectsCallback(LayerImpl* layer);
  void SaveNonFastScrollableRects(LayerTreeImpl* layer);
  void SaveNonFastScrollableRectsCallback(LayerImpl* layer);
  void SaveMainThreadScrollingReasonRects(LayerTreeImpl*);

  std::vector<DebugRect> debug_rects_;
};

}  // namespace cc

#endif  // CC_TREES_DEBUG_RECT_HISTORY_H_
