// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/slim/frame_sink_cc_wrapper.h"

#include <utility>

#include "cc/mojo_embedder/async_layer_tree_frame_sink.h"

namespace cc::slim {

FrameSinkCcWrapper::FrameSinkCcWrapper(
    scoped_refptr<base::SingleThreadTaskRunner> task_runner,
    mojo::PendingAssociatedRemote<viz::mojom::CompositorFrameSink>
        compositor_frame_sink_associated_remote,
    mojo::PendingReceiver<viz::mojom::CompositorFrameSinkClient>
        client_receiver,
    scoped_refptr<viz::ContextProvider> context_provider,
    gpu::GpuMemoryBufferManager* gpu_memory_buffer_manager,
    base::PlatformThreadId io_thread_id) {
  cc::mojo_embedder::AsyncLayerTreeFrameSink::InitParams params;
  params.compositor_task_runner = std::move(task_runner);
  params.gpu_memory_buffer_manager = gpu_memory_buffer_manager;
  params.pipes.compositor_frame_sink_associated_remote =
      std::move(compositor_frame_sink_associated_remote);
  params.pipes.client_receiver = std::move(client_receiver);
  params.io_thread_id = io_thread_id;
  cc_frame_sink_ = std::make_unique<cc::mojo_embedder::AsyncLayerTreeFrameSink>(
      std::move(context_provider), nullptr, &params);
}

FrameSinkCcWrapper::~FrameSinkCcWrapper() = default;

}  // namespace cc::slim
