// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ANDROID_COMPOSITOR_LAYER_TAB_HANDLE_LAYER_H_
#define CHROME_BROWSER_ANDROID_COMPOSITOR_LAYER_TAB_HANDLE_LAYER_H_

#include "base/memory/raw_ptr.h"
#include "chrome/browser/android/compositor/layer/layer.h"
#include "ui/android/resources/resource_manager.h"

namespace cc::slim {
class Layer;
class NinePatchLayer;
class UIResourceLayer;
class SolidColorLayer;
}  // namespace cc::slim

namespace ui {
class NinePatchResource;
}

namespace android {

class LayerTitleCache;

class TabHandleLayer : public Layer {
 public:
  static scoped_refptr<TabHandleLayer> Create(
      LayerTitleCache* layer_title_cache);
  static void SetConstants(float tab_underline_thickness,
                           float tab_underline_corner_radius,
                           float tab_underline_bottom_margin);

  TabHandleLayer(const TabHandleLayer&) = delete;
  TabHandleLayer& operator=(const TabHandleLayer&) = delete;

  void SetProperties(int id,
                     ui::Resource* close_button_resource,
                     ui::Resource* close_button_background_resource,
                     bool is_close_keyboard_focused,
                     ui::Resource* close_button_keyboard_focus_ring_resource,
                     ui::Resource* divider_resource,
                     ui::NinePatchResource* tab_handle_resource,
                     ui::NinePatchResource* tab_handle_outline_resource,
                     bool foreground,
                     bool is_pinned,
                     bool shouldShowTabOutline,
                     bool close_pressed,
                     bool should_hide_favicon,
                     bool should_show_media_indicator,
                     ui::Resource* media_indicator_resource,
                     float media_indicator_width,
                     float media_indicator_spacing,
                     float media_indicator_internal_padding,
                     float title_to_media_indicator_spacing,
                     float media_indicator_opacity,
                     ui::Resource* tab_indicator_overlay_resource,
                     float target_rotation,
                     float tab_indicator_overlay_width,
                     float toolbar_width,
                     float x,
                     float y,
                     float width,
                     float height,
                     float content_offset_y,
                     float divider_offset_x,
                     float bottom_margin,
                     float top_margin,
                     float close_button_padding,
                     float close_button_alpha,
                     bool is_start_divider_visible,
                     bool is_end_divider_visible,
                     bool is_loading,
                     float spinner_rotation,
                     float opacity,
                     bool is_keyboard_focused,
                     ui::NinePatchResource* keyboard_focus_ring_drawable,
                     int keyboard_focus_ring_offset,
                     int stroke_width,
                     float folio_foot_length,
                     float width_to_hide_tab_title,
                     float pinned_icon_offset_x,
                     bool is_underlined,
                     SkColor underline_start_color,
                     SkColor underline_end_color,
                     int underline_width_threshold);
  bool foreground();
  bool is_pinned();
  scoped_refptr<cc::slim::Layer> layer() override;

 protected:
  explicit TabHandleLayer(LayerTitleCache* layer_title_cache);
  ~TabHandleLayer() override;

 private:
  UI_ANDROID_EXPORT static inline float tab_underline_thickness_;
  UI_ANDROID_EXPORT static inline float tab_underline_corner_radius_;
  UI_ANDROID_EXPORT static inline float tab_underline_bottom_margin_;

  raw_ptr<LayerTitleCache> layer_title_cache_;

  scoped_refptr<cc::slim::Layer> layer_;
  scoped_refptr<cc::slim::Layer> tab_;
  scoped_refptr<cc::slim::UIResourceLayer> close_button_;
  scoped_refptr<cc::slim::UIResourceLayer> close_button_hover_highlight_;
  scoped_refptr<cc::slim::UIResourceLayer> close_keyboard_focus_ring_;
  scoped_refptr<cc::slim::UIResourceLayer> start_divider_;
  scoped_refptr<cc::slim::UIResourceLayer> end_divider_;
  scoped_refptr<cc::slim::UIResourceLayer> media_indicator_layer_;
  scoped_refptr<cc::slim::UIResourceLayer> tab_indicator_overlay_layer_;
  scoped_refptr<cc::slim::NinePatchLayer> decoration_tab_;
  scoped_refptr<cc::slim::NinePatchLayer> tab_outline_;
  scoped_refptr<cc::slim::Layer> title_layer_;
  scoped_refptr<cc::slim::SolidColorLayer> underline_end_layer_;
  scoped_refptr<cc::slim::SolidColorLayer> underline_start_layer_;

  scoped_refptr<cc::slim::NinePatchLayer> keyboard_focus_ring_;

  float opacity_;
  float tab_indicator_overlay_rotation_ = 0.f;
  std::unique_ptr<gfx::Transform> transform_;
  bool foreground_ = false;
  bool is_pinned_ = false;
};

}  // namespace android

#endif  // CHROME_BROWSER_ANDROID_COMPOSITOR_LAYER_TAB_HANDLE_LAYER_H_
