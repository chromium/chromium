// Copyright 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/layers/texture_layer_impl.h"

#include <stddef.h>
#include <stdint.h>

#include <vector>

#include "base/strings/stringprintf.h"
#include "cc/trees/layer_tree_frame_sink.h"
#include "cc/trees/layer_tree_impl.h"
#include "cc/trees/occlusion.h"
#include "components/viz/common/quads/solid_color_draw_quad.h"
#include "components/viz/common/quads/texture_draw_quad.h"
#include "components/viz/common/resources/bitmap_allocation.h"
#include "components/viz/common/resources/platform_color.h"
#include "components/viz/common/resources/single_release_callback.h"

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

std::unique_ptr<LayerImpl> TextureLayerImpl::CreateLayerImpl(
    LayerTreeImpl* tree_impl) {
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
  texture_layer->SetVertexOpacity(vertex_opacity_);
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
    if (!transferable_resource_.mailbox_holder.mailbox.IsZero()) {
      resource_id_ = resource_provider->ImportResource(
          transferable_resource_, std::move(release_callback_));
      DCHECK(resource_id_);
    }
    own_resource_ = false;
  }

  return resource_id_;
}

void TextureLayerImpl::AppendQuads(viz::RenderPass* render_pass,
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

  SkColor bg_color =
      blend_background_color_ ? background_color() : SK_ColorTRANSPARENT;

  if (force_texture_to_opaque_) {
    bg_color = SK_ColorBLACK;
  }

  bool are_contents_opaque =
      contents_opaque() || (SkColorGetA(bg_color) == 0xFF);

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

  if (!vertex_opacity_[0] && !vertex_opacity_[1] && !vertex_opacity_[2] &&
      !vertex_opacity_[3])
    return;

  auto* quad = render_pass->CreateAndAppendDrawQuad<viz::TextureDrawQuad>();
  quad->SetNew(shared_quad_state, quad_rect, visible_quad_rect, needs_blending,
               resource_id_, premultiplied_alpha_, uv_top_left_,
               uv_bottom_right_, bg_color, vertex_opacity_, flipped_,
               nearest_neighbor_, /*secure_output_only=*/false,
               gfx::ProtectedVideoType::kClear);
  quad->set_resource_size_in_pixels(transferable_resource_.size);
  ValidateQuadResources(quad);
}

SimpleEnclosedRegion TextureLayerImpl::VisibleOpaqueRegion() const {
  if (contents_opaque())
    return SimpleEnclosedRegion(visible_layer_rect());

  if (force_texture_to_opaque_)
    return SimpleEnclosedRegion(visible_layer_rect());

  if (blend_background_color_ && (SkColorGetA(background_color()) == 0xFF))
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

// 1--2
// |  |
// 0--3
void TextureLayerImpl::SetVertexOpacity(const float vertex_opacity[4]) {
  vertex_opacity_[0] = vertex_opacity[0];
  vertex_opacity_[1] = vertex_opacity[1];
  vertex_opacity_[2] = vertex_opacity[2];
  vertex_opacity_[3] = vertex_opacity[3];
}

void TextureLayerImpl::SetTransferableResource(
    const viz::TransferableResource& resource,
    std::unique_ptr<viz::SingleReleaseCallback> release_callback) {
  DCHECK_EQ(resource.mailbox_holder.mailbox.IsZero(), !release_callback);
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
  if (registered_bitmaps_.find(id) == registered_bitmaps_.end()) {
    // If this is a pending layer, these will be moved to the active layer when
    // we PushPropertiesTo(). Otherwise, we don't need to notify these to the
    // LayerTreeFrameSink until we're going to use them, so defer it until
    // AppendQuads().
    to_register_bitmaps_[id] = std::move(bitmap);
  }
  base::Erase(to_unregister_bitmap_ids_, id);
}

void TextureLayerImpl::UnregisterSharedBitmapId(viz::SharedBitmapId id) {
  if (IsActive()) {
    LayerTreeFrameSink* sink = layer_tree_impl()->layer_tree_frame_sink();
    if (sink && registered_bitmaps_.find(id) != registered_bitmaps_.end())
      sink->DidDeleteSharedBitmap(id);
    to_register_bitmaps_.erase(id);
    registered_bitmaps_.erase(id);
  } else {
    // The active layer will unregister. We do this because it may be using the
    // SharedBitmapId, so we should remove the SharedBitmapId only after we've
    // had a chance to replace it with activation.
    to_unregister_bitmap_ids_.push_back(id);
  }
}

const char* TextureLayerImpl::LayerTypeAsString() const {
  return "cc::TextureLayerImpl";
}

void TextureLayerImpl::FreeTransferableResource() {
  if (own_resource_) {
    DCHECK(!resource_id_);
    if (release_callback_) {
      // We didn't use the resource, but the client might need the SyncToken
      // before it can use the resource with its own GL context.
      release_callback_->Run(transferable_resource_.mailbox_holder.sync_token,
                             false);
    }
    transferable_resource_ = viz::TransferableResource();
    release_callback_ = nullptr;
  } else if (resource_id_) {
    DCHECK(!own_resource_);
    auto* resource_provider = layer_tree_impl()->resource_provider();
    resource_provider->RemoveImportedResource(resource_id_);
    resource_id_ = 0;
  }
}

}  // namespace cc
