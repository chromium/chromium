// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ANDROID_COMPOSITOR_LAYER_TAB_HANDLE_LAYER_H_
#define CHROME_BROWSER_ANDROID_COMPOSITOR_LAYER_TAB_HANDLE_LAYER_H_

#include <memory>

#include "base/macros.h"
#include "cc/layers/nine_patch_layer.h"
#include "cc/layers/solid_color_layer.h"
#include "cc/layers/ui_resource_layer.h"
#include "chrome/browser/android/compositor/layer/layer.h"
#include "ui/android/resources/resource_manager.h"

namespace cc {
class Layer;
class NinePatchLayer;
}

namespace ui {
class NinePatchResource;
}

namespace android {

class LayerTitleCache;

class TabHandleLayer : public Layer {
 public:
  static scoped_refptr<TabHandleLayer> Create(
      LayerTitleCache* layer_title_cache);

  void SetProperties(int id,
                     ui::Resource* close_button_resource,
                     ui::NinePatchResource* tab_handle_resource,
                     ui::NinePatchResource* tab_handle_outline_resource,
                     bool foreground,
                     bool close_pressed,
                     float toolbar_width,
                     float x,
                     float y,
                     float width,
                     float height,
                     float content_offset_x,
                     float close_button_alpha,
                     bool is_loading,
                     float spinner_rotation,
                     float brightness);
  scoped_refptr<cc::Layer> layer() override;

 protected:
  explicit TabHandleLayer(LayerTitleCache* layer_title_cache);
  ~TabHandleLayer() override;

 private:
  LayerTitleCache* layer_title_cache_;

  scoped_refptr<cc::Layer> layer_;
  scoped_refptr<cc::UIResourceLayer> close_button_;
  scoped_refptr<cc::NinePatchLayer> decoration_tab_;
  scoped_refptr<cc::NinePatchLayer> tab_outline_;
  scoped_refptr<cc::Layer> title_layer_;

  float brightness_;
  bool foreground_;

  DISALLOW_COPY_AND_ASSIGN(TabHandleLayer);
};

}  // namespace android

#endif  // CHROME_BROWSER_ANDROID_COMPOSITOR_LAYER_TAB_HANDLE_LAYER_H_
