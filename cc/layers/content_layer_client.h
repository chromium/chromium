// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CC_LAYERS_CONTENT_LAYER_CLIENT_H_
#define CC_LAYERS_CONTENT_LAYER_CLIENT_H_

#include <stddef.h>

#include "cc/cc_export.h"
#include "cc/paint/display_item_list.h"

namespace cc {

class CC_EXPORT ContentLayerClient {
 public:
  // Paints the content area for the layer into a DisplayItemList.
  virtual scoped_refptr<DisplayItemList> PaintContentsToDisplayList() = 0;

  // If true the layer may skip clearing the background before rasterizing,
  // because it will cover any uncleared data with content.
  virtual bool FillsBoundsCompletely() const = 0;

 protected:
  virtual ~ContentLayerClient() {}
};

}  // namespace cc

#endif  // CC_LAYERS_CONTENT_LAYER_CLIENT_H_
