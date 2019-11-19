// Copyright 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/layers/texture_layer.h"

#include "base/bind.h"
#include "base/callback_helpers.h"
#include "base/location.h"
#include "base/sequenced_task_runner.h"
#include "base/synchronization/lock.h"
#include "base/trace_event/trace_event.h"
#include "cc/base/simple_enclosed_region.h"
#include "cc/layers/texture_layer_client.h"
#include "cc/layers/texture_layer_impl.h"
#include "cc/trees/layer_tree_host.h"
#include "components/viz/common/resources/single_release_callback.h"

namespace cc {

scoped_refptr<TextureLayer> TextureLayer::CreateForMailbox(
    TextureLayerClient* client) {
  return scoped_refptr<TextureLayer>(new TextureLayer(client));
}

TextureLayer::TextureLayer(TextureLayerClient* client) : client_(client) {}

TextureLayer::~TextureLayer() = default;

void TextureLayer::ClearClient() {
  client_ = nullptr;
  ClearTexture();
  UpdateDrawsContent(HasDrawableContent());
}

void TextureLayer::ClearTexture() {
  SetTransferableResource(viz::TransferableResource(), nullptr);
}

std::unique_ptr<LayerImpl> TextureLayer::CreateLayerImpl(
    LayerTreeImpl* tree_impl) {
  return TextureLayerImpl::Create(tree_impl, id());
}

void TextureLayer::SetFlipped(bool flipped) {
  if (flipped_ == flipped)
    return;
  flipped_ = flipped;
  SetNeedsCommit();
}

void TextureLayer::SetNearestNeighbor(bool nearest_neighbor) {
  if (nearest_neighbor_ == nearest_neighbor)
    return;
  nearest_neighbor_ = nearest_neighbor;
  SetNeedsCommit();
}

void TextureLayer::SetUV(const gfx::PointF& top_left,
                         const gfx::PointF& bottom_right) {
  if (uv_top_left_ == top_left && uv_bottom_right_ == bottom_right)
    return;
  uv_top_left_ = top_left;
  uv_bottom_right_ = bottom_right;
  SetNeedsCommit();
}

void TextureLayer::SetVertexOpacity(float bottom_left,
                                    float top_left,
                                    float top_right,
                                    float bottom_right) {
  // Indexing according to the quad vertex generation:
  // 1--2
  // |  |
  // 0--3
  if (vertex_opacity_[0] == bottom_left &&
      vertex_opacity_[1] == top_left &&
      vertex_opacity_[2] == top_right &&
      vertex_opacity_[3] == bottom_right)
    return;
  vertex_opacity_[0] = bottom_left;
  vertex_opacity_[1] = top_left;
  vertex_opacity_[2] = top_right;
  vertex_opacity_[3] = bottom_right;
  SetNeedsCommit();
}

void TextureLayer::SetPremultipliedAlpha(bool premultiplied_alpha) {
  if (premultiplied_alpha_ == premultiplied_alpha)
    return;
  premultiplied_alpha_ = premultiplied_alpha;
  SetNeedsCommit();
}

void TextureLayer::SetBlendBackgroundColor(bool blend) {
  if (blend_background_color_ == blend)
    return;
  blend_background_color_ = blend;
  SetNeedsCommit();
}

void TextureLayer::SetForceTextureToOpaque(bool opaque) {
  if (force_texture_to_opaque_ == opaque)
    return;
  force_texture_to_opaque_ = opaque;
  SetNeedsCommit();
}

void TextureLayer::SetTransferableResourceInternal(
    const viz::TransferableResource& resource,
    std::unique_ptr<viz::SingleReleaseCallback> release_callback,
    bool requires_commit) {
  DCHECK(resource.mailbox_holder.mailbox.IsZero() || !holder_ref_ ||
         resource != holder_ref_->holder()->resource());
  DCHECK_EQ(resource.mailbox_holder.mailbox.IsZero(), !release_callback);

  // If we never commited the mailbox, we need to release it here.
  if (!resource.mailbox_holder.mailbox.IsZero()) {
    holder_ref_ = TransferableResourceHolder::Create(
        resource, std::move(release_callback));
  } else {
    holder_ref_ = nullptr;
  }
  needs_set_resource_ = true;
  // If we are within a commit, no need to do it again immediately after.
  if (requires_commit)
    SetNeedsCommit();
  else
    SetNeedsPushProperties();

  UpdateDrawsContent(HasDrawableContent());
  // The active frame needs to be replaced and the mailbox returned before the
  // commit is called complete.
  SetNextCommitWaitsForActivation();
}

void TextureLayer::SetTransferableResource(
    const viz::TransferableResource& resource,
    std::unique_ptr<viz::SingleReleaseCallback> release_callback) {
  bool requires_commit = true;
  SetTransferableResourceInternal(resource, std::move(release_callback),
                                  requires_commit);
}

void TextureLayer::SetNeedsDisplayRect(const gfx::Rect& dirty_rect) {
  Layer::SetNeedsDisplayRect(dirty_rect);
}

void TextureLayer::SetLayerTreeHost(LayerTreeHost* host) {
  if (layer_tree_host() == host) {
    Layer::SetLayerTreeHost(host);
    return;
  }

  // If we're removed from the tree, the TextureLayerImpl will be destroyed, and
  // we will need to set the mailbox again on a new TextureLayerImpl the next
  // time we push.
  if (!host && holder_ref_) {
    needs_set_resource_ = true;
    // The active frame needs to be replaced and the mailbox returned before the
    // commit is called complete.
    SetNextCommitWaitsForActivation();
  }
  if (host) {
    // When attached to a new LayerTreHost, all previously registered
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
  return (client_ || holder_ref_) && Layer::HasDrawableContent();
}

bool TextureLayer::Update() {
  bool updated = Layer::Update();
  if (client_) {
    viz::TransferableResource resource;
    std::unique_ptr<viz::SingleReleaseCallback> release_callback;
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

bool TextureLayer::IsSnappedToPixelGridInTarget() {
  // Often layers are positioned with CSS to "50%", which can often leave them
  // with a fractional (N + 0.5) pixel position. This would leave them looking
  // fuzzy, so we request that TextureLayers are snapped to the pixel grid,
  // since their content is generated externally and we can not adjust for it
  // inside the content (unlike for PictureLayers).
  return true;
}

void TextureLayer::PushPropertiesTo(LayerImpl* layer) {
  Layer::PushPropertiesTo(layer);
  TRACE_EVENT0("cc", "TextureLayer::PushPropertiesTo");

  TextureLayerImpl* texture_layer = static_cast<TextureLayerImpl*>(layer);
  texture_layer->SetFlipped(flipped_);
  texture_layer->SetNearestNeighbor(nearest_neighbor_);
  texture_layer->SetUVTopLeft(uv_top_left_);
  texture_layer->SetUVBottomRight(uv_bottom_right_);
  texture_layer->SetVertexOpacity(vertex_opacity_);
  texture_layer->SetPremultipliedAlpha(premultiplied_alpha_);
  texture_layer->SetBlendBackgroundColor(blend_background_color_);
  texture_layer->SetForceTextureToOpaque(force_texture_to_opaque_);
  if (needs_set_resource_) {
    viz::TransferableResource resource;
    std::unique_ptr<viz::SingleReleaseCallback> release_callback;
    if (holder_ref_) {
      TransferableResourceHolder* holder = holder_ref_->holder();
      resource = holder->resource();
      release_callback = holder->GetCallbackForImplThread(
          layer_tree_host()->GetTaskRunnerProvider()->MainThreadTaskRunner());
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

TextureLayer::TransferableResourceHolder::MainThreadReference::
    MainThreadReference(TransferableResourceHolder* holder)
    : holder_(holder) {
  holder_->InternalAddRef();
}

TextureLayer::TransferableResourceHolder::MainThreadReference::
    ~MainThreadReference() {
#if DCHECK_IS_ON()
  {
    base::AutoLock hold(holder_->posted_internal_derefs_lock_);
    ++holder_->posted_internal_derefs_;
  }
#endif
  holder_->InternalRelease();
}

TextureLayer::TransferableResourceHolder::TransferableResourceHolder(
    const viz::TransferableResource& resource,
    std::unique_ptr<viz::SingleReleaseCallback> release_callback)
    : resource_(resource),
      release_callback_(std::move(release_callback)),
      sync_token_(resource.mailbox_holder.sync_token) {}

TextureLayer::TransferableResourceHolder::~TransferableResourceHolder() {
#if DCHECK_IS_ON()
  {
    // If the MessageLoop is destroyed while a posted deref is waiting to run,
    // this object will be destroyed with an internal_references_ still present.
    // So we must also include the outstanding posted derefences.
    base::AutoLock hold(posted_internal_derefs_lock_);
    DCHECK_EQ(internal_references_, posted_internal_derefs_);
  }
#endif
  if (release_callback_) {
    // We land here if the dereferences are posted but not run and the
    // MessageLoop is destroyed, destroying those tasks and this object with it.
    // We run the ReleaseCallback in that case assuming the MessageLoop is being
    // destroyed on the main thread.
    DCHECK(main_thread_checker_.CalledOnValidThread());
    release_callback_->Run(sync_token_, is_lost_);
  }
}

std::unique_ptr<TextureLayer::TransferableResourceHolder::MainThreadReference>
TextureLayer::TransferableResourceHolder::Create(
    const viz::TransferableResource& resource,
    std::unique_ptr<viz::SingleReleaseCallback> release_callback) {
  return std::make_unique<MainThreadReference>(
      new TransferableResourceHolder(resource, std::move(release_callback)));
}

void TextureLayer::TransferableResourceHolder::Return(
    const gpu::SyncToken& sync_token,
    bool is_lost) {
  base::AutoLock lock(arguments_lock_);
  sync_token_ = sync_token;
  is_lost_ = is_lost;
}

std::unique_ptr<viz::SingleReleaseCallback>
TextureLayer::TransferableResourceHolder::GetCallbackForImplThread(
    scoped_refptr<base::SequencedTaskRunner> main_thread_task_runner) {
  // We can't call GetCallbackForImplThread if we released the main thread
  // reference.
  DCHECK_GT(internal_references_, 0);
  InternalAddRef();
  return viz::SingleReleaseCallback::Create(
      base::BindOnce(&TransferableResourceHolder::ReturnAndReleaseOnImplThread,
                     this, std::move(main_thread_task_runner)));
}

void TextureLayer::TransferableResourceHolder::InternalAddRef() {
  ++internal_references_;
}

void TextureLayer::TransferableResourceHolder::InternalRelease() {
  DCHECK(main_thread_checker_.CalledOnValidThread());
#if DCHECK_IS_ON()
  {
    base::AutoLock hold(posted_internal_derefs_lock_);
    --posted_internal_derefs_;
  }
#endif
  if (!--internal_references_) {
    release_callback_->Run(sync_token_, is_lost_);
    resource_ = viz::TransferableResource();
    release_callback_ = nullptr;
  }
}

void TextureLayer::TransferableResourceHolder::ReturnAndReleaseOnImplThread(
    const scoped_refptr<base::SequencedTaskRunner>& main_thread_task_runner,
    const gpu::SyncToken& sync_token,
    bool is_lost) {
  Return(sync_token, is_lost);
#if DCHECK_IS_ON()
  {
    base::AutoLock hold(posted_internal_derefs_lock_);
    ++posted_internal_derefs_;
  }
#endif
  main_thread_task_runner->PostTask(
      FROM_HERE,
      base::BindOnce(&TransferableResourceHolder::InternalRelease, this));
}

}  // namespace cc
