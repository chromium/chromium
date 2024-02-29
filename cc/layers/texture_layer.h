// Copyright 2010 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CC_LAYERS_TEXTURE_LAYER_H_
#define CC_LAYERS_TEXTURE_LAYER_H_

#include <memory>
#include <optional>
#include <string>
#include <vector>

#include "base/functional/callback.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/synchronization/lock.h"
#include "base/task/sequenced_task_runner.h"
#include "base/threading/thread_checker.h"
#include "cc/cc_export.h"
#include "cc/layers/layer.h"
#include "cc/resources/cross_thread_shared_bitmap.h"
#include "cc/resources/shared_bitmap_id_registrar.h"
#include "components/viz/common/resources/release_callback.h"
#include "components/viz/common/resources/transferable_resource.h"
#include "ui/gfx/hdr_metadata.h"

namespace gpu {
struct SyncToken;
}

namespace cc {
class TextureLayer;
class TextureLayerClient;

// A Layer containing a the rendered output of a plugin instance. It can be used
// to display gpu or software resources, depending if the compositor is working
// in gpu or software compositing mode (the resources must match the compositing
// mode).
class CC_EXPORT TextureLayer : public Layer, SharedBitmapIdRegistrar {
 public:
  class CC_EXPORT TransferableResourceHolder
      : public base::RefCountedThreadSafe<TransferableResourceHolder> {
   public:
    TransferableResourceHolder(const TransferableResourceHolder&) = delete;
    TransferableResourceHolder& operator=(const TransferableResourceHolder&) =
        delete;

    const viz::TransferableResource& resource() const { return resource_; }
    void Return(
        scoped_refptr<base::SequencedTaskRunner> main_thread_task_runner,
        const gpu::SyncToken& sync_token,
        bool is_lost);

    static scoped_refptr<TransferableResourceHolder> Create(
        const viz::TransferableResource& resource,
        viz::ReleaseCallback release_callback);

   protected:
    virtual ~TransferableResourceHolder();

   private:
    friend class base::RefCountedThreadSafe<TransferableResourceHolder>;
    explicit TransferableResourceHolder(
        const viz::TransferableResource& resource,
        viz::ReleaseCallback release_callback);

    const viz::TransferableResource resource_;

    // This is accessed only on the main thread.
    viz::ReleaseCallback release_callback_;

    // release_callback_task_runner_, sync_token_, and is_lost_ are only
    // modified on the impl thread, and only read from the destructor, so they
    // are not subject to race conditions.

    // If a reference to the resource is sent to the impl thread, then there's a
    // possibility that the resource will be destructed on the impl thread; but
    // release_callback_ has to run on the main thread. In that case, we use
    // release_callback_task_runner_ to PostTask to run the ReleaseCallback.
    scoped_refptr<base::SequencedTaskRunner> release_callback_task_runner_;
    gpu::SyncToken sync_token_;
    bool is_lost_ = false;
  };

  // Used when mailbox names are specified instead of texture IDs.
  static scoped_refptr<TextureLayer> CreateForMailbox(
      TextureLayerClient* client);

  TextureLayer(const TextureLayer&) = delete;
  TextureLayer& operator=(const TextureLayer&) = delete;

  // Resets the client, which also resets the texture.
  void ClearClient();

  // Resets the texture.
  void ClearTexture();

  std::unique_ptr<LayerImpl> CreateLayerImpl(
      LayerTreeImpl* tree_impl) const override;

  // Sets whether this texture should be Y-flipped at draw time. Defaults to
  // true.
  void SetFlipped(bool flipped);
  bool flipped() const { return flipped_.Read(*this); }

  // Sets whether this texture should use nearest neighbor interpolation as
  // opposed to bilinear. Defaults to false.
  void SetNearestNeighbor(bool nearest_neighbor);

  // Sets a UV transform to be used at draw time. Defaults to (0, 0) and (1, 1).
  void SetUV(const gfx::PointF& top_left, const gfx::PointF& bottom_right);

  // Sets whether the alpha channel is premultiplied or unpremultiplied.
  // Defaults to true.
  void SetPremultipliedAlpha(bool premultiplied_alpha);

  // Sets whether the texture should be blended with the background color
  // at draw time. Defaults to false.
  void SetBlendBackgroundColor(bool blend);

  // Sets whether we need to ensure that Texture is opaque before using it.
  // This will blend texture with black color. Defaults to false.
  void SetForceTextureToOpaque(bool opaque);

  // Code path for plugins which supply their own mailbox.
  void SetTransferableResource(const viz::TransferableResource& resource,
                               viz::ReleaseCallback release_callback);
  void SetNeedsSetTransferableResource();

  void SetLayerTreeHost(LayerTreeHost* layer_tree_host) override;
  bool RequiresSetNeedsDisplayOnHdrHeadroomChange() const override;
  bool Update() override;
  bool IsSnappedToPixelGridInTarget() const override;
  void PushPropertiesTo(LayerImpl* layer,
                        const CommitState& commit_state,
                        const ThreadUnsafeCommitState& unsafe_state) override;

  // Request a mapping from SharedBitmapId to SharedMemory be registered via the
  // LayerTreeFrameSink with the display compositor. Once this mapping is
  // registered, the SharedBitmapId can be used in TransferableResources given
  // to the TextureLayer for display. The SharedBitmapId registration will end
  // when the returned SharedBitmapIdRegistration object is destroyed.
  // Implemented as a SharedBitmapIdRegistrar interface so that clients can
  // have a limited API access.
  SharedBitmapIdRegistration RegisterSharedBitmapId(
      const viz::SharedBitmapId& id,
      scoped_refptr<CrossThreadSharedBitmap> bitmap) override;

  const viz::TransferableResource current_transferable_resource() const {
    if (const auto& resource_holder = resource_holder_.Read(*this))
      return resource_holder->resource();
    return viz::TransferableResource();
  }

  bool needs_set_resource_for_testing() const {
    return needs_set_resource_.Read(*this);
  }

 protected:
  explicit TextureLayer(TextureLayerClient* client);
  ~TextureLayer() override;
  bool HasDrawableContent() const override;

 private:
  void SetTransferableResourceInternal(
      const viz::TransferableResource& resource,
      viz::ReleaseCallback release_callback,
      bool requires_commit);

  // Friends to give access to UnregisterSharedBitmapId().
  friend SharedBitmapIdRegistration;
  // Remove a mapping from SharedBitmapId to SharedMemory in the display
  // compositor.
  void UnregisterSharedBitmapId(viz::SharedBitmapId id);

  // Dangling on `mac-rel` in `blink_web_tests`:
  // `fast/events/touch/touch-handler-iframe-plugin-assert.html`
  ProtectedSequenceForbidden<raw_ptr<TextureLayerClient, DanglingUntriaged>>
      client_;

  ProtectedSequenceReadable<bool> flipped_;
  ProtectedSequenceReadable<bool> nearest_neighbor_;
  ProtectedSequenceReadable<gfx::PointF> uv_top_left_;
  ProtectedSequenceReadable<gfx::PointF> uv_bottom_right_;
  // [bottom left, top left, top right, bottom right]
  ProtectedSequenceReadable<bool> premultiplied_alpha_;
  ProtectedSequenceReadable<bool> blend_background_color_;
  ProtectedSequenceReadable<bool> force_texture_to_opaque_;

  ProtectedSequenceWritable<scoped_refptr<TransferableResourceHolder>>
      resource_holder_;
  ProtectedSequenceWritable<bool> needs_set_resource_;

  typedef base::flat_map<viz::SharedBitmapId,
                         scoped_refptr<CrossThreadSharedBitmap>>
      BitMapMap;

  // The set of SharedBitmapIds to register with the LayerTreeFrameSink on the
  // compositor thread. These requests are forwarded to the TextureLayerImpl to
  // use, then stored in |registered_bitmaps_| to re-send if the
  // TextureLayerImpl object attached to this layer changes, by moving out of
  // the LayerTreeHost.
  ProtectedSequenceWritable<BitMapMap> to_register_bitmaps_;
  // The set of previously registered SharedBitmapIds for the current
  // LayerTreeHost. If the LayerTreeHost changes, these must be re-sent to the
  // (new) TextureLayerImpl to be re-registered.
  ProtectedSequenceWritable<BitMapMap> registered_bitmaps_;
  // The SharedBitmapIds to unregister on the compositor thread, passed to the
  // TextureLayerImpl.
  ProtectedSequenceWritable<std::vector<viz::SharedBitmapId>>
      to_unregister_bitmap_ids_;

  const base::WeakPtrFactory<TextureLayer> weak_ptr_factory_{this};
};

}  // namespace cc
#endif  // CC_LAYERS_TEXTURE_LAYER_H_
