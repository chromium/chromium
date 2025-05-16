// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CC_LAYERS_TEXTURE_LAYER_IMPL_H_
#define CC_LAYERS_TEXTURE_LAYER_IMPL_H_

#include <memory>
#include <optional>
#include <string>
#include <vector>

#include "base/containers/flat_map.h"
#include "base/functional/callback.h"
#include "base/memory/ptr_util.h"
#include "cc/cc_export.h"
#include "cc/layers/layer_impl.h"
#include "components/viz/common/resources/release_callback.h"
#include "components/viz/common/resources/transferable_resource.h"
#include "ui/gfx/hdr_metadata.h"

namespace cc {

class CC_EXPORT TextureLayerImpl : public LayerImpl {
 public:
  static std::unique_ptr<TextureLayerImpl> Create(LayerTreeImpl* tree_impl,
                                                  int id) {
    return base::WrapUnique(new TextureLayerImpl(tree_impl, id));
  }
  TextureLayerImpl(const TextureLayerImpl&) = delete;
  ~TextureLayerImpl() override;

  TextureLayerImpl& operator=(const TextureLayerImpl&) = delete;

  mojom::LayerType GetLayerType() const override;
  std::unique_ptr<LayerImpl> CreateLayerImpl(
      LayerTreeImpl* layer_tree_impl) const override;
  bool IsSnappedToPixelGridInTarget() override;
  void PushPropertiesTo(LayerImpl* layer) override;

  bool WillDraw(DrawMode draw_mode,
                viz::ClientResourceProvider* resource_provider) override;
  void AppendQuads(const AppendQuadsContext& context,
                   viz::CompositorRenderPass* render_pass,
                   AppendQuadsData* append_quads_data) override;
  SimpleEnclosedRegion VisibleOpaqueRegion() const override;
  void ReleaseResources() override;
  void OnPurgeMemory() override;
  gfx::ContentColorUsage GetContentColorUsage() const override;

  // These setter methods don't cause any implicit damage, so the texture client
  // must explicitly invalidate if they intend to cause a visible change in the
  // layer's output.
  void SetTextureId(unsigned id);
  void SetBlendBackgroundColor(bool blend);
  void SetForceTextureToOpaque(bool opaque);
  void SetUVTopLeft(const gfx::PointF& top_left);
  void SetUVBottomRight(const gfx::PointF& bottom_right);
  void SetHdrMetadata(const gfx::HDRMetadata& hdr_metadata);

  void SetTransferableResource(const viz::TransferableResource& resource,
                               viz::ReleaseCallback release_callback);
  bool NeedSetTransferableResource() const;

  void SetInInvisibleLayerTree() override;
  // Whether the resource may be evicted in background. If it returns true, main
  // is responsible for making sure that the resource is imported again after a
  // visibility change.
  static bool MayEvictResourceInBackground(
      viz::TransferableResource::ResourceSource source);

  bool blend_background_color() const { return blend_background_color_; }
  bool force_texture_to_opaque() const { return force_texture_to_opaque_; }
  bool needs_set_resource_push() const { return needs_set_resource_push_; }
  void ClearNeedsSetResourcePush() { needs_set_resource_push_ = false; }

  gfx::PointF uv_top_left() const { return uv_top_left_; }
  gfx::PointF uv_bottom_right() const { return uv_bottom_right_; }
  const viz::TransferableResource& transferable_resource() const {
    return transferable_resource_;
  }
  viz::ResourceId resource_id() const { return resource_id_; }

 private:
  TextureLayerImpl(LayerTreeImpl* tree_impl, int id);

  void FreeTransferableResource();
  void OnResourceEvicted();

  bool blend_background_color_ = false;
  bool force_texture_to_opaque_ = false;

  // True while the |transferable_resource_| is owned by this layer, and
  // becomes false once it is passed to another layer or to the
  // viz::ClientResourceProvider, at which point we get back a |resource_id_|.
  bool own_resource_ = false;

  // True when a resource change should be pushed to the next tree.
  bool needs_set_resource_push_ = false;

  gfx::PointF uv_top_left_ = gfx::PointF();
  gfx::PointF uv_bottom_right_ = gfx::PointF(1.f, 1.f);

  // A TransferableResource from the layer's client that will be given
  // to the display compositor.
  viz::TransferableResource transferable_resource_;
  // Local ResourceId for the TransferableResource, to be used with the
  // compositor's viz::ClientResourceProvider in order to refer to the
  // TransferableResource given to it.
  viz::ResourceId resource_id_ = viz::kInvalidResourceId;
  viz::ReleaseCallback release_callback_;
};

}  // namespace cc

#endif  // CC_LAYERS_TEXTURE_LAYER_IMPL_H_
