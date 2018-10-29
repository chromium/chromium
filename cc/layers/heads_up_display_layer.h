// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CC_LAYERS_HEADS_UP_DISPLAY_LAYER_H_
#define CC_LAYERS_HEADS_UP_DISPLAY_LAYER_H_

#include <memory>
#include <string>

#include "base/macros.h"
#include "cc/cc_export.h"
#include "cc/layers/layer.h"
#include "third_party/skia/include/core/SkRefCnt.h"
#include "third_party/skia/include/core/SkTypeface.h"

namespace cc {

class CC_EXPORT HeadsUpDisplayLayer : public Layer {
 public:
  static scoped_refptr<HeadsUpDisplayLayer> Create();

  void UpdateLocationAndSize(const gfx::Size& device_viewport,
                             float device_scale_factor);

  std::unique_ptr<LayerImpl> CreateLayerImpl(LayerTreeImpl* tree_impl) override;

  // Layer overrides.
  void PushPropertiesTo(LayerImpl* layer) override;

 protected:
  HeadsUpDisplayLayer();
  bool HasDrawableContent() const override;

 private:
  ~HeadsUpDisplayLayer() override;

  sk_sp<SkTypeface> typeface_;

  DISALLOW_COPY_AND_ASSIGN(HeadsUpDisplayLayer);
};

}  // namespace cc

#endif  // CC_LAYERS_HEADS_UP_DISPLAY_LAYER_H_
