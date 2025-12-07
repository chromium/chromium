// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CC_SLIM_TEXTURE_LAYER_H_
#define CC_SLIM_TEXTURE_LAYER_H_

#include "base/component_export.h"
#include "base/memory/scoped_refptr.h"
#include "cc/slim/layer.h"
#include "components/viz/common/resources/resource_id.h"

namespace cc {

class TextureLayerClient;

namespace slim {

// A layer that renders a texture provided by a `TextureLayerClient`.
//
// This layer operates on a pull model. When drawing the layer tree, it asks the
// client to provide a texture to display. If the client returns `true`, it
// provides a new `viz::TransferableResource` which the layer will then display.
// The layer will hold a reference to the resource until the client provides a
// new one, or the layer is destroyed. If the client returns `false`, the layer
// continues to display the resource from the previous frame.
//
// A client must notify the TextureLayer that there is a new resource available
// by calling NotifyUpdatedResource().
class COMPONENT_EXPORT(CC_SLIM) TextureLayer : public Layer {
 public:
  static scoped_refptr<TextureLayer> Create(TextureLayerClient* client);

  // The client must call this method when there is a new resource available for
  // the layer to pull.
  void NotifyUpdatedResource();

  // Layer overrides.
  void SetLayerTree(LayerTree* layer_tree) override;

 protected:
  // Layer overrides.
  void ReleaseResources() override;

 private:
  class TransferableResourceHolder;

  explicit TextureLayer(TextureLayerClient* client);
  ~TextureLayer() override;

  void OnResourceEvicted();

  // Layer overrides.
  void AppendQuads(viz::CompositorRenderPass& render_pass,
                   FrameData& data,
                   const gfx::Transform& transform_to_root,
                   const gfx::Transform& transform_to_target,
                   const gfx::Rect* clip_in_target,
                   const gfx::Rect& visible_rect,
                   float opacity) override;

  const raw_ptr<TextureLayerClient> client_;
  viz::ResourceId resource_id_;
  scoped_refptr<TransferableResourceHolder> resource_holder_;
};

}  // namespace slim

}  // namespace cc

#endif  // CC_SLIM_TEXTURE_LAYER_H_
