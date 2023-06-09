// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/slim/frame_sink.h"

#include <utility>

#include "base/memory/ptr_util.h"
#include "cc/slim/delayed_scheduler.h"
#include "cc/slim/features.h"
#include "cc/slim/frame_sink_cc_wrapper.h"
#include "cc/slim/frame_sink_impl.h"

namespace cc::slim {

// static
std::unique_ptr<FrameSink> FrameSink::Create(
    mojo::PendingAssociatedRemote<viz::mojom::CompositorFrameSink>
        compositor_frame_sink_associated_remote,
    mojo::PendingReceiver<viz::mojom::CompositorFrameSinkClient>
        client_receiver,
    scoped_refptr<viz::RasterContextProvider> context_provider,
    scoped_refptr<base::SingleThreadTaskRunner> task_runner,
    // Parameters below only used when wrapping cc.
    gpu::GpuMemoryBufferManager* gpu_memory_buffer_manager,
    base::PlatformThreadId io_thread_id) {
  if (!features::IsSlimCompositorEnabled()) {
    return base::WrapUnique<FrameSink>(new FrameSinkCcWrapper(
        std::move(task_runner),
        std::move(compositor_frame_sink_associated_remote),
        std::move(client_receiver), std::move(context_provider),
        gpu_memory_buffer_manager, io_thread_id));
  }
  return base::WrapUnique<FrameSink>(
      new FrameSinkImpl(std::move(task_runner),
                        std::move(compositor_frame_sink_associated_remote),
                        std::move(client_receiver), std::move(context_provider),
                        io_thread_id, std::make_unique<DelayedScheduler>()));
}

}  // namespace cc::slim
