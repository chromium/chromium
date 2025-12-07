// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CC_MOJOM_LAYER_TREE_DEBUG_STATE_MOJOM_TRAITS_H_
#define CC_MOJOM_LAYER_TREE_DEBUG_STATE_MOJOM_TRAITS_H_

#include "cc/debug/layer_tree_debug_state.h"
#include "cc/mojom/layer_tree_debug_state.mojom.h"

namespace mojo {

template <>
struct StructTraits<cc::mojom::LayerTreeDebugStateDataView,
                    cc::LayerTreeDebugState> {
  static bool debugger_paused(const cc::LayerTreeDebugState& state) {
    return state.debugger_paused;
  }
  static bool show_fps_counter(const cc::LayerTreeDebugState& state) {
    return state.show_fps_counter;
  }
  static bool show_render_pass_borders(const cc::LayerTreeDebugState& state) {
    return state.show_debug_borders[cc::DebugBorderType::RENDERPASS];
  }
  static bool show_surface_borders(const cc::LayerTreeDebugState& state) {
    return state.show_debug_borders[cc::DebugBorderType::SURFACE];
  }
  static bool show_layer_borders(const cc::LayerTreeDebugState& state) {
    return state.show_debug_borders[cc::DebugBorderType::LAYER];
  }
  static bool show_layout_shift_regions(const cc::LayerTreeDebugState& state) {
    return state.show_layout_shift_regions;
  }
  static bool show_paint_rects(const cc::LayerTreeDebugState& state) {
    return state.show_paint_rects;
  }
  static bool show_property_changed_rects(
      const cc::LayerTreeDebugState& state) {
    return state.show_property_changed_rects;
  }
  static bool show_surface_damage_rects(const cc::LayerTreeDebugState& state) {
    return state.show_surface_damage_rects;
  }
  static bool show_screen_space_rects(const cc::LayerTreeDebugState& state) {
    return state.show_screen_space_rects;
  }
  static bool show_touch_event_handler_rects(
      const cc::LayerTreeDebugState& state) {
    return state.show_touch_event_handler_rects;
  }
  static bool show_wheel_event_handler_rects(
      const cc::LayerTreeDebugState& state) {
    return state.show_wheel_event_handler_rects;
  }
  static bool show_scroll_event_handler_rects(
      const cc::LayerTreeDebugState& state) {
    return state.show_scroll_event_handler_rects;
  }
  static bool show_main_thread_scroll_hit_test_rects(
      const cc::LayerTreeDebugState& state) {
    return state.show_main_thread_scroll_hit_test_rects;
  }
  static bool show_main_thread_scroll_repaint_rects(
      const cc::LayerTreeDebugState& state) {
    return state.show_main_thread_scroll_repaint_rects;
  }
  static bool show_raster_inducing_scroll_rects(
      const cc::LayerTreeDebugState& state) {
    return state.show_raster_inducing_scroll_rects;
  }
  static bool show_layer_animation_bounds_rects(
      const cc::LayerTreeDebugState& state) {
    return state.show_layer_animation_bounds_rects;
  }
  static int32_t slow_down_raster_scale_factor(
      const cc::LayerTreeDebugState& state) {
    return state.slow_down_raster_scale_factor;
  }
  static bool rasterize_only_visible_content(
      const cc::LayerTreeDebugState& state) {
    return state.rasterize_only_visible_content;
  }
  static bool record_rendering_stats(const cc::LayerTreeDebugState& state) {
    return state.RecordRenderingStats();
  }

  static bool Read(cc::mojom::LayerTreeDebugStateDataView data,
                   cc::LayerTreeDebugState* out) {
    if (data.slow_down_raster_scale_factor() < 0) {
      return false;
    }
    out->debugger_paused = data.debugger_paused();
    out->show_fps_counter = data.show_fps_counter();
    out->show_debug_borders[cc::DebugBorderType::RENDERPASS] =
        data.show_render_pass_borders();
    out->show_debug_borders[cc::DebugBorderType::SURFACE] =
        data.show_surface_borders();
    out->show_debug_borders[cc::DebugBorderType::LAYER] =
        data.show_layer_borders();
    out->show_layout_shift_regions = data.show_layout_shift_regions();
    out->show_paint_rects = data.show_paint_rects();
    out->show_property_changed_rects = data.show_property_changed_rects();
    out->show_surface_damage_rects = data.show_surface_damage_rects();
    out->show_screen_space_rects = data.show_screen_space_rects();
    out->show_touch_event_handler_rects = data.show_touch_event_handler_rects();
    out->show_wheel_event_handler_rects = data.show_wheel_event_handler_rects();
    out->show_scroll_event_handler_rects =
        data.show_scroll_event_handler_rects();
    out->show_main_thread_scroll_hit_test_rects =
        data.show_main_thread_scroll_hit_test_rects();
    out->show_main_thread_scroll_repaint_rects =
        data.show_main_thread_scroll_repaint_rects();
    out->show_raster_inducing_scroll_rects =
        data.show_raster_inducing_scroll_rects();
    out->show_layer_animation_bounds_rects =
        data.show_layer_animation_bounds_rects();
    out->slow_down_raster_scale_factor = data.slow_down_raster_scale_factor();
    out->rasterize_only_visible_content = data.rasterize_only_visible_content();
    out->SetRecordRenderingStats(data.record_rendering_stats());
    return true;
  }
};

}  // namespace mojo

#endif  // CC_MOJOM_LAYER_TREE_DEBUG_STATE_MOJOM_TRAITS_H_
