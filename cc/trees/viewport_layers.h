// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CC_TREES_VIEWPORT_LAYERS_H_
#define CC_TREES_VIEWPORT_LAYERS_H_

#include "base/memory/ref_counted.h"
#include "cc/cc_export.h"
#include "cc/paint/element_id.h"

namespace cc {
class Layer;

// Viewport Layers are used to identify key layers to the compositor thread,
// so that it can perform viewport-based scrolling independently, such as
// for pinch-zoom or overscroll elasticity.
struct CC_EXPORT ViewportLayers {
  ViewportLayers();
  ~ViewportLayers();
  ElementId overscroll_elasticity_element_id;
  scoped_refptr<Layer> page_scale;
  scoped_refptr<Layer> inner_viewport_container;
  scoped_refptr<Layer> outer_viewport_container;
  scoped_refptr<Layer> inner_viewport_scroll;
  scoped_refptr<Layer> outer_viewport_scroll;
};

}  // namespace cc

#endif  // CC_TREES_VIEWPORT_LAYERS_H_
