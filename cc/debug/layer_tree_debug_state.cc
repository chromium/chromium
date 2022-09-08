// Copyright 2011 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/debug/layer_tree_debug_state.h"

namespace cc {

// IMPORTANT: new fields must be added to Equal() and Unite()
LayerTreeDebugState::LayerTreeDebugState() = default;

LayerTreeDebugState::LayerTreeDebugState(const LayerTreeDebugState& other) =
    default;

LayerTreeDebugState::~LayerTreeDebugState() = default;

void LayerTreeDebugState::SetRecordRenderingStats(bool enabled) {
  record_rendering_stats_ = enabled;
}

bool LayerTreeDebugState::RecordRenderingStats() const {
  return record_rendering_stats_;
}

bool LayerTreeDebugState::ShouldCreateHudLayer() const {
  return ShowDebugRects() || ShouldDrawHudInfo();
}

bool LayerTreeDebugState::ShowDebugRects() const {
  return show_paint_rects || show_property_changed_rects ||
         show_surface_damage_rects || show_screen_space_rects ||
         show_touch_event_handler_rects || show_wheel_event_handler_rects ||
         show_scroll_event_handler_rects || show_non_fast_scrollable_rects ||
         show_main_thread_scrolling_reason_rects ||
         show_layer_animation_bounds_rects || show_layout_shift_regions;
}

bool LayerTreeDebugState::ShowMemoryStats() const {
  return show_fps_counter;
}

bool LayerTreeDebugState::ShouldDrawHudInfo() const {
  return show_fps_counter || show_web_vital_metrics || show_smoothness_metrics;
}

void LayerTreeDebugState::TurnOffHudInfoDisplay() {
  // Turn off all types of HUD info display. ShouldDrawHudInfo() would return
  // false after this function.
  show_fps_counter = false;
  show_web_vital_metrics = false;
  show_smoothness_metrics = false;
}

bool LayerTreeDebugState::Equal(const LayerTreeDebugState& a,
                                const LayerTreeDebugState& b) {
  return (
      a.show_fps_counter == b.show_fps_counter &&
      a.show_debug_borders == b.show_debug_borders &&
      a.show_layout_shift_regions == b.show_layout_shift_regions &&
      a.show_paint_rects == b.show_paint_rects &&
      a.show_property_changed_rects == b.show_property_changed_rects &&
      a.show_surface_damage_rects == b.show_surface_damage_rects &&
      a.show_screen_space_rects == b.show_screen_space_rects &&
      a.show_touch_event_handler_rects == b.show_touch_event_handler_rects &&
      a.show_wheel_event_handler_rects == b.show_wheel_event_handler_rects &&
      a.show_scroll_event_handler_rects == b.show_scroll_event_handler_rects &&
      a.show_non_fast_scrollable_rects == b.show_non_fast_scrollable_rects &&
      a.show_main_thread_scrolling_reason_rects ==
          b.show_main_thread_scrolling_reason_rects &&
      a.show_layer_animation_bounds_rects ==
          b.show_layer_animation_bounds_rects &&
      a.slow_down_raster_scale_factor == b.slow_down_raster_scale_factor &&
      a.rasterize_only_visible_content == b.rasterize_only_visible_content &&
      a.highlight_non_lcd_text_layers == b.highlight_non_lcd_text_layers &&
      a.show_web_vital_metrics == b.show_web_vital_metrics &&
      a.record_rendering_stats_ == b.record_rendering_stats_);
}

}  // namespace cc
