// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CC_TREES_LAYER_TREE_FRAME_SINK_H_
#define CC_TREES_LAYER_TREE_FRAME_SINK_H_

#include <deque>
#include <memory>

#include "base/memory/read_only_shared_memory_region.h"
#include "base/memory/ref_counted.h"
#include "base/memory/weak_ptr.h"
#include "base/single_thread_task_runner.h"
#include "base/threading/thread_checker.h"
#include "cc/cc_export.h"
#include "components/viz/client/shared_bitmap_reporter.h"
#include "components/viz/common/gpu/context_lost_observer.h"
#include "components/viz/common/gpu/context_provider.h"
#include "components/viz/common/gpu/raster_context_provider.h"
#include "components/viz/common/resources/returned_resource.h"
#include "gpu/command_buffer/common/texture_in_use_response.h"
#include "ui/gfx/color_space.h"

namespace gpu {
class GpuMemoryBufferManager;
}

namespace viz {
class CompositorFrame;
class LocalSurfaceId;
struct BeginFrameAck;
}  // namespace viz

namespace cc {
class LayerTreeFrameSinkClient;

// An interface for submitting CompositorFrames to a display compositor
// which will compose frames from multiple clients to show on screen to the
// user.
// If a context_provider() is present, frames should be submitted with
// OpenGL resources (created with the context_provider()). If not, then
// SharedMemory resources should be used.
class CC_EXPORT LayerTreeFrameSink : public viz::SharedBitmapReporter,
                                     public viz::ContextLostObserver {
 public:
  // Constructor for GL-based and/or software resources.
  //
  // |compositor_task_runner| is used to post worker context lost callback and
  // must belong to the same thread where all calls to or from client are made.
  // Optional and won't be used unless |worker_context_provider| is present.
  //
  // |gpu_memory_buffer_manager| and |shared_bitmap_manager| must outlive the
  // LayerTreeFrameSink. |shared_bitmap_manager| is optional (won't be used) if
  // |context_provider| is present. |gpu_memory_buffer_manager| is optional
  // (won't be used) unless |context_provider| is present.
  LayerTreeFrameSink(
      scoped_refptr<viz::ContextProvider> context_provider,
      scoped_refptr<viz::RasterContextProvider> worker_context_provider,
      scoped_refptr<base::SingleThreadTaskRunner> compositor_task_runner,
      gpu::GpuMemoryBufferManager* gpu_memory_buffer_manager);
  LayerTreeFrameSink(const LayerTreeFrameSink&) = delete;

  ~LayerTreeFrameSink() override;

  LayerTreeFrameSink& operator=(const LayerTreeFrameSink&) = delete;

  base::WeakPtr<LayerTreeFrameSink> GetWeakPtr();

  // Called by the compositor on the compositor thread. This is a place where
  // thread-specific data for the output surface can be initialized, since from
  // this point to when DetachFromClient() is called the output surface will
  // only be used on the compositor thread.
  // The caller should call DetachFromClient() on the same thread before
  // destroying the LayerTreeFrameSink, even if this fails. And BindToClient
  // should not be called twice for a given LayerTreeFrameSink.
  virtual bool BindToClient(LayerTreeFrameSinkClient* client);

  // Must be called from the thread where BindToClient was called if
  // BindToClient succeeded, after which the LayerTreeFrameSink may be
  // destroyed from any thread. This is a place where thread-specific data for
  // the object can be uninitialized.
  virtual void DetachFromClient();

  bool HasClient() { return !!client_; }

  void set_source_frame_number(int64_t frame_number) {
    source_frame_number_ = frame_number;
  }

  // The viz::ContextProviders may be null if frames should be submitted with
  // software SharedMemory resources.
  viz::ContextProvider* context_provider() const {
    return context_provider_.get();
  }
  viz::RasterContextProvider* worker_context_provider() const {
    return worker_context_provider_.get();
  }
  gpu::GpuMemoryBufferManager* gpu_memory_buffer_manager() const {
    return gpu_memory_buffer_manager_;
  }

  // If supported, this sets the viz::LocalSurfaceId the LayerTreeFrameSink will
  // use to submit a CompositorFrame.
  virtual void SetLocalSurfaceId(const viz::LocalSurfaceId& local_surface_id) {}

  // Support for a pull-model where draws are requested by the implementation of
  // LayerTreeFrameSink. This is called by the compositor to notify that there's
  // new content. Can be called when nothing needs to be drawn if tile
  // priorities should be updated.
  virtual void Invalidate(bool needs_draw) {}

  // For successful swaps, the implementation must call
  // DidReceiveCompositorFrameAck() asynchronously when the frame has been
  // processed in order to unthrottle the next frame.
  // If |hit_test_data_changed| is false, we do an equality check
  // with the old hit-test data. If there is no change, we do not send the
  // hit-test data. False positives are allowed. The value of
  // |hit_test_data_changed| should remain constant in the caller.
  // |show_hit_test_borders| controls whether viz will insert debug borders over
  // hit-test data and is passed from LayerTreeDebugState.
  virtual void SubmitCompositorFrame(viz::CompositorFrame frame,
                                     bool hit_test_data_changed,
                                     bool show_hit_test_borders) = 0;

  // Signals that a BeginFrame issued by the viz::BeginFrameSource provided to
  // the client did not lead to a CompositorFrame submission.
  virtual void DidNotProduceFrame(const viz::BeginFrameAck& ack) = 0;

  // viz::SharedBitmapReporter implementation.
  void DidAllocateSharedBitmap(base::ReadOnlySharedMemoryRegion region,
                               const viz::SharedBitmapId& id) override = 0;
  void DidDeleteSharedBitmap(const viz::SharedBitmapId& id) override = 0;

 protected:
  class ContextLostForwarder;

  // viz::ContextLostObserver:
  void OnContextLost() override;

  LayerTreeFrameSinkClient* client_ = nullptr;

  scoped_refptr<viz::ContextProvider> context_provider_;
  scoped_refptr<viz::RasterContextProvider> worker_context_provider_;
  scoped_refptr<base::SingleThreadTaskRunner> compositor_task_runner_;
  gpu::GpuMemoryBufferManager* gpu_memory_buffer_manager_;

  std::unique_ptr<ContextLostForwarder> worker_context_lost_forwarder_;

  int64_t source_frame_number_;

 private:
  THREAD_CHECKER(thread_checker_);
  base::WeakPtrFactory<LayerTreeFrameSink> weak_ptr_factory_{this};
};

}  // namespace cc

#endif  // CC_TREES_LAYER_TREE_FRAME_SINK_H_
