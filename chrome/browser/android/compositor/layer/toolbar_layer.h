// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ANDROID_COMPOSITOR_LAYER_TOOLBAR_LAYER_H_
#define CHROME_BROWSER_ANDROID_COMPOSITOR_LAYER_TOOLBAR_LAYER_H_

#include <memory>

#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "cc/layers/nine_patch_layer.h"
#include "chrome/browser/android/compositor/layer/layer.h"
#include "ui/android/resources/resource_manager.h"

namespace cc {
class Layer;
class SolidColorLayer;
class UIResourceLayer;
}

namespace android {

class ToolbarLayer : public Layer {
 public:
  static scoped_refptr<ToolbarLayer> Create(
      ui::ResourceManager* resource_manager);

  // Implements Layer
  scoped_refptr<cc::Layer> layer() override;

  void PushResource(int toolbar_resource_id,
                    int toolbar_background_color,
                    bool anonymize,
                    int toolbar_textbox_background_color,
                    int url_bar_background_resource_id,
                    float url_bar_alpha,
                    float window_height,
                    float y_offset,
                    bool show_debug,
                    bool clip_shadow);

  void UpdateProgressBar(int progress_bar_x,
                         int progress_bar_y,
                         int progress_bar_width,
                         int progress_bar_height,
                         int progress_bar_color,
                         int progress_bar_background_x,
                         int progress_bar_background_y,
                         int progress_bar_background_width,
                         int progress_bar_background_height,
                         int progress_bar_background_color);

  void SetOpacity(float opacity);

 protected:
  explicit ToolbarLayer(ui::ResourceManager* resource_manager);
  ~ToolbarLayer() override;

 private:
  int GetIndexOfLayer(scoped_refptr<cc::Layer> layer);

  ui::ResourceManager* resource_manager_;

  scoped_refptr<cc::Layer> layer_;
  scoped_refptr<cc::SolidColorLayer> toolbar_background_layer_;
  scoped_refptr<cc::NinePatchLayer> url_bar_background_layer_;
  scoped_refptr<cc::UIResourceLayer> bitmap_layer_;
  scoped_refptr<cc::SolidColorLayer> progress_bar_layer_;
  scoped_refptr<cc::SolidColorLayer> progress_bar_background_layer_;
  scoped_refptr<cc::SolidColorLayer> debug_layer_;

  DISALLOW_COPY_AND_ASSIGN(ToolbarLayer);
};

}  //  namespace android

#endif  // CHROME_BROWSER_ANDROID_COMPOSITOR_LAYER_TOOLBAR_LAYER_H_
