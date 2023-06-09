// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CC_SLIM_FRAME_SINK_H_
#define CC_SLIM_FRAME_SINK_H_

#include <memory>

#include "base/component_export.h"
#include "base/memory/scoped_refptr.h"
#include "base/task/single_thread_task_runner.h"
#include "base/threading/platform_thread.h"
#include "components/viz/common/gpu/raster_context_provider.h"
#include "services/viz/public/mojom/compositing/compositor_frame_sink.mojom.h"

namespace gpu {
class GpuMemoryBufferManager;
}

namespace cc::slim {

// Abstraction and ownership over connections to the GPU process:
// `viz::mojom::CompositorFrameSink` and `viz::RasterContextProvider`.
// Client needs to create this when requested by `LayerTree`.
class COMPONENT_EXPORT(CC_SLIM) FrameSink {
 public:
  static std::unique_ptr<FrameSink> Create(
      mojo::PendingAssociatedRemote<viz::mojom::CompositorFrameSink>
          compositor_frame_sink_associated_remote,
      mojo::PendingReceiver<viz::mojom::CompositorFrameSinkClient>
          client_receiver,
      scoped_refptr<viz::RasterContextProvider> context_provider,
      scoped_refptr<base::SingleThreadTaskRunner> task_runner,
      // Parameters below only used when wrapping cc.
      gpu::GpuMemoryBufferManager* gpu_memory_buffer_manager,
      base::PlatformThreadId io_thread_id);

  virtual ~FrameSink() = default;
};

}  // namespace cc::slim

#endif  // CC_SLIM_FRAME_SINK_H_
