// Copyright 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/layers/texture_layer.h"

#include <memory>
#include <utility>

#include "base/bind.h"
#include "base/callback_helpers.h"
#include "base/containers/cxx20_erase.h"
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

TextureLayer::TextureLayer(TextureLayerClient* client) : client_(client) {}

TextureLayer::~TextureLayer() = default;

void TextureLayer::ClearClient() {
  DCHECK(IsMutationAllowed());
  client_ = nullptr;
  ClearTexture();
  UpdateDrawsContent(HasDrawableContent());
}

void TextureLayer::ClearTexture() {
  DCHECK(IsMutationAllowed());
  SetTransferableResource(viz::TransferableResource(), viz::ReleaseCallback());
}

std::unique_ptr<LayerImpl> TextureLayer::CreateLayerImpl(
    LayerTreeImpl* tree_impl) {
  return TextureLayerImpl::Create(tree_impl, id());
}

void TextureLayer::SetFlipped(bool flipped) {
  DCHECK(IsMutationAllowed());
  if (flipped_ == flipped)
    return;
  flipped_ = flipped;
  SetNeedsCommit();
}

void TextureLayer::SetNearestNeighbor(bool nearest_neighbor) {
  DCHECK(IsMutationAllowed());
  if (nearest_neighbor_ == nearest_neighbor)
    return;
  nearest_neighbor_ = nearest_neighbor;
  SetNeedsCommit();
}

void TextureLayer::SetUV(const gfx::PointF& top_left,
                         const gfx::PointF& bottom_right) {
  DCHECK(IsMutationAllowed());
  if (uv_top_left_ == top_left && uv_bottom_right_ == bottom_right)
    return;
  uv_top_left_ = top_left;
  uv_bottom_right_ = bottom_right;
  SetNeedsCommit();
}

void TextureLayer::SetPremultipliedAlpha(bool premultiplied_alpha) {
  DCHECK(IsMutationAllowed());
  if (premultiplied_alpha_ == premultiplied_alpha)
    return;
  premultiplied_alpha_ = premultiplied_alpha;
  SetNeedsCommit();
}

void TextureLayer::SetBlendBackgroundColor(bool blend) {
  DCHECK(IsMutationAllowed());
  if (blend_background_color_ == blend)
    return;
  blend_background_color_ = blend;
  SetNeedsCommit();
}

void TextureLayer::SetForceTextureToOpaque(bool opaque) {
  DCHECK(IsMutationAllowed());
  if (force_texture_to_opaque_ == opaque)
    return;
  force_texture_to_opaque_ = opaque;
  SetNeedsCommit();
}

void TextureLayer::SetTransferableResourceInternal(
    const viz::TransferableResource& resource,
    viz::ReleaseCallback release_callback,
    bool requires_commit) {
  DCHECK(IsMutationAllowed());
  DCHECK(resource.mailbox_holder.mailbox.IsZero() || !resource_holder_ ||
         resource != resource_holder_->resource());
  DCHECK_EQ(resource.mailbox_holder.mailbox.IsZero(), !release_callback);

  // If we never commited the mailbox, we need to release it here.
  if (!resource.mailbox_holder.mailbox.IsZero()) {
    resource_holder_ = TransferableResourceHolder::Create(
        resource, std::move(release_callback));
  } else {
    resource_holder_ = nullptr;
  }
  needs_set_resource_ = true;
  // If we are within a commit, no need to do it again immediately after.
  if (requires_commit)
    SetNeedsCommit();
  else
    SetNeedsPushProperties();

  UpdateDrawsContent(HasDrawableContent());
}

void TextureLayer::SetTransferableResource(
    const viz::TransferableResource& resource,
    viz::ReleaseCallback release_callback) {
  DCHECK(IsMutationAllowed());
  bool requires_commit = true;
  SetTransferableResourceInternal(resource, std::move(release_callback),
                                  requires_commit);
}

void TextureLayer::SetLayerTreeHost(LayerTreeHost* host) {
  DCHECK(IsMutationAllowed());
  if (layer_tree_host() == host) {
    Layer::SetLayerTreeHost(host);
    return;
  }

  // If we're removed from the tree, the TextureLayerImpl will be destroyed, and
  // we will need to set the mailbox again on a new TextureLayerImpl the next
  // time we push.
  if (!host && resource_holder_)
    needs_set_resource_ = true;
  if (host) {
    // When attached to a new LayerTreeHost, all previously registered
    // SharedBitmapIds will need to be re-sent to the new TextureLayerImpl
    // representing this layer on the compositor thread.
    to_register_bitmaps_.insert(
        std::make_move_iterator(registered_bitmaps_.begin()),
        std::make_move_iterator(registered_bitmaps_.end()));
    registered_bitmaps_.clear();
  }
  Layer::SetLayerTreeHost(host);
}

bool TextureLayer::HasDrawableContent() const {
  return (client_ || resource_holder_) && Layer::HasDrawableContent();
}

bool TextureLayer::Update() {
  bool updated = Layer::Update();
  if (client_) {
    viz::TransferableResource resource;
    viz::ReleaseCallback release_callback;
    if (client_->PrepareTransferableResource(this, &resource,
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
  texture_layer->SetFlipped(flipped_);
  texture_layer->SetNearestNeighbor(nearest_neighbor_);
  texture_layer->SetUVTopLeft(uv_top_left_);
  texture_layer->SetUVBottomRight(uv_bottom_right_);
  texture_layer->SetPremultipliedAlpha(premultiplied_alpha_);
  texture_layer->SetBlendBackgroundColor(blend_background_color_);
  texture_layer->SetForceTextureToOpaque(force_texture_to_opaque_);
  if (needs_set_resource_) {
    viz::TransferableResource resource;
    viz::ReleaseCallback release_callback;
    if (resource_holder_) {
      resource = resource_holder_->resource();
      release_callback =
          base::BindOnce(&TransferableResourceHolder::Return, resource_holder_,
                         base::RetainedRef(layer->layer_tree_impl()
                                               ->task_runner_provider()
                                               ->MainThreadTaskRunner()));
    }
    texture_layer->SetTransferableResource(resource,
                                           std::move(release_callback));
    needs_set_resource_ = false;
  }
  for (auto& pair : to_register_bitmaps_)
    texture_layer->RegisterSharedBitmapId(pair.first, pair.second);
  // Store the registered SharedBitmapIds in case we get a new TextureLayerImpl,
  // in a new tree, to re-send them to.
  registered_bitmaps_.insert(
      std::make_move_iterator(to_register_bitmaps_.begin()),
      std::make_move_iterator(to_register_bitmaps_.end()));
  to_register_bitmaps_.clear();
  for (const auto& id : to_unregister_bitmap_ids_)
    texture_layer->UnregisterSharedBitmapId(id);
  to_unregister_bitmap_ids_.clear();
}

SharedBitmapIdRegistration TextureLayer::RegisterSharedBitmapId(
    const viz::SharedBitmapId& id,
    scoped_refptr<CrossThreadSharedBitmap> bitmap) {
  DCHECK(IsMutationAllowed());
  DCHECK(to_register_bitmaps_.find(id) == to_register_bitmaps_.end());
  DCHECK(registered_bitmaps_.find(id) == registered_bitmaps_.end());
  to_register_bitmaps_[id] = std::move(bitmap);
  base::Erase(to_unregister_bitmap_ids_, id);
  // This does not SetNeedsCommit() to be as lazy as possible. Notifying a
  // SharedBitmapId is not needed until it is used, and using it will require
  // a commit, so we can wait for that commit before forwarding the
  // notification instead of forcing it to happen as a side effect of this
  // method.
  SetNeedsPushProperties();
  return SharedBitmapIdRegistration(weak_ptr_factory_.GetWeakPtr(), id);
}

void TextureLayer::UnregisterSharedBitmapId(viz::SharedBitmapId id) {
  DCHECK(IsMutationAllowed());
  // If we didn't get to sending the registration to the compositor thread yet,
  // just remove it.
  to_register_bitmaps_.erase(id);
  // Since we also track all previously sent registrations, we must remove that
  // to in order to prevent re-registering on another LayerTreeHost.
  registered_bitmaps_.erase(id);

  to_unregister_bitmap_ids_.push_back(id);
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
      sync_token_(resource.mailbox_holder.sync_token) {}

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
