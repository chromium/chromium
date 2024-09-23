// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/resources/resource_pool.h"

#include <stddef.h>
#include <stdint.h>

#include <algorithm>
#include <limits>
#include <memory>
#include <string>
#include <utility>

#include "base/atomic_sequence_num.h"
#include "base/containers/contains.h"
#include "base/format_macros.h"
#include "base/functional/bind.h"
#include "base/not_fatal_until.h"
#include "base/ranges/algorithm.h"
#include "base/strings/stringprintf.h"
#include "base/task/single_thread_task_runner.h"
#include "base/time/default_tick_clock.h"
#include "base/trace_event/memory_dump_manager.h"
#include "build/build_config.h"
#include "cc/base/container_util.h"
#include "components/viz/client/client_resource_provider.h"
#include "components/viz/common/gpu/raster_context_provider.h"
#include "components/viz/common/resources/shared_image_format_utils.h"
#include "gpu/command_buffer/client/client_shared_image.h"
#include "gpu/command_buffer/client/context_support.h"
#include "gpu/command_buffer/common/capabilities.h"
#include "gpu/command_buffer/common/mailbox.h"

using base::trace_event::MemoryAllocatorDump;
using base::trace_event::MemoryDumpLevelOfDetail;

namespace cc {

ResourcePool::GpuBacking::GpuBacking() = default;
ResourcePool::GpuBacking::~GpuBacking() = default;

ResourcePool::SoftwareBacking::SoftwareBacking() = default;
ResourcePool::SoftwareBacking::~SoftwareBacking() = default;

namespace {

// Process-unique number for each resource pool.
base::AtomicSequenceNumber g_next_tracing_id;

bool ResourceMeetsSizeRequirements(const gfx::Size& requested_size,
                                   const gfx::Size& actual_size,
                                   bool disallow_non_exact_reuse) {
  const float kReuseThreshold = 2.0f;

  if (disallow_non_exact_reuse)
    return requested_size == actual_size;

  // Allocating new resources is expensive, and we'd like to re-use our
  // existing ones within reason. Allow a larger resource to be used for a
  // smaller request.
  if (actual_size.width() < requested_size.width() ||
      actual_size.height() < requested_size.height())
    return false;

  // GetArea will crash on overflow, however all sizes in use are tile sizes.
  // These are capped at viz::ClientResourceProvider::max_texture_size(), and
  // will not overflow.
  float actual_area = actual_size.GetArea();
  float requested_area = requested_size.GetArea();
  // Don't use a resource that is more than |kReuseThreshold| times the
  // requested pixel area, as we want to free unnecessarily large resources.
  if (actual_area / requested_area > kReuseThreshold)
    return false;

  return true;
}

}  // namespace

constexpr base::TimeDelta ResourcePool::kDefaultExpirationDelay;
constexpr base::TimeDelta ResourcePool::kDefaultMaxFlushDelay;

ResourcePool::ResourcePool(
    viz::ClientResourceProvider* resource_provider,
    viz::RasterContextProvider* context_provider,
    scoped_refptr<base::SingleThreadTaskRunner> task_runner,
    const base::TimeDelta& expiration_delay,
    bool disallow_non_exact_reuse)
    : resource_provider_(resource_provider),
      context_provider_(context_provider),
      task_runner_(std::move(task_runner)),
      resource_expiration_delay_(expiration_delay),
      disallow_non_exact_reuse_(disallow_non_exact_reuse),
      tracing_id_(g_next_tracing_id.GetNext()),
      flush_evicted_resources_deadline_(base::TimeTicks::Max()),
      clock_(base::DefaultTickClock::GetInstance()) {
  base::trace_event::MemoryDumpManager::GetInstance()->RegisterDumpProvider(
      this, "cc::ResourcePool", task_runner_.get());
  memory_pressure_listener_ = std::make_unique<base::MemoryPressureListener>(
      FROM_HERE, base::BindRepeating(&ResourcePool::OnMemoryPressure,
                                     weak_ptr_factory_.GetWeakPtr()));
}

ResourcePool::~ResourcePool() {
  base::trace_event::MemoryDumpManager::GetInstance()->UnregisterDumpProvider(
      this);

  DCHECK_EQ(0u, in_use_resources_.size());

  while (!busy_resources_.empty()) {
    DidFinishUsingResource(PopBack(&busy_resources_));
  }

  SetResourceUsageLimits(0, 0);
  DCHECK_EQ(0u, unused_resources_.size());
  DCHECK_EQ(0u, unused_memory_usage_bytes_);
  DCHECK_EQ(0u, total_memory_usage_bytes_);
  DCHECK_EQ(0u, total_resource_count_);
}

ResourcePool::PoolResource* ResourcePool::ReuseResource(
    const gfx::Size& size,
    viz::SharedImageFormat format,
    const gfx::ColorSpace& color_space) {
  // Finding resources in |unused_resources_| from MRU to LRU direction, touches
  // LRU resources only if needed, which increases possibility of expiring more
  // LRU resources within kResourceExpirationDelayMs.
  for (auto it = unused_resources_.begin(); it != unused_resources_.end();
       ++it) {
    PoolResource* resource = it->get();
    DCHECK(!resource->resource_id());

    if (resource->format() != format) {
      continue;
    }
    if (!ResourceMeetsSizeRequirements(size, resource->size(),
                                       disallow_non_exact_reuse_))
      continue;
    if (resource->color_space() != color_space)
      continue;

    // Transfer resource to |in_use_resources_|.
    in_use_resources_[resource->unique_id()] = std::move(*it);
    unused_resources_.erase(it);
    DCHECK_GE(unused_memory_usage_bytes_, resource->memory_usage());
    unused_memory_usage_bytes_ -= resource->memory_usage();
    DCHECK_EQ(resource->state(), PoolResource::kUnused);
    resource->set_state(PoolResource::kInUse);
    return resource;
  }
  return nullptr;
}

ResourcePool::PoolResource* ResourcePool::CreateResource(
    const gfx::Size& size,
    viz::SharedImageFormat format,
    const gfx::ColorSpace& color_space) {
  DCHECK(format.VerifySizeInBytes(size));

  auto pool_resource = std::make_unique<PoolResource>(
      this, next_resource_unique_id_++, size, format, color_space);

  // No backing, the memory_usage() should be 0.
  DCHECK_EQ(pool_resource->memory_usage(), 0u);
  ++total_resource_count_;

  PoolResource* resource = pool_resource.get();
  in_use_resources_[resource->unique_id()] = std::move(pool_resource);
  resource->set_state(PoolResource::kInUse);

  return resource;
}

ResourcePool::InUsePoolResource ResourcePool::AcquireResource(
    const gfx::Size& size,
    viz::SharedImageFormat format,
    const gfx::ColorSpace& color_space,
    const std::string& debug_name) {
  PoolResource* resource = ReuseResource(size, format, color_space);
  if (!resource)
    resource = CreateResource(size, format, color_space);
  resource->set_debug_name(debug_name);
  return InUsePoolResource(resource, !!context_provider_);
}

// Iterate over all three resource lists (unused, in-use, and busy), updating
// the invalidation and content IDs to allow for future partial raster. The
// first unused resource found (if any) will be returned and used for partial
// raster directly.
//
// Note that this may cause us to have multiple resources with the same content
// ID. This is not a correctness risk, as all these resources will have valid
// invalidations can can be used safely. Note that we could improve raster
// performance at the cost of search time if we found the resource with the
// smallest invalidation ID to raster in to.
ResourcePool::InUsePoolResource
ResourcePool::TryAcquireResourceForPartialRaster(
    uint64_t new_content_id,
    const gfx::Rect& new_invalidated_rect,
    uint64_t previous_content_id,
    gfx::Rect* total_invalidated_rect,
    const gfx::ColorSpace& raster_color_space,
    const std::string& debug_name) {
  DCHECK(new_content_id);
  DCHECK(previous_content_id);
  *total_invalidated_rect = gfx::Rect();

  auto iter_resource_to_return = unused_resources_.end();
  int minimum_area = 0;

  // First update all unused resources. While updating, track the resource with
  // the smallest invalidation. That resource will be returned to the caller.
  for (auto it = unused_resources_.begin(); it != unused_resources_.end();
       ++it) {
    PoolResource* resource = it->get();

    if (resource->content_id() == previous_content_id) {
      // Skip the old resource if color space changed.
      if (resource->color_space() != raster_color_space)
        continue;

      UpdateResourceContentIdAndInvalidation(resource, new_content_id,
                                             new_invalidated_rect);

      // Return the resource with the smallest invalidation.
      int area =
          resource->invalidated_rect().size().GetCheckedArea().ValueOrDefault(
              std::numeric_limits<int>::max());
      if (iter_resource_to_return == unused_resources_.end() ||
          area < minimum_area) {
        iter_resource_to_return = it;
        minimum_area = area;
      }
    }
  }

  // Next, update all busy and in_use resources.
  for (const auto& resource : busy_resources_) {
    if (resource->content_id() == previous_content_id) {
      UpdateResourceContentIdAndInvalidation(resource.get(), new_content_id,
                                             new_invalidated_rect);
    }
  }
  for (const auto& resource_pair : in_use_resources_) {
    PoolResource* resource = resource_pair.second.get();
    if (resource->content_id() == previous_content_id) {
      UpdateResourceContentIdAndInvalidation(resource, new_content_id,
                                             new_invalidated_rect);
    }
  }

  // If we found an unused resource to return earlier, move it to
  // |in_use_resources_| and return it.
  if (iter_resource_to_return != unused_resources_.end()) {
    PoolResource* resource = iter_resource_to_return->get();
    DCHECK(!resource->resource_id());

    // Transfer resource to |in_use_resources_|.
    resource->set_state(PoolResource::kInUse);
    in_use_resources_[resource->unique_id()] =
        std::move(*iter_resource_to_return);
    unused_resources_.erase(iter_resource_to_return);
    DCHECK_GE(unused_memory_usage_bytes_, resource->memory_usage());
    unused_memory_usage_bytes_ -= resource->memory_usage();
    *total_invalidated_rect = resource->invalidated_rect();

    // Clear the invalidated rect and content ID on the resource being returned.
    // These will be updated when raster completes successfully.
    resource->set_invalidated_rect(gfx::Rect());
    resource->set_content_id(0);
    resource->set_debug_name(debug_name);
    return InUsePoolResource(resource, !!context_provider_);
  }

  return InUsePoolResource();
}

void ResourcePool::OnBackingAllocated(PoolResource* resource) {
  size_t size = resource->memory_usage();
  total_memory_usage_bytes_ += size;
  if (resource->state() == PoolResource::kUnused)
    unused_memory_usage_bytes_ += size;
}

void ResourcePool::OnResourceReleased(size_t unique_id,
                                      const gpu::SyncToken& sync_token,
                                      bool lost) {
  // If this fails we've removed a resource from the ResourceProvider somehow
  // while it was still in use by the ResourcePool client. That would prevent
  // the client from being able to use the ResourceId on the InUsePoolResource,
  // which would be problematic!
  DCHECK(!base::Contains(in_use_resources_, unique_id));

  // TODO(danakj): Should busy_resources be a map?
  auto busy_it =
      base::ranges::find(busy_resources_, unique_id, &PoolResource::unique_id);
  // If the resource isn't busy then we made it available for reuse already
  // somehow, even though it was exported to the ResourceProvider, or we evicted
  // a resource that was still in use by the display compositor.
  CHECK(busy_it != busy_resources_.end(), base::NotFatalUntil::M130);

  PoolResource* resource = busy_it->get();
  resource->set_state(PoolResource::kUnused);
  if (lost || evict_busy_resources_when_unused_ || resource->avoid_reuse()) {
    DeleteResource(std::move(*busy_it));
    busy_resources_.erase(busy_it);
    return;
  }

  resource->set_resource_id(viz::kInvalidResourceId);
  if (context_provider_)
    resource->gpu_backing()->returned_sync_token = sync_token;
  DidFinishUsingResource(std::move(*busy_it));
  busy_resources_.erase(busy_it);
}

bool ResourcePool::PrepareForExport(
    const InUsePoolResource& in_use_resource,
    viz::TransferableResource::ResourceSource resource_source) {
  PoolResource* resource = in_use_resource.resource_;
  // Exactly one of gpu or software backing should exist.
  DCHECK(resource->gpu_backing() || resource->software_backing());
  DCHECK(!resource->gpu_backing() || !resource->software_backing());
  viz::TransferableResource transferable;
  if (resource->gpu_backing()) {
    GpuBacking* gpu_backing = resource->gpu_backing();
    if (!gpu_backing->shared_image) {
      // This can happen if we failed to allocate a GpuMemoryBuffer. Avoid
      // sending an invalid resource to the parent in that case, and avoid
      // caching/reusing the resource.
      resource->set_resource_id(viz::kInvalidResourceId);
      resource->mark_avoid_reuse();
      return false;
    }
    uint32_t texture_target = gpu_backing->shared_image->GetTextureTarget();
    transferable = viz::TransferableResource::MakeGpu(
        gpu_backing->shared_image->mailbox(), texture_target,
        gpu_backing->mailbox_sync_token, resource->size(), resource->format(),
        gpu_backing->overlay_candidate, resource_source);
    if (gpu_backing->wait_on_fence_required)
      transferable.synchronization_type =
          viz::TransferableResource::SynchronizationType::kGpuCommandsCompleted;
  } else {
    SoftwareBacking* software_backing = resource->software_backing();
    transferable =
        software_backing->shared_image
            ? viz::TransferableResource::MakeSoftwareSharedImage(
                  software_backing->shared_image,
                  software_backing->mailbox_sync_token, resource->size(),
                  resource->format(), resource_source)
            : viz::TransferableResource::MakeSoftwareSharedBitmap(
                  software_backing->shared_bitmap_id,
                  software_backing->mailbox_sync_token, resource->size(),
                  resource->format(), resource_source);
  }
  transferable.color_space = resource->color_space();
  resource->set_resource_id(resource_provider_->ImportResource(
      std::move(transferable),
      base::BindOnce(&ResourcePool::OnResourceReleased,
                     weak_ptr_factory_.GetWeakPtr(), resource->unique_id())));
  return true;
}

void ResourcePool::InvalidateResources() {
  while (!unused_resources_.empty()) {
    DCHECK_GE(unused_memory_usage_bytes_,
              unused_resources_.back()->memory_usage());
    unused_memory_usage_bytes_ -= unused_resources_.back()->memory_usage();
    DeleteResource(PopBack(&unused_resources_));
  }
  DCHECK_EQ(unused_memory_usage_bytes_, 0U);

  for (auto& pool_resource : busy_resources_)
    pool_resource->mark_avoid_reuse();
  for (auto& pair : in_use_resources_)
    pair.second->mark_avoid_reuse();
}

void ResourcePool::ReleaseResource(InUsePoolResource in_use_resource) {
  PoolResource* pool_resource = in_use_resource.resource_;
  in_use_resource.SetWasFreedByResourcePool();

  DCHECK_EQ(pool_resource->state(), PoolResource::kInUse);
  // Ensure that the provided resource is valid.
  // TODO(ericrk): Remove this once we've investigated further.
  // crbug.com/598286.
  CHECK(pool_resource);

  auto it = in_use_resources_.find(pool_resource->unique_id());
  if (it == in_use_resources_.end()) {
    // We should never hit this. Do some digging to try to determine the cause.
    // TODO(ericrk): Remove this once we've investigated further.
    // crbug.com/598286.

    // Maybe this is a double free - see if the resource exists in our busy
    // list.
    CHECK(!base::Contains(busy_resources_, pool_resource->unique_id(),
                          &PoolResource::unique_id));

    // Also check if the resource exists in our unused resources list.
    CHECK(!base::Contains(unused_resources_, pool_resource->unique_id(),
                          &PoolResource::unique_id));

    // Resource doesn't exist in any of our lists. CHECK.
    CHECK(false);
  }

  // Also ensure that the resource wasn't null in our list.
  // TODO(ericrk): Remove this once we've investigated further.
  // crbug.com/598286.
  CHECK(it->second.get());

  pool_resource->set_last_usage(clock_->NowTicks());

  // Save the ResourceId since the |pool_resource| can be deleted in the next
  // step.
  viz::ResourceId resource_id = pool_resource->resource_id();

  // Transfer resource to |unused_resources_| or |busy_resources_|, depending if
  // it was exported to the ResourceProvider via PrepareForExport(). If not,
  // then we can immediately make the resource available to be reused, unless it
  // was marked not for reuse.
  if (resource_id) {
    pool_resource->set_state(PoolResource::kBusy);
    busy_resources_.push_front(std::move(it->second));
  } else if (pool_resource->avoid_reuse()) {
    pool_resource->set_state(PoolResource::kUnused);
    DeleteResource(std::move(it->second));  // This deletes |pool_resource|.
  } else {
    pool_resource->set_state(PoolResource::kUnused);
    DidFinishUsingResource(std::move(it->second));
  }
  in_use_resources_.erase(it);

  // If the resource was exported, then it has a resource id. By removing the
  // resource id, we will be notified in the ReleaseCallback when the resource
  // is no longer exported and can be reused.
  if (resource_id)
    resource_provider_->RemoveImportedResource(resource_id);

  // Now that we have evictable resources, schedule an eviction call for this
  // resource if necessary.
  ScheduleEvictExpiredResourcesIn(resource_expiration_delay_);
}

void ResourcePool::OnContentReplaced(const InUsePoolResource& in_use_resource,
                                     uint64_t content_id) {
  PoolResource* resource = in_use_resource.resource_;
  DCHECK(resource);
  resource->set_content_id(content_id);
  resource->set_invalidated_rect(gfx::Rect());
}

void ResourcePool::SetResourceUsageLimits(size_t max_memory_usage_bytes,
                                          size_t max_resource_count) {
  max_memory_usage_bytes_ = max_memory_usage_bytes;
  max_resource_count_ = max_resource_count;

  ReduceResourceUsage();
}

void ResourcePool::ReduceResourceUsage() {
  while (!unused_resources_.empty()) {
    if (!ResourceUsageTooHigh())
      break;

    // LRU eviction pattern. Most recently used might be blocked by
    // a read lock fence but it's still better to evict the least
    // recently used as it prevents a resource that is hard to reuse
    // because of unique size from being kept around. Resources that
    // can't be locked for write might also not be truly free-able.
    // We can free the resource here but it doesn't mean that the
    // memory is necessarily returned to the OS.
    DCHECK_GE(unused_memory_usage_bytes_,
              unused_resources_.back()->memory_usage());
    unused_memory_usage_bytes_ -= unused_resources_.back()->memory_usage();
    DeleteResource(PopBack(&unused_resources_));
  }
}

bool ResourcePool::ResourceUsageTooHigh() {
  if (total_resource_count_ > max_resource_count_)
    return true;
  if (total_memory_usage_bytes_ > max_memory_usage_bytes_)
    return true;
  return false;
}

void ResourcePool::DeleteResource(std::unique_ptr<PoolResource> resource) {
  DCHECK_GE(total_memory_usage_bytes_, resource->memory_usage());
  total_memory_usage_bytes_ -= resource->memory_usage();
  --total_resource_count_;
  if (flush_evicted_resources_deadline_ == base::TimeTicks::Max()) {
    flush_evicted_resources_deadline_ =
        clock_->NowTicks() + kDefaultMaxFlushDelay;
  }
}

void ResourcePool::UpdateResourceContentIdAndInvalidation(
    PoolResource* resource,
    uint64_t new_content_id,
    const gfx::Rect& new_invalidated_rect) {
  gfx::Rect updated_invalidated_rect = new_invalidated_rect;
  if (!resource->invalidated_rect().IsEmpty())
    updated_invalidated_rect.Union(resource->invalidated_rect());

  resource->set_content_id(new_content_id);
  resource->set_invalidated_rect(updated_invalidated_rect);
}

void ResourcePool::DidFinishUsingResource(
    std::unique_ptr<PoolResource> resource) {
  unused_memory_usage_bytes_ += resource->memory_usage();
  unused_resources_.push_front(std::move(resource));
}

void ResourcePool::ScheduleEvictExpiredResourcesIn(
    base::TimeDelta time_from_now) {
  if (evict_expired_resources_pending_)
    return;

  evict_expired_resources_pending_ = true;

  task_runner_->PostDelayedTask(
      FROM_HERE,
      base::BindOnce(&ResourcePool::EvictExpiredResources,
                     weak_ptr_factory_.GetWeakPtr()),
      time_from_now);
}

void ResourcePool::EvictExpiredResources() {
  evict_expired_resources_pending_ = false;
  base::TimeTicks current_time = clock_->NowTicks();

  EvictResourcesNotUsedSince(current_time - resource_expiration_delay_);

  if (unused_resources_.empty() ||
      flush_evicted_resources_deadline_ <= current_time) {
    // If nothing is evictable, we have deleted one (and possibly more)
    // resources without any new activity. Flush to ensure these deletions are
    // processed.
    FlushEvictedResources();
  }

  if (!unused_resources_.empty()) {
    // If we still have evictable resources, schedule a call to
    // EvictExpiredResources for either (a) the time when the LRU buffer expires
    // or (b) the deadline to explicitly flush previously evicted resources.
    ScheduleEvictExpiredResourcesIn(
        std::min(GetUsageTimeForLRUResource() + resource_expiration_delay_,
                 flush_evicted_resources_deadline_) -
        current_time);
  }
}

void ResourcePool::EvictResourcesNotUsedSince(base::TimeTicks time_limit) {
  while (!unused_resources_.empty()) {
    // |unused_resources_| is not strictly ordered with regards to last_usage,
    // as this may not exactly line up with the time a resource became non-busy.
    // However, this should be roughly ordered, and will only introduce slight
    // delays in freeing expired resources.
    if (unused_resources_.back()->last_usage() > time_limit)
      return;

    DCHECK_GE(unused_memory_usage_bytes_,
              unused_resources_.back()->memory_usage());
    unused_memory_usage_bytes_ -= unused_resources_.back()->memory_usage();
    DeleteResource(PopBack(&unused_resources_));
  }
}

base::TimeTicks ResourcePool::GetUsageTimeForLRUResource() const {
  if (!unused_resources_.empty()) {
    return unused_resources_.back()->last_usage();
  }

  // This is only called when we have at least one evictable resource.
  DCHECK(!busy_resources_.empty());
  return busy_resources_.back()->last_usage();
}

void ResourcePool::FlushEvictedResources() {
  flush_evicted_resources_deadline_ = base::TimeTicks::Max();
  if (context_provider_) {
    // Flush any raster + shared image work.
    context_provider_->ContextSupport()->FlushPendingWork();
  }
}

bool ResourcePool::OnMemoryDump(const base::trace_event::MemoryDumpArgs& args,
                                base::trace_event::ProcessMemoryDump* pmd) {
  if (args.level_of_detail == MemoryDumpLevelOfDetail::kBackground) {
    std::string dump_name =
        base::StringPrintf("cc/tile_memory/provider_0x%x", tracing_id_);
    MemoryAllocatorDump* dump = pmd->CreateAllocatorDump(dump_name);
    dump->AddScalar(MemoryAllocatorDump::kNameSize,
                    MemoryAllocatorDump::kUnitsBytes,
                    total_memory_usage_bytes_);
  } else {
    for (const auto& resource : unused_resources_) {
      resource->OnMemoryDump(pmd, tracing_id_, resource_provider_,
                             true /* is_free */, false /* is_busy */);
    }
    for (const auto& resource : busy_resources_) {
      resource->OnMemoryDump(pmd, tracing_id_, resource_provider_,
                             false /* is_free */, true /* is_busy */);
    }
    for (const auto& entry : in_use_resources_) {
      entry.second->OnMemoryDump(pmd, tracing_id_, resource_provider_,
                                 false /* is_free */, false /* is_busy */);
    }
  }
  return true;
}

void ResourcePool::OnMemoryPressure(
    base::MemoryPressureListener::MemoryPressureLevel level) {
  switch (level) {
    case base::MemoryPressureListener::MEMORY_PRESSURE_LEVEL_NONE:
    case base::MemoryPressureListener::MEMORY_PRESSURE_LEVEL_MODERATE:
      break;
    case base::MemoryPressureListener::MEMORY_PRESSURE_LEVEL_CRITICAL:
      EvictResourcesNotUsedSince(base::TimeTicks() + base::TimeDelta::Max());
      FlushEvictedResources();
      break;
  }
}

ResourcePool::PoolResource::PoolResource(ResourcePool* resource_pool,
                                         size_t unique_id,
                                         const gfx::Size& size,
                                         viz::SharedImageFormat format,
                                         const gfx::ColorSpace& color_space)
    : resource_pool_(resource_pool),
      unique_id_(unique_id),
      size_(size),
      format_(format),
      color_space_(color_space) {}

ResourcePool::PoolResource::~PoolResource() = default;

void ResourcePool::PoolResource::OnMemoryDump(
    base::trace_event::ProcessMemoryDump* pmd,
    int tracing_id,
    const viz::ClientResourceProvider* resource_provider,
    bool is_free,
    bool is_busy) const {
  // Resource IDs are not process-unique, so log with the ResourcePool's unique
  // tracing id.
  const std::string dump_name = base::StringPrintf(
      "cc/tile_memory/provider_%d/%s%sresource_%zd", tracing_id,
      debug_name_.empty() ? "" : debug_name_.c_str(),
      debug_name_.empty() ? "" : "/", unique_id_);
  MemoryAllocatorDump* dump = pmd->CreateAllocatorDump(dump_name);

  // The importance value used here needs to be greater than the importance
  // used in other places that use this GUID to inform the system that this is
  // the root ownership.
  const int kImportance =
      static_cast<int>(gpu::TracingImportance::kClientOwner);
  auto* dump_manager = base::trace_event::MemoryDumpManager::GetInstance();
  uint64_t tracing_process_id = dump_manager->GetTracingProcessId();
  if (software_backing_) {
    software_backing_->OnMemoryDump(pmd, dump->guid(), tracing_process_id,
                                    kImportance);
  } else if (gpu_backing_) {
    gpu_backing_->OnMemoryDump(pmd, dump->guid(), tracing_process_id,
                               kImportance);
  }

  uint64_t total_bytes = memory_usage();
  dump->AddScalar(MemoryAllocatorDump::kNameSize,
                  MemoryAllocatorDump::kUnitsBytes, total_bytes);

  uint64_t free_size = is_free ? total_bytes : 0u;
  dump->AddScalar("free_size", MemoryAllocatorDump::kUnitsBytes, free_size);
  uint64_t busy_size = is_busy ? total_bytes : 0u;
  dump->AddScalar("busy_size", MemoryAllocatorDump::kUnitsBytes, busy_size);
}

}  // namespace cc
