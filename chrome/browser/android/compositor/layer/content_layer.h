// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ANDROID_COMPOSITOR_LAYER_CONTENT_LAYER_H_
#define CHROME_BROWSER_ANDROID_COMPOSITOR_LAYER_CONTENT_LAYER_H_

#include <memory>

#include "base/macros.h"
#include "chrome/browser/android/compositor/layer/layer.h"
#include "ui/android/resources/nine_patch_resource.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/gfx/geometry/size.h"

namespace cc {
class Layer;
}

namespace android {

class TabContentManager;

// Sub layer tree representation of the contents of a tab.
// Contains logic to temporarily display a static thumbnail
// when the content layer is not available.
// To specialize call SetProperties.
class ContentLayer : public Layer {
 public:
  static scoped_refptr<ContentLayer> Create(
      TabContentManager* tab_content_manager);
  void SetProperties(int id,
                     bool can_use_live_layer,
                     float static_to_view_blend,
                     bool should_override_content_alpha,
                     float content_alpha_override,
                     float saturation,
                     bool should_clip,
                     const gfx::Rect& clip);

  scoped_refptr<cc::Layer> layer() override;

  gfx::Size ComputeSize(int id) const;

 protected:
  explicit ContentLayer(TabContentManager* tab_content_manager);
  ~ContentLayer() override;
  // This is an intermediate shim layer whose children are
  // both the static and content layers (or either, or none, depending on which
  // is available).
  scoped_refptr<cc::Layer> layer_;
  TabContentManager* tab_content_manager_;

 private:
  DISALLOW_COPY_AND_ASSIGN(ContentLayer);
};

}  //  namespace android

#endif  // CHROME_BROWSER_ANDROID_COMPOSITOR_LAYER_CONTENT_LAYER_H_
