// Copyright 2010 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/layers/texture_layer.h"

#include <memory>
#include <utility>
#include <vector>

#include "base/containers/contains.h"
#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "base/location.h"
#include "base/synchronization/lock.h"
#include "base/task/sequenced_task_runner.h"
#include "base/trace_event/trace_event.h"
#include "cc/base/features.h"
#include "cc/base/simple_enclosed_region.h"
#include "cc/layers/texture_layer_client.h"
#include "cc/layers/texture_layer_impl.h"
#include "cc/trees/layer_tree_impl.h"

namespace cc {

scoped_refptr<TextureLayer> TextureLayer::CreateForMailbox(
    TextureLayerClient* client) {
  return scoped_refptr<TextureLayer>(new TextureLayer(client));
}

TextureLayer::TextureLayer(TextureLayerClient* client)
    : client_(client),
      flipped_(true),
      nearest_neighbor_(false),
      uv_bottom_right_(1.f, 1.f),
      premultiplied_alpha_(true),
      blend_background_color_(false),
      force_texture_to_opaque_(false),
      needs_set_resource_(false) {}

TextureLayer::~TextureLayer() = default;

void TextureLayer::ClearClient() {
  client_.Write(*this) = nullptr;
  ClearTexture();
  UpdateDrawsContent();
}

void TextureLayer::ClearTexture() {
  SetTransferableResource(viz::TransferableResource(), viz::ReleaseCallback());
}

std::unique_ptr<LayerImpl> TextureLayer::CreateLayerImpl(
    LayerTreeImpl* tree_impl) const {
  return TextureLayerImpl::Create(tree_impl, id());
}

void TextureLayer::SetFlipped(bool flipped) {
  if (flipped_.Read(*this) == flipped)
    return;
  flipped_.Write(*this) = flipped;
  SetNeedsCommit();
}

void TextureLayer::SetNearestNeighbor(bool nearest_neighbor) {
  if (nearest_neighbor_.Read(*this) == nearest_neighbor)
    return;
  nearest_neighbor_.Write(*this) = nearest_neighbor;
  SetNeedsCommit();
}

void TextureLayer::SetUV(const gfx::PointF& top_left,
                         const gfx::PointF& bottom_right) {
  if (uv_top_left_.Read(*this) == top_left &&
      uv_bottom_right_.Read(*this) == bottom_right)
    return;
  uv_top_left_.Write(*this) = top_left;
  uv_bottom_right_.Write(*this) = bottom_right;
  SetNeedsCommit();
}

void TextureLayer::SetPremultipliedAlpha(bool premultiplied_alpha) {
  if (premultiplied_alpha_.Read(*this) == premultiplied_alpha)
    return;
  premultiplied_alpha_.Write(*this) = premultiplied_alpha;
  SetNeedsCommit();
}

void TextureLayer::SetBlendBackgroundColor(bool blend) {
  if (blend_background_color_.Read(*this) == blend)
    return;
  blend_background_color_.Write(*this) = blend;
  SetNeedsCommit();
}

void TextureLayer::SetForceTextureToOpaque(bool opaque) {
  if (force_texture_to_opaque_.Read(*this) == opaque)
    return;
  force_texture_to_opaque_.Write(*this) = opaque;
  SetNeedsCommit();
}

void TextureLayer::SetTransferableResourceInternal(
    const viz::TransferableResource& resource,
    viz::ReleaseCallback release_callback,
    bool requires_commit) {
  DCHECK(resource.is_empty() || !resource_holder_.Read(*this) ||
         resource != resource_holder_.Read(*this)->resource());
  DCHECK_EQ(resource.is_empty(), !release_callback);

  // If we never committed the resource, we need to release it here.
  if (!resource.is_empty()) {
    resource_holder_.Write(*this) = TransferableResourceHolder::Create(
        resource, std::move(release_callback));
  } else {
    resource_holder_.Write(*this) = nullptr;
  }
  needs_set_resource_.Write(*this) = true;
  // If we are within a commit, no need to do it again immediately after.
  if (requires_commit)
    SetNeedsCommit();
  else
    SetNeedsPushProperties();

  UpdateDrawsContent();
}

void TextureLayer::SetTransferableResource(
    const viz::TransferableResource& resource,
    viz::ReleaseCallback release_callback) {
  bool requires_commit = true;
  SetTransferableResourceInternal(resource, std::move(release_callback),
                                  requires_commit);
}

void TextureLayer::SetNeedsSetTransferableResource() {
  needs_set_resource_.Write(*this) = true;
  SetNeedsPushProperties();
}

void TextureLayer::SetLayerTreeHost(LayerTreeHost* host) {
  if (layer_tree_host() == host) {
    Layer::SetLayerTreeHost(host);
    return;
  }

  // If we're removed from the tree, the TextureLayerImpl will be destroyed, and
  // we will need to set the mailbox again on a new TextureLayerImpl the next
  // time we push.
  if (!host && resource_holder_.Read(*this))
    needs_set_resource_.Write(*this) = true;
  if (host) {
    // When attached to a new LayerTreeHost, all previously registered
    // SharedBitmapIds will need to be re-sent to the new TextureLayerImpl
    // representing this layer on the compositor thread.
    auto& registered_bitmaps = registered_bitmaps_.Write(*this);
    to_register_bitmaps_.Write(*this).insert(
        std::make_move_iterator(registered_bitmaps.begin()),
        std::make_move_iterator(registered_bitmaps.end()));
    registered_bitmaps.clear();
  }
  Layer::SetLayerTreeHost(host);
}

bool TextureLayer::HasDrawableContent() const {
  return (client_.Read(*this) || resource_holder_.Read(*this)) &&
         Layer::HasDrawableContent();
}

bool TextureLayer::RequiresSetNeedsDisplayOnHdrHeadroomChange() const {
  if (!resource_holder_.Read(*this)) {
    return false;
  }

  // If the HDR headroom is changed, then tonemapped resources will need to
  // re-draw.
  const auto& resource = resource_holder_.Read(*this)->resource();
  if (resource.color_space.IsToneMappedByDefault()) {
    return true;
  }

  // Extended range content also needs to be re-composited to limit itself to
  // the new headroom.
  if (resource.hdr_metadata.extended_range.has_value()) {
    return true;
  }

  return false;
}

bool TextureLayer::Update() {
  bool updated = Layer::Update();
  if (client_.Read(*this)) {
    viz::TransferableResource resource;
    viz::ReleaseCallback release_callback;
    if (client_.Write(*this)->PrepareTransferableResource(this, &resource,
                                                          &release_callback)) {
      // Already within a commit, no need to do another one immediately.
      bool requires_commit = false;
      SetTransferableResourceInternal(resource, std::move(release_callback),
                                      requires_commit);
      updated = true;
    }
  }

  // SetTransferableResource could be called externally and the same mailbox
  // used for different textures.  Such callers notify this layer that the
  // texture has changed by calling SetNeedsDisplay, so check for that here.
  return updated || !update_rect().IsEmpty();
}

bool TextureLayer::IsSnappedToPixelGridInTarget() const {
  // Often layers are positioned with CSS to "50%", which can often leave them
  // with a fractional (N + 0.5) pixel position. This would leave them looking
  // fuzzy, so we request that TextureLayers are snapped to the pixel grid,
  // since their content is generated externally and we can not adjust for it
  // inside the content (unlike for PictureLayers).
  return true;
}

void TextureLayer::PushPropertiesTo(
    LayerImpl* layer,
    const CommitState& commit_state,
    const ThreadUnsafeCommitState& unsafe_state) {
  Layer::PushPropertiesTo(layer, commit_state, unsafe_state);
  TRACE_EVENT0("cc", "TextureLayer::PushPropertiesTo");

  TextureLayerImpl* texture_layer = static_cast<TextureLayerImpl*>(layer);
  texture_layer->SetFlipped(flipped_.Read(*this));
  texture_layer->SetNearestNeighbor(nearest_neighbor_.Read(*this));
  texture_layer->SetUVTopLeft(uv_top_left_.Read(*this));
  texture_layer->SetUVBottomRight(uv_bottom_right_.Read(*this));
  texture_layer->SetPremultipliedAlpha(premultiplied_alpha_.Read(*this));
  texture_layer->SetBlendBackgroundColor(blend_background_color_.Read(*this));
  texture_layer->SetForceTextureToOpaque(force_texture_to_opaque_.Read(*this));
  if (needs_set_resource_.Read(*this)) {
    viz::TransferableResource resource;
    viz::ReleaseCallback release_callback;
    if (auto& resource_holder = resource_holder_.Write(*this)) {
      resource = resource_holder->resource();
      release_callback =
          base::BindOnce(&TransferableResourceHolder::Return, resource_holder,
                         base::RetainedRef(layer->layer_tree_impl()
                                               ->task_runner_provider()
                                               ->MainThreadTaskRunner()));
    }
    texture_layer->SetTransferableResource(resource,
                                           std::move(release_callback));
    needs_set_resource_.Write(*this) = false;
  }
  auto& to_register_bitmaps = to_register_bitmaps_.Write(*this);
  for (auto& pair : to_register_bitmaps)
    texture_layer->RegisterSharedBitmapId(pair.first, pair.second);
  // Store the registered SharedBitmapIds in case we get a new TextureLayerImpl,
  // in a new tree, to re-send them to.
  registered_bitmaps_.Write(*this).insert(
      std::make_move_iterator(to_register_bitmaps.begin()),
      std::make_move_iterator(to_register_bitmaps.end()));
  to_register_bitmaps.clear();
  auto& to_unregister_bitmap_ids = to_unregister_bitmap_ids_.Write(*this);
  for (const auto& id : to_unregister_bitmap_ids)
    texture_layer->UnregisterSharedBitmapId(id);
  to_unregister_bitmap_ids.clear();
}

SharedBitmapIdRegistration TextureLayer::RegisterSharedBitmapId(
    const viz::SharedBitmapId& id,
    scoped_refptr<CrossThreadSharedBitmap> bitmap) {
  DCHECK(!base::Contains(to_register_bitmaps_.Read(*this), id));
  DCHECK(!base::Contains(registered_bitmaps_.Read(*this), id));
  to_register_bitmaps_.Write(*this)[id] = std::move(bitmap);
  std::erase(to_unregister_bitmap_ids_.Write(*this), id);

  // This does not SetNeedsCommit() to be as lazy as possible.
  // Notifying a SharedBitmapId is not needed until it is used,
  // and using it will require a commit, so we can wait for that commit
  // before forwarding the notification instead of forcing it to happen
  // as a side effect of this method.
  SetNeedsPushProperties();
  return SharedBitmapIdRegistration(weak_ptr_factory_.GetMutableWeakPtr(), id);
}

void TextureLayer::UnregisterSharedBitmapId(viz::SharedBitmapId id) {
  // If we didn't get to sending the registration to the compositor thread yet,
  // just remove it.
  to_register_bitmaps_.Write(*this).erase(id);
  // Since we also track all previously sent registrations, we must remove that
  // to in order to prevent re-registering on another LayerTreeHost.
  registered_bitmaps_.Write(*this).erase(id);

  to_unregister_bitmap_ids_.Write(*this).push_back(id);
  // Unregistering a SharedBitmapId needs to happen eventually to prevent
  // leaking the SharedMemory in the display compositor. But this attempts to be
  // lazy and not force a commit prematurely, so just requests a
  // PushPropertiesTo() without requesting a commit.
  SetNeedsPushProperties();
}

TextureLayer::TransferableResourceHolder::TransferableResourceHolder(
    const viz::TransferableResource& resource,
    viz::ReleaseCallback release_callback)
    : resource_(resource),
      release_callback_(std::move(release_callback)),
      sync_token_(resource.sync_token()) {}

TextureLayer::TransferableResourceHolder::~TransferableResourceHolder() {
  if (release_callback_) {
    if (!release_callback_task_runner_ ||
        release_callback_task_runner_->RunsTasksInCurrentSequence()) {
      std::move(release_callback_).Run(sync_token_, is_lost_);
    } else {
      DCHECK(release_callback_task_runner_);
      release_callback_task_runner_->PostTask(
          FROM_HERE,
          base::BindOnce(std::move(release_callback_), sync_token_, is_lost_));
    }
  }
}

scoped_refptr<TextureLayer::TransferableResourceHolder>
TextureLayer::TransferableResourceHolder::Create(
    const viz::TransferableResource& resource,
    viz::ReleaseCallback release_callback) {
  return new TransferableResourceHolder(resource, std::move(release_callback));
}

void TextureLayer::TransferableResourceHolder::Return(
    scoped_refptr<base::SequencedTaskRunner> main_thread_task_runner,
    const gpu::SyncToken& sync_token,
    bool is_lost) {
  sync_token_ = sync_token;
  is_lost_ = is_lost;
  // When this method returns, the refcount of the holder will be decremented,
  // which might cause it to be destructed on the impl thread. We store the
  // main thread task runner here to make sure it's available to the destructor.
  release_callback_task_runner_ = std::move(main_thread_task_runner);
}

}  // namespace cc
