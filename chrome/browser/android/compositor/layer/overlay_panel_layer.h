// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ANDROID_COMPOSITOR_LAYER_OVERLAY_PANEL_LAYER_H_
#define CHROME_BROWSER_ANDROID_COMPOSITOR_LAYER_OVERLAY_PANEL_LAYER_H_

#include <memory>

#include "chrome/browser/android/compositor/layer/layer.h"

namespace cc {
class Layer;
class NinePatchLayer;
class SolidColorLayer;
class UIResourceLayer;
}

namespace ui {
class ResourceManager;
}

namespace android {

class OverlayPanelLayer : public Layer {
 public:
  // Default width for any icon displayed on an OverlayPanel.
  static constexpr float kDefaultIconWidthDp = 36.0f;

  // ID for Invalid resource.
  static constexpr int kInvalidResourceID = -1;

  void SetResourceIds(int bar_text_resource_id,
                      int panel_shadow_resource_id,
                      int rounded_bar_top_resource_id,
                      int bar_shadow_resource_id,
                      int panel_icon_resource_id,
                      int drag_handlebar_resource_id,
                      int open_tab_resource_id,
                      int close_icon_resource_id);

  void SetProperties(float dp_to_px,
                     const scoped_refptr<cc::Layer>& content_layer,
                     float content_offset_y,
                     float panel_x,
                     float panel_y,
                     float panel_width,
                     float panel_height,
                     int bar_background_color,
                     float bar_margin_side,
                     float bar_margin_top,
                     float bar_height,
                     float bar_offset_y,
                     float bar_text_opacity,
                     bool bar_border_visible,
                     float bar_border_height,
                     int icon_tint,
                     int drag_handlebar_tint,
                     float icon_opacity,
                     int separator_line_color);

  void SetProgressBar(int progress_bar_background_resource_id,
                      int progress_bar_resource_id,
                      bool progress_bar_visible,
                      float progress_bar_position_y,
                      float progress_bar_height,
                      float progress_bar_opacity,
                      float progress_bar_completion,
                      float panel_width);

  scoped_refptr<cc::Layer> layer() override;

 protected:
  explicit OverlayPanelLayer(ui::ResourceManager* resource_manager);
  ~OverlayPanelLayer() override;

  virtual scoped_refptr<cc::Layer> GetIconLayer();
  void AddBarTextLayer(scoped_refptr<cc::Layer> text_layer);

  ui::ResourceManager* resource_manager_;
  scoped_refptr<cc::Layer> layer_;

  scoped_refptr<cc::NinePatchLayer> panel_shadow_;
  scoped_refptr<cc::NinePatchLayer> panel_shadow_right_;
  scoped_refptr<cc::NinePatchLayer> rounded_bar_top_;
  scoped_refptr<cc::SolidColorLayer> bar_background_;
  scoped_refptr<cc::UIResourceLayer> bar_text_;
  scoped_refptr<cc::UIResourceLayer> bar_shadow_;
  scoped_refptr<cc::UIResourceLayer> panel_icon_;
  scoped_refptr<cc::UIResourceLayer> drag_handlebar_;
  scoped_refptr<cc::UIResourceLayer> open_tab_icon_;
  scoped_refptr<cc::UIResourceLayer> close_icon_;
  scoped_refptr<cc::Layer> content_container_;
  scoped_refptr<cc::Layer> text_container_;
  scoped_refptr<cc::SolidColorLayer> bar_border_;
  scoped_refptr<cc::NinePatchLayer> progress_bar_;
  scoped_refptr<cc::NinePatchLayer> progress_bar_background_;

  int panel_icon_resource_id_;
  int bar_text_resource_id_;
  int panel_shadow_resource_id_;
  int rounded_bar_top_resource_id_;
  int bar_shadow_resource_id_;
  int drag_handlebar_resource_id_;
  int open_tab_icon_resource_id_;
  int close_icon_resource_id_;
};

}  //  namespace android

#endif  // CHROME_BROWSER_ANDROID_COMPOSITOR_LAYER_OVERLAY_PANEL_LAYER_H_
