// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/slim/frame_sink.h"

#include <utility>

#include "base/feature_list.h"
#include "base/memory/ptr_util.h"
#include "cc/slim/delayed_scheduler.h"
#include "cc/slim/frame_sink_impl.h"
#include "cc/slim/simple_scheduler.h"
#include "components/viz/common/features.h"

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
#if BUILDFLAG(IS_ANDROID)
  if (base::FeatureList::IsEnabled(features::kAndroidBcivWithSimpleScheduler)) {
    return base::WrapUnique<FrameSink>(new FrameSinkImpl(
        std::move(task_runner),
        std::move(compositor_frame_sink_associated_remote),
        std::move(client_receiver), std::move(context_provider), io_thread_id,
        std::make_unique<SimpleScheduler>()));
  }
#endif
  return base::WrapUnique<FrameSink>(
      new FrameSinkImpl(std::move(task_runner),
                        std::move(compositor_frame_sink_associated_remote),
                        std::move(client_receiver), std::move(context_provider),
                        io_thread_id, std::make_unique<DelayedScheduler>()));
}

}  // namespace cc::slim
