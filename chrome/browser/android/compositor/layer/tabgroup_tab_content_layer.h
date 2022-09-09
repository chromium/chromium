// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ANDROID_COMPOSITOR_LAYER_TABGROUP_TAB_CONTENT_LAYER_H_
#define CHROME_BROWSER_ANDROID_COMPOSITOR_LAYER_TABGROUP_TAB_CONTENT_LAYER_H_

#include <memory>
#include <vector>

#include "chrome/browser/android/compositor/layer/layer.h"
#include "ui/android/resources/nine_patch_resource.h"
#include "ui/gfx/geometry/rect.h"

namespace cc {
class Layer;
class NinePatchLayer;
}  // namespace cc

namespace android {

class ContentLayer;
class TabContentManager;

// Sub layer tree representation of the contents of a
// tabgroup tab with a border shadow around.
class TabGroupTabContentLayer : public Layer {
 public:
  static scoped_refptr<TabGroupTabContentLayer> Create(
      TabContentManager* tab_content_manager);

  TabGroupTabContentLayer(const TabGroupTabContentLayer&) = delete;
  TabGroupTabContentLayer& operator=(const TabGroupTabContentLayer&) = delete;

  void SetProperties(int id,
                     bool can_use_live_layer,
                     float static_to_view_blend,
                     bool should_override_content_alpha,
                     float content_alpha_override,
                     float saturation,
                     bool should_clip,
                     const gfx::Rect& clip,
                     ui::NinePatchResource* border_inner_shadow_resource,
                     const std::vector<int>& tab_ids,
                     float border_inner_shadow_alpha);

  scoped_refptr<cc::Layer> layer() override;

 protected:
  explicit TabGroupTabContentLayer(TabContentManager* tab_content_manager);
  ~TabGroupTabContentLayer() override;

 private:
  void setBorderProperties(ui::NinePatchResource* border_inner_shadow_resource,
                           const gfx::Rect& clip,
                           float border_inner_shadow_alpha);
  scoped_refptr<cc::Layer> layer_;
  scoped_refptr<ContentLayer> content_;
  scoped_refptr<cc::NinePatchLayer> front_border_inner_shadow_;
};

}  //  namespace android

#endif  // CHROME_BROWSER_ANDROID_COMPOSITOR_LAYER_TABGROUP_TAB_CONTENT_LAYER_H_
