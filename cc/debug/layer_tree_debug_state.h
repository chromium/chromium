// Copyright 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CC_DEBUG_LAYER_TREE_DEBUG_STATE_H_
#define CC_DEBUG_LAYER_TREE_DEBUG_STATE_H_

#include <bitset>

#include "cc/debug/debug_export.h"

namespace cc {

namespace proto {
class LayerTreeDebugState;
}  // namespace proto

enum DebugBorderType {
  RENDERPASS = 0,
  SURFACE,
  LAYER,
  LAST_DEBUG_BORDER_TYPE = LAYER
};
using DebugBorderTypes = std::bitset<LAST_DEBUG_BORDER_TYPE + 1>;

class CC_DEBUG_EXPORT LayerTreeDebugState {
 public:
  LayerTreeDebugState();
  LayerTreeDebugState(const LayerTreeDebugState& other);
  ~LayerTreeDebugState();

  bool show_fps_counter;
  DebugBorderTypes show_debug_borders;

  bool show_layout_shift_regions;
  bool show_paint_rects;
  bool show_property_changed_rects;
  bool show_surface_damage_rects;
  bool show_screen_space_rects;
  bool show_touch_event_handler_rects;
  bool show_wheel_event_handler_rects;
  bool show_scroll_event_handler_rects;
  bool show_non_fast_scrollable_rects;
  bool show_main_thread_scrolling_reason_rects;
  bool show_layer_animation_bounds_rects;

  int slow_down_raster_scale_factor;
  bool rasterize_only_visible_content;
  bool highlight_non_lcd_text_layers;

  bool show_hit_test_borders;

  void SetRecordRenderingStats(bool enabled);
  bool RecordRenderingStats() const;

  bool ShowHudInfo() const;
  bool ShowHudRects() const;
  bool ShowMemoryStats() const;

  static bool Equal(const LayerTreeDebugState& a, const LayerTreeDebugState& b);
  static LayerTreeDebugState Unite(const LayerTreeDebugState& a,
                                   const LayerTreeDebugState& b);

 private:
  bool record_rendering_stats_;
};

}  // namespace cc

#endif  // CC_DEBUG_LAYER_TREE_DEBUG_STATE_H_
