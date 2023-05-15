// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ANDROID_COMPOSITOR_LAYER_CONTENT_LAYER_H_
#define CHROME_BROWSER_ANDROID_COMPOSITOR_LAYER_CONTENT_LAYER_H_

#include "base/memory/raw_ptr.h"
#include "chrome/browser/android/compositor/layer/layer.h"
#include "ui/android/resources/nine_patch_resource.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/gfx/geometry/size.h"

namespace cc::slim {
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

  ContentLayer(const ContentLayer&) = delete;
  ContentLayer& operator=(const ContentLayer&) = delete;

  void SetProperties(int id,
                     bool can_use_live_layer,
                     float static_to_view_blend,
                     bool should_override_content_alpha,
                     float content_alpha_override,
                     float saturation,
                     bool should_clip,
                     const gfx::Rect& clip);

  scoped_refptr<cc::slim::Layer> layer() override;

  gfx::Size ComputeSize(int id) const;

 protected:
  explicit ContentLayer(TabContentManager* tab_content_manager);
  ~ContentLayer() override;

 private:
  // This is an intermediate shim layer whose children are
  // both the static and content layers (or either, or none, depending on which
  // is available).
  scoped_refptr<cc::slim::Layer> layer_;
  raw_ptr<TabContentManager> tab_content_manager_;
};

}  //  namespace android

#endif  // CHROME_BROWSER_ANDROID_COMPOSITOR_LAYER_CONTENT_LAYER_H_
