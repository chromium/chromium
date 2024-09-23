// Copyright 2011 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CC_DEBUG_LAYER_TREE_DEBUG_STATE_H_
#define CC_DEBUG_LAYER_TREE_DEBUG_STATE_H_

#include <bitset>

#include "cc/debug/debug_export.h"

namespace cc {

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

  bool debugger_paused = false;
  bool show_fps_counter = false;
  DebugBorderTypes show_debug_borders = false;

  bool show_layout_shift_regions = false;
  bool show_paint_rects = false;
  bool show_property_changed_rects = false;
  bool show_surface_damage_rects = false;
  bool show_screen_space_rects = false;
  bool show_touch_event_handler_rects = false;
  bool show_wheel_event_handler_rects = false;
  bool show_scroll_event_handler_rects = false;
  bool show_main_thread_scroll_hit_test_rects = false;
  bool show_main_thread_scroll_repaint_rects = false;
  bool show_raster_inducing_scroll_rects = false;
  bool show_layer_animation_bounds_rects = false;

  int slow_down_raster_scale_factor = 0;
  bool rasterize_only_visible_content = false;
  bool highlight_non_lcd_text_layers = false;

  // This is part of the feature to show performance metrics on HUD. This
  // particular flag is set only in Blink.

  void SetRecordRenderingStats(bool enabled);
  bool RecordRenderingStats() const;

  // HUD layer is responsible for drawing debug rects as well as displaying HUD
  // overlay. This function checks if a HUD layer should be created for any of
  // these situations.
  bool ShouldCreateHudLayer() const;
  bool ShowDebugRects() const;
  bool ShowMemoryStats() const;
  bool ShouldDrawHudInfo() const;
  void TurnOffHudInfoDisplay();

  bool operator==(const LayerTreeDebugState&) const;

 private:
  bool record_rendering_stats_ = false;
};

}  // namespace cc

#endif  // CC_DEBUG_LAYER_TREE_DEBUG_STATE_H_
