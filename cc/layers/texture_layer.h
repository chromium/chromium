// Copyright 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CC_LAYERS_TEXTURE_LAYER_H_
#define CC_LAYERS_TEXTURE_LAYER_H_

#include <string>

#include "base/callback.h"
#include "base/memory/weak_ptr.h"
#include "base/synchronization/lock.h"
#include "base/threading/thread_checker.h"
#include "cc/cc_export.h"
#include "cc/layers/layer.h"
#include "cc/resources/cross_thread_shared_bitmap.h"
#include "cc/resources/shared_bitmap_id_registrar.h"
#include "components/viz/common/resources/transferable_resource.h"

namespace gpu {
struct SyncToken;
}

namespace viz {
class SingleReleaseCallback;
}

namespace cc {
class SingleReleaseCallback;
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
    class CC_EXPORT MainThreadReference {
     public:
      explicit MainThreadReference(TransferableResourceHolder* holder);
      MainThreadReference(const MainThreadReference&) = delete;
      ~MainThreadReference();

      MainThreadReference& operator=(const MainThreadReference&) = delete;

      TransferableResourceHolder* holder() { return holder_.get(); }

     private:
      scoped_refptr<TransferableResourceHolder> holder_;
    };

    TransferableResourceHolder(const TransferableResourceHolder&) = delete;
    TransferableResourceHolder& operator=(const TransferableResourceHolder&) =
        delete;

    const viz::TransferableResource& resource() const { return resource_; }
    void Return(const gpu::SyncToken& sync_token, bool is_lost);

    // Gets a viz::ReleaseCallback that can be called from another thread. Note:
    // the caller must ensure the callback is called.
    std::unique_ptr<viz::SingleReleaseCallback> GetCallbackForImplThread(
        scoped_refptr<base::SequencedTaskRunner> main_thread_task_runner);

   protected:
    friend class TextureLayer;

    // Protected visiblity so only TextureLayer and unit tests can create these.
    static std::unique_ptr<MainThreadReference> Create(
        const viz::TransferableResource& resource,
        std::unique_ptr<viz::SingleReleaseCallback> release_callback);
    virtual ~TransferableResourceHolder();

   private:
    friend class base::RefCountedThreadSafe<TransferableResourceHolder>;
    friend class MainThreadReference;
    explicit TransferableResourceHolder(
        const viz::TransferableResource& resource,
        std::unique_ptr<viz::SingleReleaseCallback> release_callback);

    void InternalAddRef();
    void InternalRelease();
    void ReturnAndReleaseOnImplThread(
        const scoped_refptr<base::SequencedTaskRunner>& main_thread_task_runner,
        const gpu::SyncToken& sync_token,
        bool is_lost);

    // These members are only accessed on the main thread, or on the impl thread
    // during commit where the main thread is blocked.
    int internal_references_ = 0;
#if DCHECK_IS_ON()
    // The number of derefs posted from the impl thread, and a lock for
    // accessing it.
    base::Lock posted_internal_derefs_lock_;
    int posted_internal_derefs_ = 0;
#endif
    viz::TransferableResource resource_;
    std::unique_ptr<viz::SingleReleaseCallback> release_callback_;

    // This lock guards the sync_token_ and is_lost_ fields because they can be
    // accessed on both the impl and main thread. We do this to ensure that the
    // values of these fields are well-ordered such that the last call to
    // ReturnAndReleaseOnImplThread() defines their values.
    base::Lock arguments_lock_;
    gpu::SyncToken sync_token_;
    bool is_lost_ = false;
    base::ThreadChecker main_thread_checker_;
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

  std::unique_ptr<LayerImpl> CreateLayerImpl(LayerTreeImpl* tree_impl) override;

  // Sets whether this texture should be Y-flipped at draw time. Defaults to
  // true.
  void SetFlipped(bool flipped);
  bool flipped() const { return flipped_; }

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
  void SetTransferableResource(
      const viz::TransferableResource& resource,
      std::unique_ptr<viz::SingleReleaseCallback> release_callback);

  void SetNeedsDisplayRect(const gfx::Rect& dirty_rect) override;

  void SetLayerTreeHost(LayerTreeHost* layer_tree_host) override;
  bool Update() override;
  bool IsSnappedToPixelGridInTarget() override;
  void PushPropertiesTo(LayerImpl* layer) override;

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

  viz::TransferableResource current_transferable_resource() const {
    return holder_ref_ ? holder_ref_->holder()->resource()
                       : viz::TransferableResource();
  }

 protected:
  explicit TextureLayer(TextureLayerClient* client);
  ~TextureLayer() override;
  bool HasDrawableContent() const override;

 private:
  void SetTransferableResourceInternal(
      const viz::TransferableResource& resource,
      std::unique_ptr<viz::SingleReleaseCallback> release_callback,
      bool requires_commit);

  // Friends to give access to UnregisterSharedBitmapId().
  friend SharedBitmapIdRegistration;
  // Remove a mapping from SharedBitmapId to SharedMemory in the display
  // compositor.
  void UnregisterSharedBitmapId(viz::SharedBitmapId id);

  TextureLayerClient* client_;

  bool flipped_ = true;
  bool nearest_neighbor_ = false;
  gfx::PointF uv_top_left_ = gfx::PointF();
  gfx::PointF uv_bottom_right_ = gfx::PointF(1.f, 1.f);
  // [bottom left, top left, top right, bottom right]
  bool premultiplied_alpha_ = true;
  bool blend_background_color_ = false;
  bool force_texture_to_opaque_ = false;

  std::unique_ptr<TransferableResourceHolder::MainThreadReference> holder_ref_;
  bool needs_set_resource_ = false;

  // The set of SharedBitmapIds to register with the LayerTreeFrameSink on the
  // compositor thread. These requests are forwarded to the TextureLayerImpl to
  // use, then stored in |registered_bitmaps_| to re-send if the
  // TextureLayerImpl object attached to this layer changes, by moving out of
  // the LayerTreeHost.
  base::flat_map<viz::SharedBitmapId, scoped_refptr<CrossThreadSharedBitmap>>
      to_register_bitmaps_;
  // The set of previously registered SharedBitmapIds for the current
  // LayerTreeHost. If the LayerTreeHost changes, these must be re-sent to the
  // (new) TextureLayerImpl to be re-registered.
  base::flat_map<viz::SharedBitmapId, scoped_refptr<CrossThreadSharedBitmap>>
      registered_bitmaps_;
  // The SharedBitmapIds to unregister on the compositor thread, passed to the
  // TextureLayerImpl.
  std::vector<viz::SharedBitmapId> to_unregister_bitmap_ids_;

  base::WeakPtrFactory<TextureLayer> weak_ptr_factory_{this};
};

}  // namespace cc
#endif  // CC_LAYERS_TEXTURE_LAYER_H_
