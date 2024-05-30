// Copyright 2011 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/layers/texture_layer_impl.h"

#include <stddef.h>
#include <stdint.h>

#include <memory>
#include <utility>
#include <vector>

#include "base/containers/contains.h"
#include "base/feature_list.h"
#include "base/functional/callback_forward.h"
#include "base/logging.h"
#include "cc/base/features.h"
#include "cc/trees/layer_tree_frame_sink.h"
#include "cc/trees/layer_tree_impl.h"
#include "cc/trees/occlusion.h"
#include "components/viz/common/features.h"
#include "components/viz/common/quads/solid_color_draw_quad.h"
#include "components/viz/common/quads/texture_draw_quad.h"
#include "components/viz/common/resources/bitmap_allocation.h"
#include "components/viz/common/resources/platform_color.h"

namespace cc {

TextureLayerImpl::TextureLayerImpl(LayerTreeImpl* tree_impl, int id)
    : LayerImpl(tree_impl, id) {}

TextureLayerImpl::~TextureLayerImpl() {
  FreeTransferableResource();

  LayerTreeFrameSink* sink = layer_tree_impl()->layer_tree_frame_sink();
  // The LayerTreeFrameSink may be gone, in which case there's no need to
  // unregister anything.
  if (sink) {
    for (const auto& pair : registered_bitmaps_) {
      sink->DidDeleteSharedBitmap(pair.first);
    }
  }
}

mojom::LayerType TextureLayerImpl::GetLayerType() const {
  return mojom::LayerType::kTexture;
}

std::unique_ptr<LayerImpl> TextureLayerImpl::CreateLayerImpl(
    LayerTreeImpl* tree_impl) const {
  return TextureLayerImpl::Create(tree_impl, id());
}

bool TextureLayerImpl::IsSnappedToPixelGridInTarget() {
  // See TextureLayer::IsSnappedToPixelGridInTarget() for explanation of |true|.
  return true;
}

void TextureLayerImpl::PushPropertiesTo(LayerImpl* layer) {
  LayerImpl::PushPropertiesTo(layer);
  TextureLayerImpl* texture_layer = static_cast<TextureLayerImpl*>(layer);
  texture_layer->SetFlipped(flipped_);
  texture_layer->SetUVTopLeft(uv_top_left_);
  texture_layer->SetUVBottomRight(uv_bottom_right_);
  texture_layer->SetPremultipliedAlpha(premultiplied_alpha_);
  texture_layer->SetBlendBackgroundColor(blend_background_color_);
  texture_layer->SetForceTextureToOpaque(force_texture_to_opaque_);
  texture_layer->SetNearestNeighbor(nearest_neighbor_);
  if (own_resource_) {
    texture_layer->SetTransferableResource(transferable_resource_,
                                           std::move(release_callback_));
    own_resource_ = false;
  }
  for (auto& pair : to_register_bitmaps_)
    texture_layer->RegisterSharedBitmapId(pair.first, std::move(pair.second));
  to_register_bitmaps_.clear();
  for (const auto& id : to_unregister_bitmap_ids_)
    texture_layer->UnregisterSharedBitmapId(id);
  to_unregister_bitmap_ids_.clear();
}

bool TextureLayerImpl::WillDraw(
    DrawMode draw_mode,
    viz::ClientResourceProvider* resource_provider) {
  if (draw_mode == DRAW_MODE_RESOURCELESS_SOFTWARE)
    return false;
  // These imply some synchronization problem where the compositor is in gpu
  // compositing but the client thinks it is in software, or vice versa. These
  // should only happen transiently, and should resolve when the client hears
  // about the mode switch.
  if (draw_mode == DRAW_MODE_HARDWARE && transferable_resource_.is_software) {
    DLOG(ERROR) << "Gpu compositor has software resource in TextureLayer";
    return false;
  }
  if (draw_mode == DRAW_MODE_SOFTWARE && !transferable_resource_.is_software) {
    DLOG(ERROR) << "Software compositor has gpu resource in TextureLayer";
    return false;
  }

  if (!LayerImpl::WillDraw(draw_mode, resource_provider))
    return false;

  if (own_resource_) {
    DCHECK(!resource_id_);
    if (!transferable_resource_.is_empty()) {
      // Currently only Canvas supports releases resources in response to
      // eviction. Other sources will be add once they can support this. Some
      // complexity arises here from WebGL/WebGPU textures, as they do not
      // necessarily maintain the data needed to rebuild the resources.
      base::OnceClosure evicted_cb = viz::ResourceEvictedCallback();
      if (base::FeatureList::IsEnabled(features::kEvictionUnlocksResources) &&
          transferable_resource_.resource_source ==
              viz::TransferableResource::ResourceSource::kCanvas) {
        evicted_cb = base::BindOnce(&TextureLayerImpl::OnResourceEvicted,
                                    base::Unretained(this));
        DCHECK(MayEvictResourceInBackground(
            transferable_resource_.resource_source));
      }
      resource_id_ = resource_provider->ImportResource(
          transferable_resource_,
          /* impl_thread_release_callback= */ viz::ReleaseCallback(),
          /* main_thread_release_callback= */ std::move(release_callback_),
          /* evicted_callback= */ std::move(evicted_cb));
      DCHECK(resource_id_);
    }
    own_resource_ = false;
  }

  return resource_id_ != viz::kInvalidResourceId;
}

void TextureLayerImpl::AppendQuads(viz::CompositorRenderPass* render_pass,
                                   AppendQuadsData* append_quads_data) {
  DCHECK(resource_id_);

  LayerTreeFrameSink* sink = layer_tree_impl()->layer_tree_frame_sink();
  for (const auto& pair : to_register_bitmaps_) {
    sink->DidAllocateSharedBitmap(pair.second->shared_region().Duplicate(),
                                  pair.first);
  }
  // All |to_register_bitmaps_| have been registered above, so we can move them
  // all to the |registered_bitmaps_|.
  registered_bitmaps_.insert(
      std::make_move_iterator(to_register_bitmaps_.begin()),
      std::make_move_iterator(to_register_bitmaps_.end()));
  to_register_bitmaps_.clear();

  SkColor4f bg_color =
      blend_background_color_ ? background_color() : SkColors::kTransparent;

  if (force_texture_to_opaque_) {
    bg_color = SkColors::kBlack;
  }

  bool are_contents_opaque = contents_opaque() || bg_color.isOpaque();

  viz::SharedQuadState* shared_quad_state =
      render_pass->CreateAndAppendSharedQuadState();
  PopulateSharedQuadState(shared_quad_state, are_contents_opaque);

  AppendDebugBorderQuad(render_pass, gfx::Rect(bounds()), shared_quad_state,
                        append_quads_data);

  gfx::Rect quad_rect(bounds());
  gfx::Rect visible_quad_rect =
      draw_properties().occlusion_in_content_space.GetUnoccludedContentRect(
          quad_rect);
  bool needs_blending = !are_contents_opaque;
  if (visible_quad_rect.IsEmpty())
    return;

  auto* quad = render_pass->CreateAndAppendDrawQuad<viz::TextureDrawQuad>();
  quad->SetNew(shared_quad_state, quad_rect, visible_quad_rect, needs_blending,
               resource_id_, premultiplied_alpha_, uv_top_left_,
               uv_bottom_right_, bg_color, flipped_, nearest_neighbor_,
               /*secure_output=*/false, gfx::ProtectedVideoType::kClear);
  quad->set_resource_size_in_pixels(transferable_resource_.size);
  ValidateQuadResources(quad);
}

SimpleEnclosedRegion TextureLayerImpl::VisibleOpaqueRegion() const {
  if (transferable_resource_.is_empty()) {
    return SimpleEnclosedRegion();
  }

  if (contents_opaque())
    return SimpleEnclosedRegion(visible_layer_rect());

  if (force_texture_to_opaque_)
    return SimpleEnclosedRegion(visible_layer_rect());

  if (blend_background_color_ && background_color().isOpaque())
    return SimpleEnclosedRegion(visible_layer_rect());

  return SimpleEnclosedRegion();
}

void TextureLayerImpl::OnPurgeMemory() {
  // Do nothing here intentionally as the LayerTreeFrameSink isn't lost.
  // Unregistering SharedBitmapIds with the LayerTreeFrameSink wouldn't free
  // the shared memory, as the TextureLayer and/or TextureLayerClient will still
  // have a reference to it.
}

void TextureLayerImpl::ReleaseResources() {
  // Gpu resources are lost when the LayerTreeFrameSink is lost. But software
  // resources are still valid, and we can keep them here in that case.
  if (!transferable_resource_.is_software)
    FreeTransferableResource();

  // The LayerTreeFrameSink is gone and being replaced, so we will have to
  // re-register all SharedBitmapIds on the new LayerTreeFrameSink. We don't
  // need to do that until the SharedBitmapIds will be used, in AppendQuads(),
  // but we mark them all as to be registered here.
  to_register_bitmaps_.insert(
      std::make_move_iterator(registered_bitmaps_.begin()),
      std::make_move_iterator(registered_bitmaps_.end()));
  registered_bitmaps_.clear();
  // The |to_unregister_bitmap_ids_| are kept since the active layer will re-
  // register its SharedBitmapIds with a new LayerTreeFrameSink in the future,
  // so we must remember that we want to unregister it (or avoid registering at
  // all) instead.
}

gfx::ContentColorUsage TextureLayerImpl::GetContentColorUsage() const {
  if (transferable_resource_.hdr_metadata.extended_range.has_value()) {
    return gfx::ContentColorUsage::kHDR;
  }
  return transferable_resource_.color_space.GetContentColorUsage();
}

void TextureLayerImpl::SetPremultipliedAlpha(bool premultiplied_alpha) {
  premultiplied_alpha_ = premultiplied_alpha;
}

void TextureLayerImpl::SetBlendBackgroundColor(bool blend) {
  blend_background_color_ = blend;
}

void TextureLayerImpl::SetForceTextureToOpaque(bool opaque) {
  force_texture_to_opaque_ = opaque;
}

void TextureLayerImpl::SetFlipped(bool flipped) {
  flipped_ = flipped;
}

void TextureLayerImpl::SetNearestNeighbor(bool nearest_neighbor) {
  nearest_neighbor_ = nearest_neighbor;
}

void TextureLayerImpl::SetUVTopLeft(const gfx::PointF& top_left) {
  uv_top_left_ = top_left;
}

void TextureLayerImpl::SetUVBottomRight(const gfx::PointF& bottom_right) {
  uv_bottom_right_ = bottom_right;
}

void TextureLayerImpl::SetTransferableResource(
    const viz::TransferableResource& resource,
    viz::ReleaseCallback release_callback) {
  DCHECK_EQ(resource.is_empty(), !release_callback);
  FreeTransferableResource();
  transferable_resource_ = resource;
  release_callback_ = std::move(release_callback);
  own_resource_ = true;
}

void TextureLayerImpl::RegisterSharedBitmapId(
    viz::SharedBitmapId id,
    scoped_refptr<CrossThreadSharedBitmap> bitmap) {
  // If a TextureLayer leaves and rejoins a tree without the TextureLayerImpl
  // being destroyed, then it will re-request registration of ids that are still
  // registered on the impl side, so we can just ignore these requests.
  if (!base::Contains(registered_bitmaps_, id)) {
    // If this is a pending layer, these will be moved to the active layer
    // when we PushPropertiesTo(). Otherwise, we don't need to notify these to
    // the LayerTreeFrameSink until we're going to use them, so defer it until
    // AppendQuads().
    to_register_bitmaps_[id] = std::move(bitmap);
  }
  std::erase(to_unregister_bitmap_ids_, id);
}

void TextureLayerImpl::UnregisterSharedBitmapId(viz::SharedBitmapId id) {
  if (IsActive()) {
    LayerTreeFrameSink* sink = layer_tree_impl()->layer_tree_frame_sink();
    if (sink && base::Contains(registered_bitmaps_, id)) {
      sink->DidDeleteSharedBitmap(id);
    }
    to_register_bitmaps_.erase(id);
    registered_bitmaps_.erase(id);
  } else {
    // The active layer will unregister. We do this because it may be using the
    // SharedBitmapId, so we should remove the SharedBitmapId only after we've
    // had a chance to replace it with activation.
    to_unregister_bitmap_ids_.push_back(id);
  }
}

void TextureLayerImpl::FreeTransferableResource() {
  if (own_resource_) {
    DCHECK(!resource_id_);
    if (release_callback_) {
      // We didn't use the resource, but the client might need the SyncToken
      // before it can use the resource with its own GL context.
      std::move(release_callback_)
          .Run(transferable_resource_.sync_token(), false);
    }
    transferable_resource_ = viz::TransferableResource();
  } else if (resource_id_) {
    DCHECK(!own_resource_);
    auto* resource_provider = layer_tree_impl()->resource_provider();
    resource_provider->RemoveImportedResource(resource_id_);
    resource_id_ = viz::kInvalidResourceId;
  }
}

void TextureLayerImpl::OnResourceEvicted() {
  // Once we are evicted we want to remove it to unlock the memory. This will
  // allow it to be released upon the next return. We also clear out the
  // `resource_id_` so we don't attempt to delete it a second time when any
  // future resource is pushed and ready to be imported.
  if (resource_id_) {
    auto* resource_provider = layer_tree_impl()->resource_provider();
    resource_provider->RemoveImportedResource(resource_id_);
  }
  resource_id_ = viz::kInvalidResourceId;
}

void TextureLayerImpl::SetInInvisibleLayerTree() {
  // With canvas hibernation, main will release the resource, which will be
  // recreated once visibility changes. Don't hold onto it.
  //
  // Only do it when the resource has not been imported, meaning that it's not
  // visible.
  //
  // In this case, main is responsible for giving us the transferable resource
  // (and making sure that the layer properties are pushed) next time the tree
  // becomes visible. See Canvas2DLayerBridge::PageVisibilityChanged().
  if (base::FeatureList::IsEnabled(
          features::kClearCanvasResourcesInBackground) &&
      transferable_resource_.resource_source ==
          viz::TransferableResource::ResourceSource::kCanvas &&
      own_resource_) {
    DCHECK(
        MayEvictResourceInBackground(transferable_resource_.resource_source));
    if (!transferable_resource_.is_software) {
      FreeTransferableResource();
    }
  }
}

// static
bool TextureLayerImpl::MayEvictResourceInBackground(
    viz::TransferableResource::ResourceSource source) {
  return source == viz::TransferableResource::ResourceSource::kCanvas &&
         (base::FeatureList::IsEnabled(
              features::kClearCanvasResourcesInBackground) ||
          base::FeatureList::IsEnabled(features::kEvictionUnlocksResources));
}

}  // namespace cc
