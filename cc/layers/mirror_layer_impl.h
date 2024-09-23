// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CC_LAYERS_MIRROR_LAYER_IMPL_H_
#define CC_LAYERS_MIRROR_LAYER_IMPL_H_

#include <cstdint>
#include <memory>

#include "base/memory/ptr_util.h"
#include "cc/cc_export.h"
#include "cc/layers/layer_impl.h"

namespace cc {

// This type of layer is used to mirror contents of another layer (specified by
// |mirrored_layer_id_|) by forcing a render pass for the mirrored layer and
// adding a CompositorRenderPassDrawQuad in the compositor frame for this layer
// referring to that render pass. The mirroring layer should not be a descendant
// of the mirrored layer (in terms of the effect tree). Due to ordering
// requirements for render passes in the compositor frame, the render pass
// containing mirroring layer should appear after the render pass created for
// the mirrored layer. Currently, render passes are in reverse-draw order of the
// effect tree, so we should be careful that this reverse-draw order does not
// conflict with render pass ordering requirement mentioned above.
// TODO(mohsen): If necessary, reorder render passes in compositor frame such
// that the render pass containing mirroring layer appears after the render pass
// created for the mirrored layer.
class CC_EXPORT MirrorLayerImpl : public LayerImpl {
 public:
  static std::unique_ptr<MirrorLayerImpl> Create(LayerTreeImpl* tree_impl,
                                                 int id) {
    return base::WrapUnique(new MirrorLayerImpl(tree_impl, id));
  }

  MirrorLayerImpl(const MirrorLayerImpl&) = delete;
  MirrorLayerImpl& operator=(const MirrorLayerImpl&) = delete;

  ~MirrorLayerImpl() override;

  void SetMirroredLayerId(int id) { mirrored_layer_id_ = id; }
  int mirrored_layer_id() const { return mirrored_layer_id_; }

  // LayerImpl overrides.
  mojom::LayerType GetLayerType() const override;
  std::unique_ptr<LayerImpl> CreateLayerImpl(
      LayerTreeImpl* tree_impl) const override;
  void AppendQuads(viz::CompositorRenderPass* render_pass,
                   AppendQuadsData* append_quads_data) override;
  void PushPropertiesTo(LayerImpl* layer) override;
  gfx::Rect GetDamageRect() const override;
  gfx::Rect GetEnclosingVisibleRectInTargetSpace() const override;

 protected:
  MirrorLayerImpl(LayerTreeImpl* tree_impl, int id);

 private:
  viz::CompositorRenderPassId mirrored_layer_render_pass_id() const {
    return viz::CompositorRenderPassId{
        static_cast<uint64_t>(mirrored_layer_id())};
  }

  int mirrored_layer_id_ = 0;
};

}  // namespace cc

#endif  // CC_LAYERS_MIRROR_LAYER_IMPL_H_
