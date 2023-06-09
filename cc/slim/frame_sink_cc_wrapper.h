// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CC_SLIM_FRAME_SINK_CC_WRAPPER_H_
#define CC_SLIM_FRAME_SINK_CC_WRAPPER_H_

#include <memory>

#include "base/memory/scoped_refptr.h"
#include "base/task/single_thread_task_runner.h"
#include "cc/slim/frame_sink.h"
#include "components/viz/common/gpu/raster_context_provider.h"
#include "services/viz/public/mojom/compositing/compositor_frame_sink.mojom.h"

namespace cc::mojo_embedder {
class AsyncLayerTreeFrameSink;
}

namespace cc::slim {

class LayerTreeCcWrapper;

class FrameSinkCcWrapper : public FrameSink {
 public:
  ~FrameSinkCcWrapper() override;

 private:
  friend class FrameSink;
  friend class LayerTreeCcWrapper;

  FrameSinkCcWrapper(
      scoped_refptr<base::SingleThreadTaskRunner> task_runner,
      mojo::PendingAssociatedRemote<viz::mojom::CompositorFrameSink>
          compositor_frame_sink_associated_remote,
      mojo::PendingReceiver<viz::mojom::CompositorFrameSinkClient>
          client_receiver,
      scoped_refptr<viz::RasterContextProvider> context_provider,
      gpu::GpuMemoryBufferManager* gpu_memory_buffer_manager,
      base::PlatformThreadId io_thread_id);

  std::unique_ptr<cc::mojo_embedder::AsyncLayerTreeFrameSink> cc_frame_sink_;
};

}  // namespace cc::slim

#endif  // CC_SLIM_FRAME_SINK_CC_WRAPPER_H_
