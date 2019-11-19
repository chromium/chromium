// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/trees/layer_tree_frame_sink.h"

#include <stdint.h>

#include "base/bind.h"
#include "base/location.h"
#include "base/threading/thread_task_runner_handle.h"
#include "base/trace_event/trace_event.h"
#include "cc/trees/layer_tree_frame_sink_client.h"
#include "components/viz/common/gpu/context_lost_observer.h"
#include "gpu/GLES2/gl2extchromium.h"
#include "gpu/command_buffer/client/context_support.h"
#include "gpu/command_buffer/client/raster_interface.h"

namespace cc {

class LayerTreeFrameSink::ContextLostForwarder
    : public viz::ContextLostObserver {
 public:
  ContextLostForwarder(base::WeakPtr<LayerTreeFrameSink> frame_sink,
                       scoped_refptr<base::SingleThreadTaskRunner> task_runner)
      : frame_sink_(frame_sink), task_runner_(std::move(task_runner)) {}
  ContextLostForwarder(const ContextLostForwarder&) = delete;
  ~ContextLostForwarder() override = default;

  ContextLostForwarder& operator=(const ContextLostForwarder&) = delete;

  void OnContextLost() override {
    task_runner_->PostTask(
        FROM_HERE,
        base::BindOnce(&LayerTreeFrameSink::OnContextLost, frame_sink_));
  }

 private:
  base::WeakPtr<LayerTreeFrameSink> frame_sink_;
  scoped_refptr<base::SingleThreadTaskRunner> task_runner_;
};

LayerTreeFrameSink::LayerTreeFrameSink(
    scoped_refptr<viz::ContextProvider> context_provider,
    scoped_refptr<viz::RasterContextProvider> worker_context_provider,
    scoped_refptr<base::SingleThreadTaskRunner> compositor_task_runner,
    gpu::GpuMemoryBufferManager* gpu_memory_buffer_manager)
    : context_provider_(std::move(context_provider)),
      worker_context_provider_(std::move(worker_context_provider)),
      compositor_task_runner_(std::move(compositor_task_runner)),
      gpu_memory_buffer_manager_(gpu_memory_buffer_manager) {
  DETACH_FROM_THREAD(thread_checker_);
}

LayerTreeFrameSink::~LayerTreeFrameSink() {
  if (client_)
    DetachFromClient();
}

base::WeakPtr<LayerTreeFrameSink> LayerTreeFrameSink::GetWeakPtr() {
  return weak_ptr_factory_.GetWeakPtr();
}

bool LayerTreeFrameSink::BindToClient(LayerTreeFrameSinkClient* client) {
  DCHECK(client);
  DCHECK(!client_);
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);

  // Note: If |context_provider_| was always bound to a thread before here then
  // the return value could be replaced with a PostTask to OnContextLost(). This
  // would simplify the calling code so it didn't have to handle failures in
  // BindToClient().

  if (context_provider_) {
    context_provider_->AddObserver(this);
    auto result = context_provider_->BindToCurrentThread();
    if (result != gpu::ContextResult::kSuccess) {
      context_provider_->RemoveObserver(this);
      context_provider_ = nullptr;
      return false;
    }
  }

  if (worker_context_provider_) {
    DCHECK(context_provider_);
    DCHECK(compositor_task_runner_);
    DCHECK(compositor_task_runner_->BelongsToCurrentThread());
    viz::RasterContextProvider::ScopedRasterContextLock lock(
        worker_context_provider_.get());
    if (lock.RasterInterface()->GetGraphicsResetStatusKHR() != GL_NO_ERROR) {
      context_provider_->RemoveObserver(this);
      context_provider_ = nullptr;
      return false;
    }
    // Worker context lost callback is called on the main thread so it has to be
    // forwarded to compositor thread.
    worker_context_lost_forwarder_ = std::make_unique<ContextLostForwarder>(
        weak_ptr_factory_.GetWeakPtr(), compositor_task_runner_);
    worker_context_provider_->AddObserver(worker_context_lost_forwarder_.get());
  }

  client_ = client;

  return true;
}

void LayerTreeFrameSink::DetachFromClient() {
  DCHECK(client_);
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);

  client_ = nullptr;
  weak_ptr_factory_.InvalidateWeakPtrs();

  // Do not release worker context provider here as this is called on the
  // compositor thread and it must be released on the main thread. However,
  // compositor context provider must be released here.
  if (context_provider_) {
    context_provider_->RemoveObserver(this);
    context_provider_ = nullptr;
  }

  if (worker_context_provider_) {
    viz::RasterContextProvider::ScopedRasterContextLock lock(
        worker_context_provider_.get());
    worker_context_provider_->RemoveObserver(
        worker_context_lost_forwarder_.get());
    worker_context_lost_forwarder_ = nullptr;
  }
}

void LayerTreeFrameSink::OnContextLost() {
  DCHECK(client_);
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  TRACE_EVENT0("cc", "LayerTreeFrameSink::OnContextLost");
  client_->DidLoseLayerTreeFrameSink();
}

}  // namespace cc
