// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CC_RESOURCES_RESOURCE_POOL_H_
#define CC_RESOURCES_RESOURCE_POOL_H_

#include <stddef.h>
#include <stdint.h>

#include <map>
#include <memory>
#include <string>
#include <utility>

#include "base/containers/circular_deque.h"
#include "base/gtest_prod_util.h"
#include "base/memory/memory_pressure_listener.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/time/tick_clock.h"
#include "base/time/time.h"
#include "base/trace_event/memory_allocator_dump_guid.h"
#include "base/trace_event/memory_dump_provider.h"
#include "cc/cc_export.h"
#include "components/viz/common/resources/resource_id.h"
#include "components/viz/common/resources/resource_sizes.h"
#include "components/viz/common/resources/shared_bitmap.h"
#include "components/viz/common/resources/shared_image_format.h"
#include "components/viz/common/resources/transferable_resource.h"
#include "gpu/command_buffer/common/sync_token.h"
#include "third_party/khronos/GLES2/gl2.h"
#include "ui/gfx/color_space.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/gfx/gpu_memory_buffer.h"

namespace base {
class SingleThreadTaskRunner;
}

namespace gpu {
class ClientSharedImage;
}

namespace viz {
class ClientResourceProvider;
class RasterContextProvider;
}

namespace cc {

class CC_EXPORT ResourcePool : public base::trace_event::MemoryDumpProvider {
  class PoolResource;

 public:
  // Delay before a resource is considered expired.
  static constexpr base::TimeDelta kDefaultExpirationDelay = base::Seconds(5);
  // Max delay before an evicted resource is flushed.
  static constexpr base::TimeDelta kDefaultMaxFlushDelay = base::Seconds(1);

  // A base class to hold ownership of gpu backed PoolResources. Allows the
  // client to define destruction semantics.
  class CC_EXPORT GpuBacking {
   public:
    GpuBacking();
    virtual ~GpuBacking();

    // Dumps information about the memory backing the GpuBacking to |pmd|.
    // The memory usage is attributed to |buffer_dump_guid|.
    // |tracing_process_id| uniquely identifies the process owning the memory.
    // |importance| is relevant only for the cases of co-ownership, the memory
    // gets attributed to the owner with the highest importance.
    // Called on the compositor thread.
    virtual void OnMemoryDump(
        base::trace_event::ProcessMemoryDump* pmd,
        const base::trace_event::MemoryAllocatorDumpGuid& buffer_dump_guid,
        uint64_t tracing_process_id,
        int importance) const = 0;

    scoped_refptr<gpu::ClientSharedImage> shared_image;
    gpu::SyncToken mailbox_sync_token;
    bool overlay_candidate = false;
    // For resources that are modified directly on the gpu, outside the command
    // stream, a fence must be used to know when the backing is not in use and
    // may be returned to and reused by the pool.
    bool wait_on_fence_required = false;

    // Set by the ResourcePool when a resource is returned from the display
    // compositor, or when the resource texture and mailbox are created for the
    // first time, if the resource is shared with another context. The client of
    // ResourcePool needs to wait on this token if it exists, before using a
    // resource handed out by the ResourcePool.
    gpu::SyncToken returned_sync_token;

    // True if the backing is using raw draw.
    bool is_using_raw_draw = false;
  };

  // A base class to hold ownership of software backed PoolResources. Allows the
  // client to define destruction semantics.
  class CC_EXPORT SoftwareBacking {
   public:
    SoftwareBacking();
    virtual ~SoftwareBacking();

    // Dumps information about the memory backing the SoftwareBacking to |pmd|.
    // The memory usage is attributed to |buffer_dump_guid|.
    // |tracing_process_id| uniquely identifies the process owning the memory.
    // |importance| is relevant only for the cases of co-ownership, the memory
    // gets attributed to the owner with the highest importance.
    // Called on the compositor thread.
    virtual void OnMemoryDump(
        base::trace_event::ProcessMemoryDump* pmd,
        const base::trace_event::MemoryAllocatorDumpGuid& buffer_dump_guid,
        uint64_t tracing_process_id,
        int importance) const = 0;

    // Mailbox
    viz::SharedBitmapId shared_bitmap_id;

    scoped_refptr<gpu::ClientSharedImage> shared_image;
    gpu::SyncToken mailbox_sync_token;
  };

  // Scoped move-only object returned when getting a resource from the pool.
  // Ownership must be given back to the pool to release the resource.
  class InUsePoolResource {
   public:
    InUsePoolResource() = default;
    ~InUsePoolResource() {
      DCHECK(!resource_) << "Must be returned to ResourcePool to be freed.";
    }

    InUsePoolResource(InUsePoolResource&& other) {
      is_gpu_ = other.is_gpu_;
      resource_ = other.resource_;
      other.resource_ = nullptr;
    }
    InUsePoolResource& operator=(InUsePoolResource&& other) {
      is_gpu_ = other.is_gpu_;
      resource_ = other.resource_;
      other.resource_ = nullptr;
      return *this;
    }

    InUsePoolResource(const InUsePoolResource&) = delete;
    InUsePoolResource& operator=(const InUsePoolResource&) = delete;

    explicit operator bool() const { return !!resource_; }

    const gfx::Size& size() const { return resource_->size(); }
    const viz::SharedImageFormat& format() const { return resource_->format(); }
    const gfx::ColorSpace& color_space() const {
      return resource_->color_space();
    }
    // The ResourceId when the backing is given to the ResourceProvider for
    // export to the display compositor.
    const viz::ResourceId& resource_id_for_export() const {
      // The ResourceId should not be accessed before it is created!
      DCHECK(resource_->resource_id());
      return resource_->resource_id();
    }

    // Only valid when the ResourcePool is vending texture-backed resources.
    GpuBacking* gpu_backing() const {
      DCHECK(is_gpu_);
      return resource_->gpu_backing();
    }
    void set_gpu_backing(std::unique_ptr<GpuBacking> gpu) const {
      DCHECK(is_gpu_);
      return resource_->set_gpu_backing(std::move(gpu));
    }

    // Only valid when the ResourcePool is vending software-backed resources.
    SoftwareBacking* software_backing() const {
      DCHECK(!is_gpu_);
      return resource_->software_backing();
    }
    void set_software_backing(std::unique_ptr<SoftwareBacking> software) const {
      DCHECK(!is_gpu_);
      resource_->set_software_backing(std::move(software));
    }

    size_t memory_usage() const {
      DCHECK(resource_);
      return resource_->memory_usage();
    }

    // Production code should not be built around these ids, but tests use them
    // to check for identity.
    size_t unique_id_for_testing() const { return resource_->unique_id(); }

   private:
    friend ResourcePool;
    explicit InUsePoolResource(PoolResource* resource, bool is_gpu)
        : is_gpu_(is_gpu), resource_(resource) {
      DCHECK_EQ(resource->state(), PoolResource::kInUse);
    }
    void SetWasFreedByResourcePool() { resource_ = nullptr; }

    bool is_gpu_ = false;

    // `resource_` is not a raw_ptr<...> for performance reasons (based on
    // analysis of sampling profiler data and tab_search:top100:2020).
    RAW_PTR_EXCLUSION PoolResource* resource_ = nullptr;
  };

  // When holding gpu resources, the |context_provider| should be non-null,
  // and when holding software resources, it should be null. It is used for
  // consistency checking as well as for correctness.
  ResourcePool(viz::ClientResourceProvider* resource_provider,
               viz::RasterContextProvider* context_provider,
               scoped_refptr<base::SingleThreadTaskRunner> task_runner,
               const base::TimeDelta& expiration_delay,
               bool disallow_non_exact_reuse);

  ResourcePool(const ResourcePool&) = delete;
  ~ResourcePool() override;

  ResourcePool& operator=(const ResourcePool&) = delete;

  // Tries to reuse a resource. If none are available, makes a new one.
  InUsePoolResource AcquireResource(
      const gfx::Size& size,
      viz::SharedImageFormat format,
      const gfx::ColorSpace& color_space,
      const std::string& debug_name = std::string());

  // Tries to acquire the resource with |previous_content_id| for us in partial
  // raster. If successful, this function will retun the invalidated rect which
  // must be re-rastered in |total_invalidated_rect|.
  InUsePoolResource TryAcquireResourceForPartialRaster(
      uint64_t new_content_id,
      const gfx::Rect& new_invalidated_rect,
      uint64_t previous_content_id,
      gfx::Rect* total_invalidated_rect,
      const gfx::ColorSpace& raster_color_space,
      const std::string& debug_name = std::string());

  // Gives the InUsePoolResource a |resource_id_for_export()| in order to allow
  // exporting of the resource to the display compositor. This must be called
  // with a resource only after it has a backing allocated for it. Initially an
  // acquired InUsePoolResource will be only metadata, and the backing is given
  // to it by code which is aware of the expected backing type - currently by
  // RasterBufferProvider::AcquireBufferForRaster().
  // Returns false if the backing does not contain valid data, in particular
  // a zero mailbox for GpuBacking, in which case the resource is not exported,
  // and true otherwise.
  bool PrepareForExport(
      const InUsePoolResource& resource,
      viz::TransferableResource::ResourceSource resource_source);

  // Marks any resources in the pool as invalid, preventing their reuse. Call if
  // previous resources were allocated in one way, but future resources should
  // be allocated in a different way.
  void InvalidateResources();

  // Called when a resource's content has been fully replaced (and is completely
  // valid). Updates the resource's content ID to its new value.
  void OnContentReplaced(const InUsePoolResource& in_use_resource,
                         uint64_t content_id);
  void ReleaseResource(InUsePoolResource resource);

  void SetResourceUsageLimits(size_t max_memory_usage_bytes,
                              size_t max_resource_count);
  void ReduceResourceUsage();
  bool ResourceUsageTooHigh();

  size_t memory_usage_bytes() const {
    return total_memory_usage_bytes_ - unused_memory_usage_bytes_;
  }
  size_t resource_count() const { return in_use_resources_.size(); }

  // Overridden from base::trace_event::MemoryDumpProvider:
  bool OnMemoryDump(const base::trace_event::MemoryDumpArgs& args,
                    base::trace_event::ProcessMemoryDump* pmd) override;

  void OnMemoryPressure(
      base::MemoryPressureListener::MemoryPressureLevel level);

  size_t GetTotalMemoryUsageForTesting() const {
    return total_memory_usage_bytes_;
  }
  size_t GetTotalResourceCountForTesting() const {
    return total_resource_count_;
  }
  size_t GetBusyResourceCountForTesting() const {
    return busy_resources_.size();
  }
  bool AllowsNonExactReUseForTesting() const {
    return !disallow_non_exact_reuse_;
  }

  // Overrides internal clock for testing purposes.
  void SetClockForTesting(const base::TickClock* clock) { clock_ = clock; }
  int tracing_id() const { return tracing_id_; }

 private:
  FRIEND_TEST_ALL_PREFIXES(ResourcePoolTest, ReuseResource);
  FRIEND_TEST_ALL_PREFIXES(ResourcePoolTest, ExactRequestsRespected);
  class PoolResource {
   public:
    PoolResource(ResourcePool* resource_pool,
                 size_t unique_id,
                 const gfx::Size& size,
                 viz::SharedImageFormat format,
                 const gfx::ColorSpace& color_space);
    ~PoolResource();

    size_t unique_id() const { return unique_id_; }
    const gfx::Size& size() const { return size_; }
    const viz::SharedImageFormat& format() const { return format_; }
    const gfx::ColorSpace& color_space() const { return color_space_; }

    const viz::ResourceId& resource_id() const { return resource_id_; }
    void set_resource_id(viz::ResourceId id) { resource_id_ = id; }

    GpuBacking* gpu_backing() const { return gpu_backing_.get(); }
    void set_gpu_backing(std::unique_ptr<GpuBacking> gpu) {
      DCHECK(gpu);
      DCHECK(!gpu_backing_);
      DCHECK(!software_backing_);
      gpu_backing_ = std::move(gpu);
      resource_pool_->OnBackingAllocated(this);
    }

    SoftwareBacking* software_backing() const {
      return software_backing_.get();
    }
    void set_software_backing(std::unique_ptr<SoftwareBacking> software) {
      DCHECK(software);
      DCHECK(!gpu_backing_);
      DCHECK(!software_backing_);
      software_backing_ = std::move(software);
      resource_pool_->OnBackingAllocated(this);
    }

    uint64_t content_id() const { return content_id_; }
    void set_content_id(uint64_t content_id) { content_id_ = content_id; }

    base::TimeTicks last_usage() const { return last_usage_; }
    void set_last_usage(base::TimeTicks time) { last_usage_ = time; }

    const gfx::Rect& invalidated_rect() const { return invalidated_rect_; }
    void set_invalidated_rect(const gfx::Rect& invalidated_rect) {
      invalidated_rect_ = invalidated_rect;
    }

    bool avoid_reuse() const { return avoid_reuse_; }
    void mark_avoid_reuse() { avoid_reuse_ = true; }

    void OnMemoryDump(base::trace_event::ProcessMemoryDump* pmd,
                      int tracing_id,
                      const viz::ClientResourceProvider* resource_provider,
                      bool is_free,
                      bool is_busy) const;

    void set_debug_name(const std::string& name) { debug_name_ = name; }
    const std::string& debug_name() const { return debug_name_; }

    ResourcePool* resource_pool() const { return resource_pool_; }

    enum State {
      // kUnused means the resource is free for reusing or releasing.
      // A new created resource is in kUnused as well.
      kUnused,

      // kInUse means the resource is being used viz InUsePoolResource.
      // The InUsePoolResource can be released by calling
      // ResourcePool::ReleaseResource(), after that, the state will be changed
      // to kBusy or kUnused depends on if the resource is exported.
      kInUse,

      // The resource has been expored (sent) to viz process for compositing.
      // When the resource is returned from the viz, the state will be changed
      // to kUnused.
      kBusy,
    };
    State state() const { return state_; }
    void set_state(State state) { state_ = state; }

    size_t memory_usage() const {
      if (!gpu_backing_ && !software_backing_)
        return 0;

      size_t memory_usage = format().EstimatedSizeInBytes(size());

      // Early research found with raw draw, GPU memory usage is reduced to
      // 50%, so we consider a raw draw backing uses 50% of a normal backing
      // in average.
      // TODO(crbug.com/40214331): use accurate size for raw draw backings.
      if (gpu_backing_ && gpu_backing_->is_using_raw_draw) {
        memory_usage = memory_usage / 2;
      }

      return memory_usage;
    }

   private:
    const raw_ptr<ResourcePool> resource_pool_;
    const size_t unique_id_;
    const gfx::Size size_;
    const viz::SharedImageFormat format_;
    const gfx::ColorSpace color_space_;

    uint64_t content_id_ = 0;
    base::TimeTicks last_usage_;
    gfx::Rect invalidated_rect_;

    // Set to true for resources that should be destroyed instead of returned to
    // the pool for reuse.
    bool avoid_reuse_ = false;

    // An id used to name the backing for transfer to the display compositor.
    viz::ResourceId resource_id_ = viz::kInvalidResourceId;

    // The backing for gpu resources. Initially null for resources given
    // out by ResourcePool, to be filled in by the client. Is destroyed on the
    // compositor thread.
    std::unique_ptr<GpuBacking> gpu_backing_;

    // The backing for software resources. Initially null for resources given
    // out by ResourcePool, to be filled in by the client. Is destroyed on the
    // compositor thread.
    std::unique_ptr<SoftwareBacking> software_backing_;

    // Used for debugging and tracing.
    std::string debug_name_;

    // The current resource state. See enum State for detail.
    State state_ = kUnused;
  };

  // Called when backing is set for the PoolResource.
  void OnBackingAllocated(PoolResource* resource);

  // Callback from the ResourceProvider to notify when an exported PoolResource
  // is not busy and may be reused.
  void OnResourceReleased(size_t unique_id,
                          const gpu::SyncToken& sync_token,
                          bool lost);

  // Tries to reuse a resource. Returns |nullptr| if none are available.
  PoolResource* ReuseResource(const gfx::Size& size,
                              viz::SharedImageFormat format,
                              const gfx::ColorSpace& color_space);

  // Creates a new resource without trying to reuse an old one.
  PoolResource* CreateResource(const gfx::Size& size,
                               viz::SharedImageFormat format,
                               const gfx::ColorSpace& color_space);

  void DidFinishUsingResource(std::unique_ptr<PoolResource> resource);
  void DeleteResource(std::unique_ptr<PoolResource> resource);
  static void UpdateResourceContentIdAndInvalidation(
      PoolResource* resource,
      uint64_t new_content_id,
      const gfx::Rect& new_invalidated_rect);

  // Functions which manage periodic eviction of expired resources.
  void ScheduleEvictExpiredResourcesIn(base::TimeDelta time_from_now);
  void EvictExpiredResources();
  void EvictResourcesNotUsedSince(base::TimeTicks time_limit);
  bool HasEvictableResources() const;
  base::TimeTicks GetUsageTimeForLRUResource() const;
  void FlushEvictedResources();

  const raw_ptr<viz::ClientResourceProvider> resource_provider_;
  const raw_ptr<viz::RasterContextProvider> context_provider_;
  const scoped_refptr<base::SingleThreadTaskRunner> task_runner_;
  const base::TimeDelta resource_expiration_delay_;
  const bool disallow_non_exact_reuse_ = false;
  const int tracing_id_;

  size_t next_resource_unique_id_ = 1;
  size_t max_memory_usage_bytes_ = 0;
  size_t max_resource_count_ = 0;
  size_t unused_memory_usage_bytes_ = 0;
  size_t total_memory_usage_bytes_ = 0;
  size_t total_resource_count_ = 0;
  bool evict_expired_resources_pending_ = false;
  bool evict_busy_resources_when_unused_ = false;

  // Holds most recently used resources at the front of the queue.
  base::circular_deque<std::unique_ptr<PoolResource>> unused_resources_;
  base::circular_deque<std::unique_ptr<PoolResource>> busy_resources_;

  // Map from the PoolResource |unique_id| to the PoolResource.
  std::map<size_t, std::unique_ptr<PoolResource>> in_use_resources_;

  std::unique_ptr<base::MemoryPressureListener> memory_pressure_listener_;

  base::TimeTicks flush_evicted_resources_deadline_;

  raw_ptr<const base::TickClock> clock_;

  base::WeakPtrFactory<ResourcePool> weak_ptr_factory_{this};
};

}  // namespace cc

#endif  // CC_RESOURCES_RESOURCE_POOL_H_
