// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/trees/layer_tree_frame_sink.h"

#include <stdint.h>

#include <utility>

#include "base/feature_list.h"
#include "base/functional/bind.h"
#include "base/location.h"
#include "base/task/bind_post_task.h"
#include "base/task/single_thread_task_runner.h"
#include "base/trace_event/trace_event.h"
#include "cc/trees/layer_context.h"
#include "cc/trees/layer_tree_frame_sink_client.h"
#include "components/viz/common/features.h"
#include "components/viz/common/gpu/context_lost_observer.h"
#include "components/viz/common/gpu/raster_context_provider.h"
#include "gpu/GLES2/gl2extchromium.h"
#include "gpu/command_buffer/client/context_support.h"
#include "gpu/command_buffer/client/raster_interface.h"
#include "gpu/ipc/client/client_shared_image_interface.h"
#include "gpu/ipc/client/gpu_channel_host.h"

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

LayerTreeFrameSink::LayerTreeFrameSink()
    : LayerTreeFrameSink(nullptr, nullptr, nullptr, nullptr, nullptr) {}

LayerTreeFrameSink::LayerTreeFrameSink(
    scoped_refptr<viz::RasterContextProvider> context_provider,
    scoped_refptr<RasterContextProviderWrapper> worker_context_provider_wrapper,
    scoped_refptr<base::SingleThreadTaskRunner> compositor_task_runner,
    gpu::GpuMemoryBufferManager* gpu_memory_buffer_manager,
    scoped_refptr<gpu::ClientSharedImageInterface> shared_image_interface)
    : context_provider_(std::move(context_provider)),
      worker_context_provider_wrapper_(
          std::move(worker_context_provider_wrapper)),
      compositor_task_runner_(std::move(compositor_task_runner)),
      gpu_memory_buffer_manager_(gpu_memory_buffer_manager),
      shared_image_interface_(std::move(shared_image_interface)) {
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
    auto result = context_provider_->BindToCurrentSequence();
    if (result != gpu::ContextResult::kSuccess) {
      context_provider_->RemoveObserver(this);
      context_provider_ = nullptr;
      return false;
    }
  }

  if (auto* worker_context_provider_ptr = worker_context_provider()) {
    DCHECK(context_provider_);
    DCHECK(compositor_task_runner_);
    DCHECK(compositor_task_runner_->BelongsToCurrentThread());
    viz::RasterContextProvider::ScopedRasterContextLock lock(
        worker_context_provider_ptr);
    if (lock.RasterInterface()->GetGraphicsResetStatusKHR() != GL_NO_ERROR) {
      context_provider_->RemoveObserver(this);
      context_provider_ = nullptr;
      return false;
    }
    // Worker context lost callback is called on the main thread so it has to be
    // forwarded to compositor thread.
    worker_context_lost_forwarder_ = std::make_unique<ContextLostForwarder>(
        weak_ptr_factory_.GetWeakPtr(), compositor_task_runner_);
    worker_context_provider_ptr->AddObserver(
        worker_context_lost_forwarder_.get());
  }

  // Add GpuChannelLost observer when in software rendering mode.
  if (shared_image_interface_ &&
      (!context_provider_ && !worker_context_provider())) {
    task_gpu_channel_lost_on_client_thread_ =
        base::BindPostTaskToCurrentDefault(base::BindOnce(
            &LayerTreeFrameSink::GpuChannelLostOnClientThread, GetWeakPtr()));
    shared_image_interface_->gpu_channel()->AddObserver(this);
  }

  client_ = client;

  return true;
}

void LayerTreeFrameSink::DetachFromClient() {
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
  if (worker_context_lost_forwarder_) {
    auto* worker_context_provider_ptr = worker_context_provider();
    CHECK(worker_context_provider_ptr);
    viz::RasterContextProvider::ScopedRasterContextLock lock(
        worker_context_provider_ptr);
    worker_context_provider_ptr->RemoveObserver(
        worker_context_lost_forwarder_.get());
    worker_context_lost_forwarder_ = nullptr;
  }
  if (shared_image_interface_) {
    if (task_gpu_channel_lost_on_client_thread_) {
      shared_image_interface_->gpu_channel()->RemoveObserver(this);
    }
    shared_image_interface_.reset();
  }
}

std::unique_ptr<LayerContext> LayerTreeFrameSink::CreateLayerContext(
    LayerTreeHostImpl& host_impl) {
  return nullptr;
}

void LayerTreeFrameSink::OnContextLost() {
  DCHECK(client_);
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  TRACE_EVENT0("cc", "LayerTreeFrameSink::OnContextLost");
  client_->DidLoseLayerTreeFrameSink();
}

void LayerTreeFrameSink::OnGpuChannelLost() {
  // OnGpuChannelLost() is called on the IOThread. so it has to be forwareded
  // to LayerTreeFrameSink::OnGpuChannelLostClientThread(), which is on the same
  // thread where BindToClient is called, either the BrowserMain thread or the
  // compositor thread.
  if (task_gpu_channel_lost_on_client_thread_) {
    std::move(task_gpu_channel_lost_on_client_thread_).Run();
  }
}
void LayerTreeFrameSink::GpuChannelLostOnClientThread() {
  // No need to RemoveObserver(). The Observable removes all observers
  // after completing GpuChannelLost notification.
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  shared_image_interface_.reset();

  DCHECK(client_);
  client_->DidLoseLayerTreeFrameSink();
}

scoped_refptr<gpu::ClientSharedImageInterface>
LayerTreeFrameSink::shared_image_interface() const {
  return shared_image_interface_;
}

}  // namespace cc
