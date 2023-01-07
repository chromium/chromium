// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ANDROID_COMPOSITOR_LAYER_TABGROUP_CONTENT_LAYER_H_
#define CHROME_BROWSER_ANDROID_COMPOSITOR_LAYER_TABGROUP_CONTENT_LAYER_H_

#include <memory>
#include <vector>

#include "chrome/browser/android/compositor/layer/content_layer.h"
#include "ui/android/resources/nine_patch_resource.h"
#include "ui/gfx/geometry/rect.h"

namespace android {

class TabContentManager;
class TabGroupTabContentLayer;

// Sub layer tree representation of the contents of a
// set of four tabs within the group.
class TabGroupContentLayer : public ContentLayer {
 public:
  static scoped_refptr<TabGroupContentLayer> Create(
      TabContentManager* tab_content_manager);

  TabGroupContentLayer(const TabGroupContentLayer&) = delete;
  TabGroupContentLayer& operator=(const TabGroupContentLayer&) = delete;

  void SetProperties(int id,
                     const std::vector<int>& tab_ids,
                     bool can_use_live_layer,
                     float static_to_view_blend,
                     bool should_override_content_alpha,
                     float content_alpha_override,
                     float saturation,
                     bool should_clip,
                     const gfx::Rect& clip,
                     ui::NinePatchResource* inner_shadow_resource,
                     float inner_shadow_alpha);

 protected:
  explicit TabGroupContentLayer(TabContentManager* tab_content_manager);
  ~TabGroupContentLayer() override;

 private:
  std::vector<scoped_refptr<TabGroupTabContentLayer>> group_tab_content_layers_;
};

}  //  namespace android

#endif  // CHROME_BROWSER_ANDROID_COMPOSITOR_LAYER_TABGROUP_CONTENT_LAYER_H_
