// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.


#ifndef CC_LAYERS_SOLID_COLOR_LAYER_H_
#define CC_LAYERS_SOLID_COLOR_LAYER_H_

#include <memory>

#include "cc/cc_export.h"
#include "cc/layers/layer.h"

namespace cc {

// A Layer that renders a solid color. The color is specified by using
// SetBackgroundColor() on the base class.
class CC_EXPORT SolidColorLayer : public Layer {
 public:
  static scoped_refptr<SolidColorLayer> Create();

  SolidColorLayer(const SolidColorLayer&) = delete;
  SolidColorLayer& operator=(const SolidColorLayer&) = delete;

  std::unique_ptr<LayerImpl> CreateLayerImpl(
      LayerTreeImpl* tree_impl) const override;

  void SetBackgroundColor(SkColor4f color) override;
  sk_sp<const SkPicture> GetPicture() const override;
  bool IsSolidColorLayerForTesting() const override;

 protected:
  SolidColorLayer();

 private:
  ~SolidColorLayer() override;
};

}  // namespace cc
#endif  // CC_LAYERS_SOLID_COLOR_LAYER_H_
