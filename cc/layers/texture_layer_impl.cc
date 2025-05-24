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
#include "components/viz/client/client_resource_provider.h"
#include "components/viz/common/features.h"
#include "components/viz/common/quads/solid_color_draw_quad.h"
#include "components/viz/common/quads/texture_draw_quad.h"
#include "components/viz/common/resources/platform_color.h"

namespace cc {

TextureLayerImpl::TextureLayerImpl(LayerTreeImpl* tree_impl, int id)
    : LayerImpl(tree_impl, id) {}

TextureLayerImpl::~TextureLayerImpl() {
  FreeTransferableResource();
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
  texture_layer->SetUVTopLeft(uv_top_left_);
  texture_layer->SetUVBottomRight(uv_bottom_right_);
  texture_layer->SetBlendBackgroundColor(blend_background_color_);
  texture_layer->SetForceTextureToOpaque(force_texture_to_opaque_);
  if (needs_set_resource_push_) {
    texture_layer->SetTransferableResource(transferable_resource_,
                                           std::move(release_callback_));
    needs_set_resource_push_ = false;
  }
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

void TextureLayerImpl::AppendQuads(const AppendQuadsContext& context,
                                   viz::CompositorRenderPass* render_pass,
                                   AppendQuadsData* append_quads_data) {
  DCHECK(resource_id_);

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
  const bool needs_blending = !are_contents_opaque;
  if (visible_quad_rect.IsEmpty())
    return;
  const bool nearest_neighbor =
      GetFilterQuality() == PaintFlags::FilterQuality::kNone;

  auto* quad = render_pass->CreateAndAppendDrawQuad<viz::TextureDrawQuad>();
  quad->SetNew(shared_quad_state, quad_rect, visible_quad_rect, needs_blending,
               resource_id_, uv_top_left_, uv_bottom_right_, bg_color,
               nearest_neighbor,
               /*secure_output=*/false, gfx::ProtectedVideoType::kClear);
  quad->dynamic_range_limit = GetDynamicRangeLimit();
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
}

void TextureLayerImpl::ReleaseResources() {
  // Gpu resources are lost when the LayerTreeFrameSink is lost. But software
  // resources are still valid, and we can keep them here in that case.
  if (!transferable_resource_.is_software)
    FreeTransferableResource();
}

gfx::ContentColorUsage TextureLayerImpl::GetContentColorUsage() const {
  if (transferable_resource_.hdr_metadata.extended_range.has_value()) {
    return gfx::ContentColorUsage::kHDR;
  }
  return transferable_resource_.color_space.GetContentColorUsage();
}

void TextureLayerImpl::SetBlendBackgroundColor(bool blend) {
  blend_background_color_ = blend;
}

void TextureLayerImpl::SetForceTextureToOpaque(bool opaque) {
  force_texture_to_opaque_ = opaque;
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
  needs_set_resource_push_ = true;
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
