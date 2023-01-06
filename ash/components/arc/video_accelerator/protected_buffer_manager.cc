// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/components/arc/video_accelerator/protected_buffer_manager.h"

#include <utility>

#include "ash/components/arc/video_accelerator/protected_buffer_allocator.h"
#include "base/bits.h"
#include "base/functional/bind.h"
#include "base/logging.h"
#include "base/memory/unsafe_shared_memory_region.h"
#include "base/posix/eintr_wrapper.h"
#include "base/system/sys_info.h"
#include "base/threading/thread_checker.h"
#include "media/gpu/macros.h"
#include "media/media_buildflags.h"
#include "ui/gfx/geometry/size.h"
#include "ui/ozone/public/ozone_platform.h"
#include "ui/ozone/public/surface_factory_ozone.h"

namespace arc {

namespace {
// Size of the pixmap to be used as the dummy handle for protected buffers.
constexpr gfx::Size kDummyBufferSize(16, 16);

// Maximum number of concurrent ProtectedBufferAllocatorImpl instances.
// Currently we have no way to know the resources of ProtectedBufferAllocator.
// Arbitrarily chosen a reasonable constant as the limit.
constexpr size_t kMaxConcurrentProtectedBufferAllocators = 32;

// Maximum number of concurrent allocated protected buffers in a single
// ProtectedBufferAllocator. This limitation, 64 is arbitrarily chosen.
constexpr size_t kMaxBuffersPerAllocator = 64;
}  // namespace

class ProtectedBufferManager::ProtectedBuffer {
 public:
  ProtectedBuffer(const ProtectedBuffer&) = delete;
  ProtectedBuffer& operator=(const ProtectedBuffer&) = delete;

  virtual ~ProtectedBuffer() {}

  // Downcasting methods to return duplicated handles to the underlying
  // protected buffers for each buffer type, or empty/null handles if not
  // applicable.
  virtual base::UnsafeSharedMemoryRegion DuplicateSharedMemoryRegion() const {
    return {};
  }
  virtual gfx::NativePixmapHandle DuplicateNativePixmapHandle() const {
    return gfx::NativePixmapHandle();
  }

  // Downcasting method to return a scoped_refptr to the underlying
  // NativePixmap, or null if not applicable.
  virtual scoped_refptr<gfx::NativePixmap> GetNativePixmap() const {
    return nullptr;
  }

 protected:
  explicit ProtectedBuffer(scoped_refptr<gfx::NativePixmap> dummy_handle)
      : dummy_handle_(std::move(dummy_handle)) {}

 private:
  scoped_refptr<gfx::NativePixmap> dummy_handle_;
};

class ProtectedBufferManager::ProtectedSharedMemory
    : public ProtectedBufferManager::ProtectedBuffer {
 public:
  ~ProtectedSharedMemory() override;

  // Allocate a ProtectedSharedMemory buffer of |size| bytes.
  static std::unique_ptr<ProtectedSharedMemory> Create(
      scoped_refptr<gfx::NativePixmap> dummy_handle,
      size_t size);

  base::UnsafeSharedMemoryRegion DuplicateSharedMemoryRegion() const override {
    return region_.Duplicate();
  }

 private:
  explicit ProtectedSharedMemory(scoped_refptr<gfx::NativePixmap> dummy_handle);

  base::UnsafeSharedMemoryRegion region_;
};

ProtectedBufferManager::ProtectedSharedMemory::ProtectedSharedMemory(
    scoped_refptr<gfx::NativePixmap> dummy_handle)
    : ProtectedBuffer(std::move(dummy_handle)) {}

ProtectedBufferManager::ProtectedSharedMemory::~ProtectedSharedMemory() {}

// static
std::unique_ptr<ProtectedBufferManager::ProtectedSharedMemory>
ProtectedBufferManager::ProtectedSharedMemory::Create(
    scoped_refptr<gfx::NativePixmap> dummy_handle,
    size_t size) {
  std::unique_ptr<ProtectedSharedMemory> protected_shmem(
      new ProtectedSharedMemory(std::move(dummy_handle)));

  size_t aligned_size =
      base::bits::AlignUp(size, base::SysInfo::VMAllocationGranularity());

  auto shmem_region = base::UnsafeSharedMemoryRegion::Create(aligned_size);
  if (!shmem_region.IsValid()) {
    VLOGF(1) << "Failed to allocate shared memory";
    return nullptr;
  }

  protected_shmem->region_ = std::move(shmem_region);
  return protected_shmem;
}

class ProtectedBufferManager::ProtectedNativePixmap
    : public ProtectedBufferManager::ProtectedBuffer {
 public:
  ~ProtectedNativePixmap() override;

  // Allocate a ProtectedNativePixmap of |format| and |size|.
  static std::unique_ptr<ProtectedNativePixmap> Create(
      scoped_refptr<gfx::NativePixmap> dummy_handle,
      gfx::BufferFormat format,
      const gfx::Size& size);

  gfx::NativePixmapHandle DuplicateNativePixmapHandle() const override {
    return native_pixmap_->ExportHandle();
  }

  scoped_refptr<gfx::NativePixmap> GetNativePixmap() const override {
    return native_pixmap_;
  }

 private:
  explicit ProtectedNativePixmap(scoped_refptr<gfx::NativePixmap> dummy_handle);

  scoped_refptr<gfx::NativePixmap> native_pixmap_;
};

ProtectedBufferManager::ProtectedNativePixmap::ProtectedNativePixmap(
    scoped_refptr<gfx::NativePixmap> dummy_handle)
    : ProtectedBuffer(std::move(dummy_handle)) {}

ProtectedBufferManager::ProtectedNativePixmap::~ProtectedNativePixmap() {}

// static
std::unique_ptr<ProtectedBufferManager::ProtectedNativePixmap>
ProtectedBufferManager::ProtectedNativePixmap::Create(
    scoped_refptr<gfx::NativePixmap> dummy_handle,
    gfx::BufferFormat format,
    const gfx::Size& size) {
  std::unique_ptr<ProtectedNativePixmap> protected_pixmap(
      new ProtectedNativePixmap(std::move(dummy_handle)));

  ui::OzonePlatform* platform = ui::OzonePlatform::GetInstance();
  ui::SurfaceFactoryOzone* factory = platform->GetSurfaceFactoryOzone();
  protected_pixmap->native_pixmap_ = factory->CreateNativePixmap(
      gfx::kNullAcceleratedWidget, VK_NULL_HANDLE, size, format,
#if BUILDFLAG(USE_ARC_PROTECTED_MEDIA)
      gfx::BufferUsage::PROTECTED_SCANOUT_VDA_WRITE);
#else
      gfx::BufferUsage::SCANOUT_VDA_WRITE);
#endif  // BUILDFLAG(USE_ARC_PROTECTED_MEDIA)

  if (!protected_pixmap->native_pixmap_) {
    VLOGF(1) << "Failed allocating a native pixmap";
    return nullptr;
  }

  return protected_pixmap;
}

// The ProtectedBufferAllocator implementation using ProtectedBufferManager.
// The functions just delegate the corresponding functions of
// ProtectedBufferManager.
// This destructor executes the callback to release the references of all non
// released protected buffers.
class ProtectedBufferManager::ProtectedBufferAllocatorImpl
    : public ProtectedBufferAllocator {
 public:
  ProtectedBufferAllocatorImpl(
      uint64_t allocator_id,
      scoped_refptr<ProtectedBufferManager> protected_buffer_manager,
      base::OnceClosure release_all_protected_buffers_cb);

  ProtectedBufferAllocatorImpl(const ProtectedBufferAllocatorImpl&) = delete;
  ProtectedBufferAllocatorImpl& operator=(const ProtectedBufferAllocatorImpl&) =
      delete;

  ~ProtectedBufferAllocatorImpl() override;
  bool AllocateProtectedSharedMemory(base::ScopedFD dummy_fd,
                                     size_t size) override;
  bool AllocateProtectedNativePixmap(base::ScopedFD dummy_fd,
                                     gfx::BufferFormat format,
                                     const gfx::Size& size) override;
  void ReleaseProtectedBuffer(base::ScopedFD dummy_fd) override;

 private:
  const uint64_t allocator_id_;
  const scoped_refptr<ProtectedBufferManager> protected_buffer_manager_;
  base::OnceClosure release_all_protected_buffers_cb_;

  THREAD_CHECKER(thread_checker_);
};

ProtectedBufferManager::ProtectedBufferAllocatorImpl::
    ProtectedBufferAllocatorImpl(
        uint64_t allocator_id,
        scoped_refptr<ProtectedBufferManager> protected_buffer_manager,
        base::OnceClosure release_all_protected_buffers_cb)
    : allocator_id_(allocator_id),
      protected_buffer_manager_(std::move(protected_buffer_manager)),
      release_all_protected_buffers_cb_(
          std::move(release_all_protected_buffers_cb)) {}

ProtectedBufferManager::ProtectedBufferAllocatorImpl::
    ~ProtectedBufferAllocatorImpl() {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  DCHECK(release_all_protected_buffers_cb_);
  std::move(release_all_protected_buffers_cb_).Run();
}

bool ProtectedBufferManager::ProtectedBufferAllocatorImpl::
    AllocateProtectedSharedMemory(base::ScopedFD dummy_fd, size_t size) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  return protected_buffer_manager_->AllocateProtectedSharedMemory(
      allocator_id_, std::move(dummy_fd), size);
}

bool ProtectedBufferManager::ProtectedBufferAllocatorImpl::
    AllocateProtectedNativePixmap(base::ScopedFD dummy_fd,
                                  gfx::BufferFormat format,
                                  const gfx::Size& size) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  return protected_buffer_manager_->AllocateProtectedNativePixmap(
      allocator_id_, std::move(dummy_fd), format, size);
}

void ProtectedBufferManager::ProtectedBufferAllocatorImpl::
    ReleaseProtectedBuffer(base::ScopedFD dummy_fd) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  protected_buffer_manager_->ReleaseProtectedBuffer(allocator_id_,
                                                    std::move(dummy_fd));
}

ProtectedBufferManager::ProtectedBufferManager()
    : next_protected_buffer_allocator_id_(0) {
  VLOGF(2);
}

ProtectedBufferManager::~ProtectedBufferManager() {
  VLOGF(2);
}

bool ProtectedBufferManager::GetAllocatorId(uint64_t* const allocator_id) {
  base::AutoLock lock(buffer_map_lock_);
  if (allocator_to_buffers_map_.size() ==
      kMaxConcurrentProtectedBufferAllocators) {
    VLOGF(1) << "Failed Creating PBA due to many ProtectedBufferAllocators: "
             << kMaxConcurrentProtectedBufferAllocators;
    return false;
  }
  *allocator_id = next_protected_buffer_allocator_id_;
  allocator_to_buffers_map_.insert({*allocator_id, {}});
  next_protected_buffer_allocator_id_++;
  return true;
}

// static
std::unique_ptr<ProtectedBufferAllocator>
ProtectedBufferManager::CreateProtectedBufferAllocator(
    scoped_refptr<ProtectedBufferManager> protected_buffer_manager) {
  uint64_t allocator_id = 0;
  if (!protected_buffer_manager->GetAllocatorId(&allocator_id))
    return nullptr;

  return std::make_unique<ProtectedBufferAllocatorImpl>(
      allocator_id, protected_buffer_manager,
      base::BindOnce(&ProtectedBufferManager::ReleaseAllProtectedBuffers,
                     protected_buffer_manager, allocator_id));
}

bool ProtectedBufferManager::AllocateProtectedSharedMemory(
    uint64_t allocator_id,
    base::ScopedFD dummy_fd,
    size_t size) {
  VLOGF(2) << "allocator_id:" << allocator_id
           << ", dummy_fd: " << dummy_fd.get() << ", size: " << size;

  // Import the |dummy_fd| to produce a unique id for it.
  uint32_t id;
  auto pixmap = ImportDummyFd(std::move(dummy_fd), &id);
  if (!pixmap)
    return false;

  base::AutoLock lock(buffer_map_lock_);
  if (!CanAllocateFor(allocator_id, id))
    return false;

  // Allocate a protected buffer and associate it with the dummy pixmap.
  // The pixmap needs to be stored to ensure the id remains the same for
  // the entire lifetime of the dummy pixmap.
  auto protected_shmem = ProtectedSharedMemory::Create(pixmap, size);
  if (!protected_shmem) {
    VLOGF(1) << "Failed allocating a protected shared memory buffer";
    return false;
  }

  if (!allocator_to_buffers_map_[allocator_id].insert(id).second) {
    VLOGF(1) << "Failed inserting id: " << id
             << " to allocator_to_buffers_map_, allocator_id: " << allocator_id;
    return false;
  }
  // This will always succeed as we find() first in CanAllocateFor().
  buffer_map_.emplace(id, std::move(protected_shmem));
  VLOGF(2) << "New protected shared memory buffer, handle id: " << id;
  return true;
}

bool ProtectedBufferManager::AllocateProtectedNativePixmap(
    uint64_t allocator_id,
    base::ScopedFD dummy_fd,
    gfx::BufferFormat format,
    const gfx::Size& size) {
  VLOGF(2) << "allocator_id: " << allocator_id
           << ", dummy_fd: " << dummy_fd.get()
           << ", format: " << static_cast<int>(format)
           << ", size: " << size.ToString();

  // Import the |dummy_fd| to produce a unique id for it.
  uint32_t id = 0;
  auto pixmap = ImportDummyFd(std::move(dummy_fd), &id);
  if (!pixmap)
    return false;

  base::AutoLock lock(buffer_map_lock_);
  if (!CanAllocateFor(allocator_id, id))
    return false;

  // Allocate a protected buffer and associate it with the dummy pixmap.
  // The pixmap needs to be stored to ensure the id remains the same for
  // the entire lifetime of the dummy pixmap.
  auto protected_pixmap = ProtectedNativePixmap::Create(pixmap, format, size);
  if (!protected_pixmap) {
    VLOGF(1) << "Failed allocating a protected native pixmap";
    return false;
  }

  if (!allocator_to_buffers_map_[allocator_id].insert(id).second) {
    VLOGF(1) << "Failed inserting id: " << id
             << " to allocator_to_buffers_map_, allocator_id: " << allocator_id;
    return false;
  }
  // This will always succeed as we find() first in CanAllocateFor().
  buffer_map_.emplace(id, std::move(protected_pixmap));
  VLOGF(2) << "New protected native pixmap, handle id: " << id;
  return true;
}

void ProtectedBufferManager::ReleaseProtectedBuffer(uint64_t allocator_id,
                                                    base::ScopedFD dummy_fd) {
  VLOGF(2) << "allocator_id: " << allocator_id
           << ", dummy_fd: " << dummy_fd.get();

  // Import the |dummy_fd| to produce a unique id for it.
  uint32_t id = 0;
  auto pixmap = ImportDummyFd(std::move(dummy_fd), &id);
  if (!pixmap)
    return;

  base::AutoLock lock(buffer_map_lock_);
  RemoveEntry(id);

  auto it = allocator_to_buffers_map_.find(allocator_id);
  if (it == allocator_to_buffers_map_.end()) {
    VLOGF(1) << "No allocated buffer by allocator id " << allocator_id;
    return;
  }
  if (it->second.erase(id) != 1)
    VLOGF(1) << "No buffer id " << id << " to destroy";
}

void ProtectedBufferManager::ReleaseAllProtectedBuffers(uint64_t allocator_id) {
  base::AutoLock lock(buffer_map_lock_);
  auto it = allocator_to_buffers_map_.find(allocator_id);
  if (it == allocator_to_buffers_map_.end())
    return;

  for (const uint32_t id : it->second) {
    RemoveEntry(id);
  }
  allocator_to_buffers_map_.erase(allocator_id);
}

base::UnsafeSharedMemoryRegion
ProtectedBufferManager::GetProtectedSharedMemoryRegionFor(
    base::ScopedFD dummy_fd) {
  uint32_t id = 0;
  auto pixmap = ImportDummyFd(std::move(dummy_fd), &id);
  if (!pixmap)
    return {};

  base::AutoLock lock(buffer_map_lock_);
  const auto& iter = buffer_map_.find(id);
  if (iter == buffer_map_.end())
    return {};

  return iter->second->DuplicateSharedMemoryRegion();
}

gfx::NativePixmapHandle
ProtectedBufferManager::GetProtectedNativePixmapHandleFor(
    base::ScopedFD dummy_fd) {
  uint32_t id = 0;
  auto pixmap = ImportDummyFd(std::move(dummy_fd), &id);
  if (!pixmap)
    return gfx::NativePixmapHandle();

  base::AutoLock lock(buffer_map_lock_);
  const auto& iter = buffer_map_.find(id);
  if (iter == buffer_map_.end())
    return gfx::NativePixmapHandle();

  return iter->second->DuplicateNativePixmapHandle();
}

void ProtectedBufferManager::GetProtectedSharedMemoryRegionFor(
    base::ScopedFD dummy_fd,
    GetProtectedSharedMemoryRegionForResponseCB response_cb) {
  std::move(response_cb)
      .Run(GetProtectedSharedMemoryRegionFor(std::move(dummy_fd)));
}

void ProtectedBufferManager::GetProtectedNativePixmapHandleFor(
    base::ScopedFD dummy_fd,
    GetProtectedNativePixmapHandleForResponseCB response_cb) {
  std::move(response_cb)
      .Run(GetProtectedNativePixmapHandleFor(std::move(dummy_fd)));
}

scoped_refptr<gfx::NativePixmap>
ProtectedBufferManager::GetProtectedNativePixmapFor(
    const gfx::NativePixmapHandle& handle) {
  // Only the first fd is used for lookup.
  if (handle.planes.empty())
    return nullptr;

  base::ScopedFD dummy_fd(HANDLE_EINTR(dup(handle.planes[0].fd.get())));
  uint32_t id = 0;
  auto pixmap = ImportDummyFd(std::move(dummy_fd), &id);
  if (!pixmap)
    return nullptr;

  base::AutoLock lock(buffer_map_lock_);
  const auto& iter = buffer_map_.find(id);
  if (iter == buffer_map_.end())
    return nullptr;

  return iter->second->GetNativePixmap();
}

bool ProtectedBufferManager::IsProtectedNativePixmapHandle(
    base::ScopedFD dummy_fd) {
  uint32_t id = 0;
  auto pixmap = ImportDummyFd(std::move(dummy_fd), &id);
  if (!pixmap)
    return false;

  base::AutoLock lock(buffer_map_lock_);
  const auto& iter = buffer_map_.find(id);
  return iter != buffer_map_.end();
}

scoped_refptr<gfx::NativePixmap> ProtectedBufferManager::ImportDummyFd(
    base::ScopedFD dummy_fd,
    uint32_t* id) const {
  // 0 is an invalid handle id.
  *id = 0;
  if (!dummy_fd.is_valid())
    return nullptr;

  // Import dummy_fd to acquire its unique id.
  // CreateNativePixmapFromHandle() takes ownership and will close the handle
  // also on failure.
  gfx::NativePixmapHandle pixmap_handle;
  pixmap_handle.planes.emplace_back(
      gfx::NativePixmapPlane(0, 0, 0, std::move(dummy_fd)));
  ui::OzonePlatform* platform = ui::OzonePlatform::GetInstance();
  ui::SurfaceFactoryOzone* factory = platform->GetSurfaceFactoryOzone();
  scoped_refptr<gfx::NativePixmap> pixmap =
      factory->CreateNativePixmapForProtectedBufferHandle(
          gfx::kNullAcceleratedWidget, kDummyBufferSize,
          gfx::BufferFormat::RGBA_8888, std::move(pixmap_handle));
  if (!pixmap) {
    VLOGF(1) << "Failed importing dummy handle";
    return nullptr;
  }

  *id = pixmap->GetUniqueId();
  if (*id == 0) {
    VLOGF(1) << "Failed acquiring unique id for handle";
    return nullptr;
  }

  return pixmap;
}

void ProtectedBufferManager::RemoveEntry(uint32_t id) {
  VLOGF(2) << "id: " << id;
  auto num_erased = buffer_map_.erase(id);
  if (num_erased != 1)
    VLOGF(1) << "No buffer id " << id << " to destroy";
}

bool ProtectedBufferManager::CanAllocateFor(uint64_t allocator_id,
                                            uint32_t id) {
  if (buffer_map_.find(id) != buffer_map_.end()) {
    VLOGF(1) << "A protected buffer for this handle already exists";
    return false;
  }

  auto it = allocator_to_buffers_map_.find(allocator_id);
  if (it == allocator_to_buffers_map_.end()) {
    VLOGF(1) << "allocator_to_buffers_map_ has no entry, allocator_id="
             << allocator_id;
    return false;
  }
  auto& allocated_protected_buffer_ids = it->second;
  // Check if the number of allocated protected buffers for |allocator_id| is
  // less than kMaxBuffersPerAllocator.
  if (allocated_protected_buffer_ids.size() >= kMaxBuffersPerAllocator) {
    VLOGF(1) << "Too many allocated protected buffers: "
             << kMaxBuffersPerAllocator;
    return false;
  }
  return true;
}
}  // namespace arc
