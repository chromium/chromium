// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CC_RASTER_STAGING_BUFFER_POOL_H_
#define CC_RASTER_STAGING_BUFFER_POOL_H_

#include <stdint.h>

#include <memory>
#include <set>

#include "base/containers/circular_deque.h"
#include "base/memory/memory_pressure_listener.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/synchronization/lock.h"
#include "base/task/sequenced_task_runner.h"
#include "base/time/time.h"
#include "base/trace_event/memory_dump_provider.h"
#include "base/trace_event/trace_event.h"
#include "cc/cc_export.h"
#include "components/viz/common/resources/shared_image_format.h"
#include "gpu/command_buffer/client/client_shared_image.h"
#include "gpu/command_buffer/common/gl2_types.h"
#include "gpu/command_buffer/common/mailbox.h"
#include "gpu/command_buffer/common/sync_token.h"
#include "ui/gfx/geometry/size.h"

namespace gpu {
namespace raster {
class RasterInterface;
}
class SharedImageInterface;
}  // namespace gpu

namespace viz {
class RasterContextProvider;
}  // namespace viz

namespace cc {

struct StagingBuffer {
  StagingBuffer(const gfx::Size& size, viz::SharedImageFormat format);
  ~StagingBuffer();

  void DestroyGLResources(gpu::raster::RasterInterface* gl,
                          gpu::SharedImageInterface* sii);
  void OnMemoryDump(base::trace_event::ProcessMemoryDump* pmd,
                    viz::SharedImageFormat dump_format,
                    bool is_free) const;

  const gfx::Size size;
  const viz::SharedImageFormat format;
  base::TimeTicks last_usage;

  // The shared image used by this StagingBuffer instance.
  scoped_refptr<gpu::ClientSharedImage> client_shared_image;

  // Sync token for the last RasterInterface operations using the shared image.
  gpu::SyncToken sync_token;

  // Id of command buffer query that tracks use of this staging buffer by the
  // GPU.  In general, GPU synchronization is necessary for native
  // GpuMemoryBuffers.
  GLuint query_id = 0;

  // Id of the content that's rastered into this staging buffer.  Used to
  // retrieve staging buffer with known content for reuse for partial raster.
  uint64_t content_id = 0;

  // Whether the underlying buffer is shared memory or GPU native.
  bool is_shared_memory = false;
};

class CC_EXPORT StagingBufferPool final
    : public base::trace_event::MemoryDumpProvider {
 public:
  StagingBufferPool(scoped_refptr<base::SequencedTaskRunner> task_runner,
                    viz::RasterContextProvider* worker_context_provider,
                    bool use_partial_raster,
                    int max_staging_buffer_usage_in_bytes);
  StagingBufferPool(const StagingBufferPool&) = delete;
  ~StagingBufferPool() final;

  StagingBufferPool& operator=(const StagingBufferPool&) = delete;

  void Shutdown();

  // Overridden from base::trace_event::MemoryDumpProvider:
  bool OnMemoryDump(const base::trace_event::MemoryDumpArgs& args,
                    base::trace_event::ProcessMemoryDump* pmd) override;

  std::unique_ptr<StagingBuffer> AcquireStagingBuffer(
      const gfx::Size& size,
      viz::SharedImageFormat format,
      uint64_t previous_content_id);
  void ReleaseStagingBuffer(std::unique_ptr<StagingBuffer> staging_buffer);

 private:
  void AddStagingBuffer(const StagingBuffer* staging_buffer,
                        viz::SharedImageFormat format)
      EXCLUSIVE_LOCKS_REQUIRED(lock_);
  void RemoveStagingBuffer(const StagingBuffer* staging_buffer)
      EXCLUSIVE_LOCKS_REQUIRED(lock_);
  void MarkStagingBufferAsFree(const StagingBuffer* staging_buffer)
      EXCLUSIVE_LOCKS_REQUIRED(lock_);
  void MarkStagingBufferAsBusy(const StagingBuffer* staging_buffer)
      EXCLUSIVE_LOCKS_REQUIRED(lock_);

  base::TimeTicks GetUsageTimeForLRUBuffer() EXCLUSIVE_LOCKS_REQUIRED(lock_);
  void ScheduleReduceMemoryUsage() EXCLUSIVE_LOCKS_REQUIRED(lock_);
  void ReduceMemoryUsage();
  void ReleaseBuffersNotUsedSince(base::TimeTicks time)
      EXCLUSIVE_LOCKS_REQUIRED(lock_);

  std::unique_ptr<base::trace_event::ConvertableToTraceFormat> StateAsValue()
      const;
  void StagingStateAsValueInto(
      base::trace_event::TracedValue* staging_state) const;

  void OnMemoryPressure(
      base::MemoryPressureListener::MemoryPressureLevel level);

  scoped_refptr<base::SequencedTaskRunner> task_runner_;
  const raw_ptr<viz::RasterContextProvider> worker_context_provider_;
  const bool use_partial_raster_;

  mutable base::Lock lock_;
  // |lock_| must be acquired when accessing the following members.
  using StagingBufferSet =
      std::set<raw_ptr<const StagingBuffer, SetExperimental>>;
  StagingBufferSet buffers_;
  using StagingBufferDeque =
      base::circular_deque<std::unique_ptr<StagingBuffer>>;
  StagingBufferDeque free_buffers_ GUARDED_BY(lock_);
  StagingBufferDeque busy_buffers_ GUARDED_BY(lock_);
  const int max_staging_buffer_usage_in_bytes_ GUARDED_BY(lock_);
  int staging_buffer_usage_in_bytes_ GUARDED_BY(lock_);
  int free_staging_buffer_usage_in_bytes_ GUARDED_BY(lock_);
  const base::TimeDelta staging_buffer_expiration_delay_ GUARDED_BY(lock_);
  bool reduce_memory_usage_pending_ GUARDED_BY(lock_);
  base::RepeatingClosure reduce_memory_usage_callback_ GUARDED_BY(lock_);

  std::unique_ptr<base::MemoryPressureListener> memory_pressure_listener_;

  base::WeakPtrFactory<StagingBufferPool> weak_ptr_factory_{this};
};

}  // namespace cc

#endif  // CC_RASTER_STAGING_BUFFER_POOL_H_
