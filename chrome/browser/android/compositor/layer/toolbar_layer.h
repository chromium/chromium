// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ANDROID_COMPOSITOR_LAYER_TOOLBAR_LAYER_H_
#define CHROME_BROWSER_ANDROID_COMPOSITOR_LAYER_TOOLBAR_LAYER_H_

#include "base/memory/raw_ptr.h"
#include "base/memory/ref_counted.h"
#include "chrome/browser/android/compositor/layer/layer.h"
#include "components/viz/common/quads/offset_tag.h"
#include "ui/android/resources/resource_manager.h"

namespace cc::slim {
class Layer;
class NinePatchLayer;
class SolidColorLayer;
class UIResourceLayer;
}  // namespace cc::slim

namespace android {

class ToolbarLayer : public Layer {
 public:
  static scoped_refptr<ToolbarLayer> Create(
      ui::ResourceManager* resource_manager);

  ToolbarLayer(const ToolbarLayer&) = delete;
  ToolbarLayer& operator=(const ToolbarLayer&) = delete;

  // Implements Layer
  scoped_refptr<cc::slim::Layer> layer() override;

  void PushResource(int toolbar_resource_id,
                    int toolbar_background_color,
                    bool anonymize,
                    int toolbar_textbox_background_color,
                    int url_bar_background_resource_id,
                    float x_offset,
                    float content_offset,
                    bool show_debug,
                    bool clip_shadow,
                    const viz::OffsetTag& offset_tag);

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
  int GetIndexOfLayer(scoped_refptr<cc::slim::Layer> layer);

  raw_ptr<ui::ResourceManager, DanglingUntriaged> resource_manager_;

  scoped_refptr<cc::slim::Layer> layer_;
  scoped_refptr<cc::slim::SolidColorLayer> toolbar_background_layer_;
  scoped_refptr<cc::slim::NinePatchLayer> url_bar_background_layer_;
  scoped_refptr<cc::slim::UIResourceLayer> bitmap_layer_;
  scoped_refptr<cc::slim::SolidColorLayer> progress_bar_layer_;
  scoped_refptr<cc::slim::SolidColorLayer> progress_bar_background_layer_;
  scoped_refptr<cc::slim::SolidColorLayer> debug_layer_;
};

}  //  namespace android

#endif  // CHROME_BROWSER_ANDROID_COMPOSITOR_LAYER_TOOLBAR_LAYER_H_
