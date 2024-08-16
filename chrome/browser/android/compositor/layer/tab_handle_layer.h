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

  TabHandleLayer(const TabHandleLayer&) = delete;
  TabHandleLayer& operator=(const TabHandleLayer&) = delete;

  void SetProperties(int id,
                     ui::Resource* close_button_resource,
                     ui::Resource* close_button_background_resource,
                     ui::Resource* divider_resource,
                     ui::NinePatchResource* tab_handle_resource,
                     ui::NinePatchResource* tab_handle_outline_resource,
                     bool foreground,
                     bool shouldShowTabOutline,
                     bool close_pressed,
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
                     float opacity);
  scoped_refptr<cc::slim::Layer> layer() override;

 protected:
  explicit TabHandleLayer(LayerTitleCache* layer_title_cache);
  ~TabHandleLayer() override;

 private:
  raw_ptr<LayerTitleCache> layer_title_cache_;

  scoped_refptr<cc::slim::Layer> layer_;
  scoped_refptr<cc::slim::Layer> tab_;
  scoped_refptr<cc::slim::UIResourceLayer> close_button_;
  scoped_refptr<cc::slim::UIResourceLayer> close_button_hover_highlight_;
  scoped_refptr<cc::slim::UIResourceLayer> start_divider_;
  scoped_refptr<cc::slim::UIResourceLayer> end_divider_;
  scoped_refptr<cc::slim::NinePatchLayer> decoration_tab_;
  scoped_refptr<cc::slim::NinePatchLayer> tab_outline_;
  scoped_refptr<cc::slim::Layer> title_layer_;

  float brightness_;
  float opacity_;
  bool foreground_;
};

}  // namespace android

#endif  // CHROME_BROWSER_ANDROID_COMPOSITOR_LAYER_TAB_HANDLE_LAYER_H_
