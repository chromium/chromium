// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ANDROID_COMPOSITOR_LAYER_TAB_LAYER_H_
#define CHROME_BROWSER_ANDROID_COMPOSITOR_LAYER_TAB_LAYER_H_

#include "base/memory/raw_ptr.h"
#include "chrome/browser/android/compositor/layer/layer.h"
#include "ui/android/resources/nine_patch_resource.h"

namespace cc::slim {
class Layer;
class NinePatchLayer;
class SolidColorLayer;
}  // namespace cc::slim

namespace gfx {
class Size;
}

namespace ui {
class ResourceManager;
}

namespace android {

class ContentLayer;
class TabContentManager;
class ToolbarLayer;

// Sub layer tree representation of a tab.  A TabLayer is not tied to
// specific tab. To specialize it call CustomizeForId() and SetProperties().
class TabLayer : public Layer {
 public:
  static scoped_refptr<TabLayer> Create(bool incognito,
                                        ui::ResourceManager* resource_manager,
                                        TabContentManager* tab_content_manager);

  TabLayer(const TabLayer&) = delete;
  TabLayer& operator=(const TabLayer&) = delete;

  // TODO(meiliang): This method needs another parameter, a resource that can be
  // used to indicate the currently selected tab for the TabLayer.
  void SetProperties(int id,
                     bool can_use_live_layer,
                     int toolbar_resource_id,
                     int shadow_resource_id,
                     int contour_resource_id,
                     int border_resource_id,
                     int border_inner_shadow_resource_id,
                     int default_background_color,
                     float x,
                     float y,
                     float width,
                     float height,
                     float shadow_width,
                     float shadow_height,
                     float alpha,
                     float border_alpha,
                     float border_inner_shadow_alpha,
                     float contour_alpha,
                     float shadow_alpha,
                     float border_scale,
                     float saturation,
                     float static_to_view_blend,
                     float content_width,
                     float content_height,
                     float view_width,
                     bool show_toolbar,
                     int default_theme_color,
                     int toolbar_background_color,
                     bool anonymize_toolbar,
                     int toolbar_textbox_resource_id,
                     int toolbar_textbox_background_color,
                     float content_offset);

  bool is_incognito() const { return incognito_; }

  scoped_refptr<cc::slim::Layer> layer() override;

  static void ComputePaddingPositions(const gfx::Size& content_size,
                                      const gfx::Size& desired_size,
                                      gfx::Rect* side_padding_rect,
                                      gfx::Rect* bottom_padding_rect);

 protected:
  TabLayer(bool incognito,
           ui::ResourceManager* resource_manager,
           TabContentManager* tab_content_manager);
  ~TabLayer() override;

 private:
  const bool incognito_;
  raw_ptr<ui::ResourceManager> resource_manager_;
  raw_ptr<TabContentManager> tab_content_manager_;

  // [layer]-+-[toolbar]
  //         +-[front border]
  //         +-[content]
  //         +-[padding]
  //         +-[contour_shadow]
  //         +-[shadow]
  scoped_refptr<cc::slim::Layer> layer_;
  scoped_refptr<ToolbarLayer> toolbar_layer_;
  scoped_refptr<ContentLayer> content_;
  scoped_refptr<cc::slim::SolidColorLayer> side_padding_;
  scoped_refptr<cc::slim::SolidColorLayer> bottom_padding_;

  scoped_refptr<cc::slim::NinePatchLayer> front_border_;
  scoped_refptr<cc::slim::NinePatchLayer> front_border_inner_shadow_;

  scoped_refptr<cc::slim::NinePatchLayer> contour_shadow_;

  scoped_refptr<cc::slim::NinePatchLayer> shadow_;
};

}  //  namespace android

#endif  // CHROME_BROWSER_ANDROID_COMPOSITOR_LAYER_TAB_LAYER_H_
