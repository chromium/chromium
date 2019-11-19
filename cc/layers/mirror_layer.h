// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CC_LAYERS_MIRROR_LAYER_H_
#define CC_LAYERS_MIRROR_LAYER_H_

#include "base/memory/scoped_refptr.h"
#include "cc/cc_export.h"
#include "cc/layers/layer.h"

namespace cc {

// A layer that can mirror contents of another Layer.
class CC_EXPORT MirrorLayer : public Layer {
 public:
  static scoped_refptr<MirrorLayer> Create(scoped_refptr<Layer> mirrored_layer);

  MirrorLayer(const MirrorLayer&) = delete;
  MirrorLayer& operator=(const MirrorLayer&) = delete;

  Layer* mirrored_layer() const { return mirrored_layer_.get(); }

  // Layer overrides.
  std::unique_ptr<LayerImpl> CreateLayerImpl(LayerTreeImpl* tree_impl) override;
  void PushPropertiesTo(LayerImpl* layer) override;
  void SetLayerTreeHost(LayerTreeHost* host) override;

 protected:
  explicit MirrorLayer(scoped_refptr<Layer> mirrored_layer);

 private:
  ~MirrorLayer() override;

  // A reference to a layer that is mirrored by this layer. |mirrored_layer_|
  // cannot be an ancestor of |this|.
  scoped_refptr<Layer> mirrored_layer_;
};

}  // namespace cc

#endif  // CC_LAYERS_MIRROR_LAYER_H_
