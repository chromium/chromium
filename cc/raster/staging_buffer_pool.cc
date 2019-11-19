// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/raster/staging_buffer_pool.h"

#include <memory>

#include "base/bind.h"
#include "base/strings/stringprintf.h"
#include "base/threading/thread_task_runner_handle.h"
#include "base/trace_event/memory_dump_manager.h"
#include "cc/base/container_util.h"
#include "components/viz/common/gpu/raster_context_provider.h"
#include "components/viz/common/resources/resource_sizes.h"
#include "gpu/command_buffer/client/context_support.h"
#include "gpu/command_buffer/client/raster_interface.h"
#include "gpu/command_buffer/client/shared_image_interface.h"
#include "third_party/khronos/GLES2/gl2.h"
#include "third_party/khronos/GLES2/gl2ext.h"
#include "ui/gfx/gpu_memory_buffer.h"

using base::trace_event::MemoryAllocatorDump;
using base::trace_event::MemoryAllocatorDumpGuid;
using base::trace_event::MemoryDumpLevelOfDetail;

namespace cc {
namespace {

// Delay between checking for query result to be available.
const int kCheckForQueryResultAvailableTickRateMs = 1;

// Number of attempts to allow before we perform a check that will wait for
// query to complete.
const int kMaxCheckForQueryResultAvailableAttempts = 256;

// Delay before a staging buffer might be released.
const int kStagingBufferExpirationDelayMs = 1000;

bool CheckForQueryResult(gpu::raster::RasterInterface* ri, GLuint query_id) {
  DCHECK(query_id);
  GLuint complete = 1;
  ri->GetQueryObjectuivEXT(query_id, GL_QUERY_RESULT_AVAILABLE_EXT, &complete);
  return !!complete;
}

void WaitForQueryResult(gpu::raster::RasterInterface* ri, GLuint query_id) {
  TRACE_EVENT0("cc", "WaitForQueryResult");
  DCHECK(query_id);

  int attempts_left = kMaxCheckForQueryResultAvailableAttempts;
  while (attempts_left--) {
    if (CheckForQueryResult(ri, query_id))
      break;

    // We have to flush the context to be guaranteed that a query result will
    // be available in a finite amount of time.
    ri->ShallowFlushCHROMIUM();

    base::PlatformThread::Sleep(base::TimeDelta::FromMilliseconds(
        kCheckForQueryResultAvailableTickRateMs));
  }

  GLuint result = 0;
  ri->GetQueryObjectuivEXT(query_id, GL_QUERY_RESULT_EXT, &result);
}

}  // namespace

StagingBuffer::StagingBuffer(const gfx::Size& size, viz::ResourceFormat format)
    : size(size), format(format) {}

StagingBuffer::~StagingBuffer() {
  DCHECK(mailbox.IsZero());
  DCHECK_EQ(query_id, 0u);
}

void StagingBuffer::DestroyGLResources(gpu::raster::RasterInterface* ri,
                                       gpu::SharedImageInterface* sii) {
  if (query_id) {
    ri->DeleteQueriesEXT(1, &query_id);
    query_id = 0;
  }
  if (!mailbox.IsZero()) {
    sii->DestroySharedImage(sync_token, mailbox);
    mailbox.SetZero();
  }
}

void StagingBuffer::OnMemoryDump(base::trace_event::ProcessMemoryDump* pmd,
                                 viz::ResourceFormat dump_format,
                                 bool in_free_list) const {
  if (!gpu_memory_buffer)
    return;

  // Use |this| as the id, which works with multiple StagingBuffers.
  std::string buffer_dump_name =
      base::StringPrintf("cc/one_copy/staging_memory/buffer_%p", this);
  MemoryAllocatorDump* buffer_dump = pmd->CreateAllocatorDump(buffer_dump_name);

  uint64_t buffer_size_in_bytes =
      viz::ResourceSizes::UncheckedSizeInBytes<uint64_t>(size, dump_format);
  buffer_dump->AddScalar(MemoryAllocatorDump::kNameSize,
                         MemoryAllocatorDump::kUnitsBytes,
                         buffer_size_in_bytes);
  buffer_dump->AddScalar("free_size", MemoryAllocatorDump::kUnitsBytes,
                         in_free_list ? buffer_size_in_bytes : 0);

  // Emit an ownership edge towards a global allocator dump node.
  const uint64_t tracing_process_id =
      base::trace_event::MemoryDumpManager::GetInstance()
          ->GetTracingProcessId();
  const int kImportance = 2;
  gpu_memory_buffer->OnMemoryDump(pmd, buffer_dump->guid(), tracing_process_id,
                                  kImportance);
}

StagingBufferPool::StagingBufferPool(
    scoped_refptr<base::SequencedTaskRunner> task_runner,
    viz::RasterContextProvider* worker_context_provider,
    bool use_partial_raster,
    int max_staging_buffer_usage_in_bytes)
    : task_runner_(std::move(task_runner)),
      worker_context_provider_(worker_context_provider),
      use_partial_raster_(use_partial_raster),
      max_staging_buffer_usage_in_bytes_(max_staging_buffer_usage_in_bytes),
      staging_buffer_usage_in_bytes_(0),
      free_staging_buffer_usage_in_bytes_(0),
      staging_buffer_expiration_delay_(
          base::TimeDelta::FromMilliseconds(kStagingBufferExpirationDelayMs)),
      reduce_memory_usage_pending_(false) {
  DCHECK(worker_context_provider_);
  base::trace_event::MemoryDumpManager::GetInstance()->RegisterDumpProvider(
      this, "cc::StagingBufferPool", base::ThreadTaskRunnerHandle::Get());

  memory_pressure_listener_.reset(new base::MemoryPressureListener(
      base::BindRepeating(&StagingBufferPool::OnMemoryPressure,
                          weak_ptr_factory_.GetWeakPtr())));

  reduce_memory_usage_callback_ = base::BindRepeating(
      &StagingBufferPool::ReduceMemoryUsage, weak_ptr_factory_.GetWeakPtr());
}

StagingBufferPool::~StagingBufferPool() {
  base::trace_event::MemoryDumpManager::GetInstance()->UnregisterDumpProvider(
      this);
}

void StagingBufferPool::Shutdown() {
  base::AutoLock lock(lock_);

  if (buffers_.empty())
    return;

  ReleaseBuffersNotUsedSince(base::TimeTicks() + base::TimeDelta::Max());
  DCHECK_EQ(staging_buffer_usage_in_bytes_, 0);
  DCHECK_EQ(free_staging_buffer_usage_in_bytes_, 0);
}

void StagingBufferPool::ReleaseStagingBuffer(
    std::unique_ptr<StagingBuffer> staging_buffer) {
  base::AutoLock lock(lock_);

  staging_buffer->last_usage = base::TimeTicks::Now();
  busy_buffers_.push_back(std::move(staging_buffer));

  ScheduleReduceMemoryUsage();
}

bool StagingBufferPool::OnMemoryDump(
    const base::trace_event::MemoryDumpArgs& args,
    base::trace_event::ProcessMemoryDump* pmd) {
  base::AutoLock lock(lock_);

  if (args.level_of_detail == MemoryDumpLevelOfDetail::BACKGROUND) {
    std::string dump_name("cc/one_copy/staging_memory");
    MemoryAllocatorDump* dump = pmd->CreateAllocatorDump(dump_name);
    dump->AddScalar(MemoryAllocatorDump::kNameSize,
                    MemoryAllocatorDump::kUnitsBytes,
                    staging_buffer_usage_in_bytes_);
  } else {
    for (const auto* buffer : buffers_) {
      auto in_free_buffers =
          std::find_if(free_buffers_.begin(), free_buffers_.end(),
                       [buffer](const std::unique_ptr<StagingBuffer>& b) {
                         return b.get() == buffer;
                       });
      buffer->OnMemoryDump(pmd, buffer->format,
                           in_free_buffers != free_buffers_.end());
    }
  }
  return true;
}

void StagingBufferPool::AddStagingBuffer(const StagingBuffer* staging_buffer,
                                         viz::ResourceFormat format) {
  lock_.AssertAcquired();

  DCHECK(buffers_.find(staging_buffer) == buffers_.end());
  buffers_.insert(staging_buffer);
  int buffer_usage_in_bytes = viz::ResourceSizes::UncheckedSizeInBytes<int>(
      staging_buffer->size, format);
  staging_buffer_usage_in_bytes_ += buffer_usage_in_bytes;
}

void StagingBufferPool::RemoveStagingBuffer(
    const StagingBuffer* staging_buffer) {
  lock_.AssertAcquired();

  DCHECK(buffers_.find(staging_buffer) != buffers_.end());
  buffers_.erase(staging_buffer);
  int buffer_usage_in_bytes = viz::ResourceSizes::UncheckedSizeInBytes<int>(
      staging_buffer->size, staging_buffer->format);
  DCHECK_GE(staging_buffer_usage_in_bytes_, buffer_usage_in_bytes);
  staging_buffer_usage_in_bytes_ -= buffer_usage_in_bytes;
}

void StagingBufferPool::MarkStagingBufferAsFree(
    const StagingBuffer* staging_buffer) {
  lock_.AssertAcquired();

  int buffer_usage_in_bytes = viz::ResourceSizes::UncheckedSizeInBytes<int>(
      staging_buffer->size, staging_buffer->format);
  free_staging_buffer_usage_in_bytes_ += buffer_usage_in_bytes;
}

void StagingBufferPool::MarkStagingBufferAsBusy(
    const StagingBuffer* staging_buffer) {
  lock_.AssertAcquired();

  int buffer_usage_in_bytes = viz::ResourceSizes::UncheckedSizeInBytes<int>(
      staging_buffer->size, staging_buffer->format);
  DCHECK_GE(free_staging_buffer_usage_in_bytes_, buffer_usage_in_bytes);
  free_staging_buffer_usage_in_bytes_ -= buffer_usage_in_bytes;
}

std::unique_ptr<StagingBuffer> StagingBufferPool::AcquireStagingBuffer(
    const gfx::Size& size,
    viz::ResourceFormat format,
    uint64_t previous_content_id) {
  base::AutoLock lock(lock_);

  std::unique_ptr<StagingBuffer> staging_buffer;

  viz::RasterContextProvider::ScopedRasterContextLock scoped_context(
      worker_context_provider_);

  gpu::raster::RasterInterface* ri = scoped_context.RasterInterface();
  gpu::SharedImageInterface* sii =
      worker_context_provider_->SharedImageInterface();
  DCHECK(ri);

  // Check if any busy buffers have become available.
  while (!busy_buffers_.empty()) {
    // Early out if query isn't used, or if query isn't complete yet.  Query is
    // created in OneCopyRasterBufferProvider::CopyOnWorkerThread().
    if (!busy_buffers_.front()->query_id ||
        !CheckForQueryResult(ri, busy_buffers_.front()->query_id))
      break;

    MarkStagingBufferAsFree(busy_buffers_.front().get());
    free_buffers_.push_back(PopFront(&busy_buffers_));
  }

  // Wait for memory usage of non-free buffers to become less than the limit.
  while (
      (staging_buffer_usage_in_bytes_ - free_staging_buffer_usage_in_bytes_) >=
      max_staging_buffer_usage_in_bytes_) {
    // Stop when there are no more busy buffers to wait for.
    if (busy_buffers_.empty())
      break;

    if (busy_buffers_.front()->query_id) {
      WaitForQueryResult(ri, busy_buffers_.front()->query_id);
      MarkStagingBufferAsFree(busy_buffers_.front().get());
      free_buffers_.push_back(PopFront(&busy_buffers_));
    } else {
      // Fall back to glFinish if query isn't used.
      ri->Finish();
      while (!busy_buffers_.empty()) {
        MarkStagingBufferAsFree(busy_buffers_.front().get());
        free_buffers_.push_back(PopFront(&busy_buffers_));
      }
    }
  }

  // Find a staging buffer that allows us to perform partial raster when
  // using persistent GpuMemoryBuffers.
  if (use_partial_raster_ && previous_content_id) {
    StagingBufferDeque::iterator it = std::find_if(
        free_buffers_.begin(), free_buffers_.end(),
        [previous_content_id](const std::unique_ptr<StagingBuffer>& buffer) {
          return buffer->content_id == previous_content_id;
        });
    if (it != free_buffers_.end()) {
      staging_buffer = std::move(*it);
      free_buffers_.erase(it);
      MarkStagingBufferAsBusy(staging_buffer.get());
    }
  }

  // Find staging buffer of correct size and format.
  if (!staging_buffer) {
    StagingBufferDeque::iterator it = std::find_if(
        free_buffers_.begin(), free_buffers_.end(),
        [&size, format](const std::unique_ptr<StagingBuffer>& buffer) {
          return buffer->size == size && buffer->format == format;
        });
    if (it != free_buffers_.end()) {
      staging_buffer = std::move(*it);
      free_buffers_.erase(it);
      MarkStagingBufferAsBusy(staging_buffer.get());
    }
  }

  // Create new staging buffer if necessary.
  if (!staging_buffer) {
    staging_buffer = std::make_unique<StagingBuffer>(size, format);
    AddStagingBuffer(staging_buffer.get(), format);
  }

  // Release enough free buffers to stay within the limit.
  while (staging_buffer_usage_in_bytes_ > max_staging_buffer_usage_in_bytes_) {
    if (free_buffers_.empty())
      break;

    free_buffers_.front()->DestroyGLResources(ri, sii);
    MarkStagingBufferAsBusy(free_buffers_.front().get());
    RemoveStagingBuffer(free_buffers_.front().get());
    free_buffers_.pop_front();
  }

  return staging_buffer;
}

base::TimeTicks StagingBufferPool::GetUsageTimeForLRUBuffer() {
  lock_.AssertAcquired();

  if (!free_buffers_.empty())
    return free_buffers_.front()->last_usage;

  if (!busy_buffers_.empty())
    return busy_buffers_.front()->last_usage;

  return base::TimeTicks();
}

void StagingBufferPool::ScheduleReduceMemoryUsage() {
  lock_.AssertAcquired();

  if (reduce_memory_usage_pending_)
    return;

  reduce_memory_usage_pending_ = true;

  // Schedule a call to ReduceMemoryUsage at the time when the LRU buffer
  // should be released.
  base::TimeTicks reduce_memory_usage_time =
      GetUsageTimeForLRUBuffer() + staging_buffer_expiration_delay_;
  task_runner_->PostDelayedTask(
      FROM_HERE, reduce_memory_usage_callback_,
      reduce_memory_usage_time - base::TimeTicks::Now());
}

void StagingBufferPool::ReduceMemoryUsage() {
  base::AutoLock lock(lock_);

  reduce_memory_usage_pending_ = false;

  if (free_buffers_.empty() && busy_buffers_.empty())
    return;

  base::TimeTicks current_time = base::TimeTicks::Now();
  ReleaseBuffersNotUsedSince(current_time - staging_buffer_expiration_delay_);

  if (free_buffers_.empty() && busy_buffers_.empty())
    return;

  reduce_memory_usage_pending_ = true;

  // Schedule another call to ReduceMemoryUsage at the time when the next
  // buffer should be released.
  base::TimeTicks reduce_memory_usage_time =
      GetUsageTimeForLRUBuffer() + staging_buffer_expiration_delay_;
  task_runner_->PostDelayedTask(FROM_HERE, reduce_memory_usage_callback_,
                                reduce_memory_usage_time - current_time);
}

void StagingBufferPool::ReleaseBuffersNotUsedSince(base::TimeTicks time) {
  lock_.AssertAcquired();

  {
    viz::RasterContextProvider::ScopedRasterContextLock scoped_context(
        worker_context_provider_);

    gpu::raster::RasterInterface* ri = scoped_context.RasterInterface();
    DCHECK(ri);
    gpu::SharedImageInterface* sii =
        worker_context_provider_->SharedImageInterface();
    DCHECK(sii);

    bool destroyed_buffers = false;
    // Note: Front buffer is guaranteed to be LRU so we can stop releasing
    // buffers as soon as we find a buffer that has been used since |time|.
    while (!free_buffers_.empty()) {
      if (free_buffers_.front()->last_usage > time)
        break;

      destroyed_buffers = true;
      free_buffers_.front()->DestroyGLResources(ri, sii);
      MarkStagingBufferAsBusy(free_buffers_.front().get());
      RemoveStagingBuffer(free_buffers_.front().get());
      free_buffers_.pop_front();
    }

    while (!busy_buffers_.empty()) {
      if (busy_buffers_.front()->last_usage > time)
        break;

      destroyed_buffers = true;
      busy_buffers_.front()->DestroyGLResources(ri, sii);
      RemoveStagingBuffer(busy_buffers_.front().get());
      busy_buffers_.pop_front();
    }

    if (destroyed_buffers) {
      ri->OrderingBarrierCHROMIUM();
      worker_context_provider_->ContextSupport()->FlushPendingWork();
    }
  }
}

void StagingBufferPool::OnMemoryPressure(
    base::MemoryPressureListener::MemoryPressureLevel level) {
  base::AutoLock lock(lock_);
  switch (level) {
    case base::MemoryPressureListener::MEMORY_PRESSURE_LEVEL_NONE:
    case base::MemoryPressureListener::MEMORY_PRESSURE_LEVEL_MODERATE:
      break;
    case base::MemoryPressureListener::MEMORY_PRESSURE_LEVEL_CRITICAL:
      // Release all buffers, regardless of how recently they were used.
      ReleaseBuffersNotUsedSince(base::TimeTicks() + base::TimeDelta::Max());
      break;
  }
}

}  // namespace cc
