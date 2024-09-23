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
#include "cc/resources/cross_thread_shared_bitmap.h"
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
  void AppendQuads(viz::CompositorRenderPass* render_pass,
                   AppendQuadsData* append_quads_data) override;
  SimpleEnclosedRegion VisibleOpaqueRegion() const override;
  void ReleaseResources() override;
  void OnPurgeMemory() override;
  gfx::ContentColorUsage GetContentColorUsage() const override;

  // These setter methods don't cause any implicit damage, so the texture client
  // must explicitly invalidate if they intend to cause a visible change in the
  // layer's output.
  void SetTextureId(unsigned id);
  void SetPremultipliedAlpha(bool premultiplied_alpha);
  void SetBlendBackgroundColor(bool blend);
  void SetForceTextureToOpaque(bool opaque);
  void SetFlipped(bool flipped);
  void SetNearestNeighbor(bool nearest_neighbor);
  void SetUVTopLeft(const gfx::PointF& top_left);
  void SetUVBottomRight(const gfx::PointF& bottom_right);
  void SetHdrMetadata(const gfx::HDRMetadata& hdr_metadata);

  void SetTransferableResource(const viz::TransferableResource& resource,
                               viz::ReleaseCallback release_callback);
  bool NeedSetTransferableResource() const;

  // These methods notify the display compositor, through the
  // CompositorFrameSink, of the existence of a SharedBitmapId and its
  // mapping to a SharedMemory in |bitmap|. Then this SharedBitmapId can be used
  // in TransferableResources inserted on the layer while it is registered. If
  // the layer is destroyed, the SharedBitmapId will be unregistered
  // automatically, and if the CompositorFrameSink is replaced, it will be
  // re-registered on the new one. The SharedMemory must be kept alive while it
  // is registered.
  // If this is a pending layer, the registration is deferred to the active
  // layer.
  void RegisterSharedBitmapId(viz::SharedBitmapId id,
                              scoped_refptr<CrossThreadSharedBitmap> bitmap);
  void UnregisterSharedBitmapId(viz::SharedBitmapId id);
  void SetInInvisibleLayerTree() override;
  // Whether the resource may be evicted in background. If it returns true, main
  // is responsible for making sure that the resource is imported again after a
  // visibility change.
  static bool MayEvictResourceInBackground(
      viz::TransferableResource::ResourceSource source);

 private:
  TextureLayerImpl(LayerTreeImpl* tree_impl, int id);

  void FreeTransferableResource();
  void OnResourceEvicted();

  bool premultiplied_alpha_ = true;
  bool blend_background_color_ = false;
  bool force_texture_to_opaque_ = false;
  bool flipped_ = true;
  bool nearest_neighbor_ = false;
  gfx::PointF uv_top_left_ = gfx::PointF();
  gfx::PointF uv_bottom_right_ = gfx::PointF(1.f, 1.f);

  // True while the |transferable_resource_| is owned by this layer, and
  // becomes false once it is passed to another layer or to the
  // viz::ClientResourceProvider, at which point we get back a |resource_id_|.
  bool own_resource_ = false;
  // A TransferableResource from the layer's client that will be given
  // to the display compositor.
  viz::TransferableResource transferable_resource_;
  // Local ResourceId for the TransferableResource, to be used with the
  // compositor's viz::ClientResourceProvider in order to refer to the
  // TransferableResource given to it.
  viz::ResourceId resource_id_ = viz::kInvalidResourceId;
  viz::ReleaseCallback release_callback_;

  // As a pending layer, the set of SharedBitmapIds and the underlying
  // base::SharedMemory that must be notified to the display compositor through
  // the LayerTreeFrameSink. These will be passed to the active layer. As an
  // active layer, the set of SharedBitmapIds that need to be registered but
  // have not been yet, since it is done lazily.
  base::flat_map<viz::SharedBitmapId, scoped_refptr<CrossThreadSharedBitmap>>
      to_register_bitmaps_;

  // For active layers only. The set of SharedBitmapIds and ownership of the
  // underlying base::SharedMemory that have been notified to the display
  // compositor through the LayerTreeFrameSink. These will need to be
  // re-registered if the LayerTreeFrameSink changes (ie ReleaseResources()
  // occurs).
  base::flat_map<viz::SharedBitmapId, scoped_refptr<CrossThreadSharedBitmap>>
      registered_bitmaps_;

  // As a pending layer, the set of SharedBitmapIds that the active layer should
  // unregister.
  std::vector<viz::SharedBitmapId> to_unregister_bitmap_ids_;
};

}  // namespace cc

#endif  // CC_LAYERS_TEXTURE_LAYER_IMPL_H_
