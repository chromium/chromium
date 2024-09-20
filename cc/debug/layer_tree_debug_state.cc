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
         show_scroll_event_handler_rects ||
         show_main_thread_scroll_hit_test_rects ||
         show_main_thread_scroll_repaint_rects ||
         show_raster_inducing_scroll_rects ||
         show_layer_animation_bounds_rects || show_layout_shift_regions;
}

bool LayerTreeDebugState::ShowMemoryStats() const {
  return show_fps_counter;
}

bool LayerTreeDebugState::ShouldDrawHudInfo() const {
  return show_fps_counter || debugger_paused;
}

void LayerTreeDebugState::TurnOffHudInfoDisplay() {
  // Turn off all types of HUD info display. We do not reset `debugger_paused`.
  show_fps_counter = false;
}

bool LayerTreeDebugState::operator==(const LayerTreeDebugState&) const =
    default;

}  // namespace cc
