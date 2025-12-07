// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/slim/texture_layer.h"

#include <utility>

#include "base/functional/bind.h"
#include "base/memory/scoped_refptr.h"
#include "cc/layers/texture_layer_client.h"
#include "cc/slim/layer_tree_impl.h"
#include "components/viz/client/client_resource_provider.h"
#include "components/viz/common/quads/compositor_render_pass.h"
#include "components/viz/common/quads/texture_draw_quad.h"
#include "components/viz/common/resources/release_callback.h"
#include "components/viz/common/resources/resource_id.h"
#include "components/viz/common/resources/transferable_resource.h"

namespace cc::slim {

class TextureLayer::TransferableResourceHolder
    : public base::RefCountedThreadSafe<TransferableResourceHolder> {
 public:
  static scoped_refptr<TransferableResourceHolder> Create(
      const viz::TransferableResource& resource,
      viz::ReleaseCallback release_callback) {
    return new TransferableResourceHolder(resource,
                                          std::move(release_callback));
  }

  TransferableResourceHolder(const TransferableResourceHolder&) = delete;
  TransferableResourceHolder& operator=(const TransferableResourceHolder&) =
      delete;

  // Returns a callback that stores the parameters in order to run the actual
  // callback on destruction.
  viz::ReleaseCallback CreateCallback() {
    return base::BindOnce(&TransferableResourceHolder::DoReleaseCallback,
                          base::WrapRefCounted(this));
  }

  const viz::TransferableResource& resource() const { return resource_; }

 protected:
  virtual ~TransferableResourceHolder() {
    if (release_callback_) {
      std::move(release_callback_).Run(destruction_sync_token_, is_lost_);
    }
  }

 private:
  friend class base::RefCountedThreadSafe<TransferableResourceHolder>;
  explicit TransferableResourceHolder(const viz::TransferableResource& resource,
                                      viz::ReleaseCallback release_callback)
      : resource_(resource),
        release_callback_(std::move(release_callback)),
        destruction_sync_token_(resource.sync_token()) {}

  void DoReleaseCallback(const gpu::SyncToken& sync_token, bool is_lost) {
    destruction_sync_token_ = sync_token;
    is_lost_ = is_lost_ || is_lost;
  }

  const viz::TransferableResource resource_;

  viz::ReleaseCallback release_callback_;

  gpu::SyncToken destruction_sync_token_;
  bool is_lost_ = false;
};

// static
scoped_refptr<TextureLayer> TextureLayer::Create(TextureLayerClient* client) {
  return base::AdoptRef(new TextureLayer(client));
}

TextureLayer::TextureLayer(TextureLayerClient* client) : client_(client) {
  CHECK(client_);
}

TextureLayer::~TextureLayer() {
  OnResourceEvicted();
}

void TextureLayer::NotifyUpdatedResource() {
  // Guarantees that AppendQuads is called for the next frame.
  NotifyPropertyChanged();
}

void TextureLayer::SetLayerTree(LayerTree* layer_tree) {
  if (this->layer_tree() == layer_tree) {
    return;
  }
  OnResourceEvicted();
  Layer::SetLayerTree(layer_tree);
}

void TextureLayer::ReleaseResources() {
  OnResourceEvicted();
  resource_holder_.reset();
}

void TextureLayer::OnResourceEvicted() {
  if (resource_id_) {
    DCHECK(layer_tree());
    auto* resource_provider =
        static_cast<LayerTreeImpl*>(layer_tree())->GetClientResourceProvider();
    // TODO(crbug.com/447533816): Remove this conditional. Right now, during a
    // GPU context loss, the layers aren't informed about lost resources.
    if (resource_provider) {
      resource_provider->RemoveImportedResource(resource_id_);
    }
    resource_id_ = viz::kInvalidResourceId;
  }
}

void TextureLayer::AppendQuads(viz::CompositorRenderPass& render_pass,
                               FrameData& data,
                               const gfx::Transform& transform_to_root,
                               const gfx::Transform& transform_to_target,
                               const gfx::Rect* clip_in_target,
                               const gfx::Rect& visible_rect,
                               float opacity) {
  viz::TransferableResource resource;
  viz::ReleaseCallback release_callback;
  if (client_->PrepareTransferableResource(&resource, &release_callback)) {
    OnResourceEvicted();
    resource_holder_ = TransferableResourceHolder::Create(
        resource, std::move(release_callback));
  }
  if (!resource_holder_) {
    return;
  }
  if (!resource_id_) {
    auto* resource_provider =
        static_cast<LayerTreeImpl*>(layer_tree())->GetClientResourceProvider();
    // base::Unretained is safe because ~TextureLayer will clear the callback
    // (through ClientResourceProvider::RemoveImportedResources).
    auto evicted_callback = base::BindOnce(&TextureLayer::OnResourceEvicted,
                                           base::Unretained(this));
    resource_id_ = resource_provider->ImportResource(
        resource_holder_->resource(), viz::ReleaseCallback(),
        resource_holder_->CreateCallback(), std::move(evicted_callback));
  }

  viz::SharedQuadState* quad_state =
      CreateAndAppendSharedQuadState(render_pass, data, transform_to_target,
                                     clip_in_target, visible_rect, opacity);

  viz::TextureDrawQuad* quad =
      render_pass.CreateAndAppendDrawQuad<viz::TextureDrawQuad>();
  constexpr bool kNearest = false;
  constexpr bool kSecureOutputOnly = false;
  constexpr auto kVideoType = gfx::ProtectedVideoType::kClear;
  const bool needs_blending =
      !contents_opaque() && !background_color().isOpaque();
  quad->SetNew(quad_state, quad_state->quad_layer_rect,
               quad_state->visible_quad_layer_rect, needs_blending,
               resource_id_, gfx::PointF(), gfx::PointF(1.0f, 1.0f),
               background_color(), kNearest, kSecureOutputOnly, kVideoType);
}

}  // namespace cc::slim
