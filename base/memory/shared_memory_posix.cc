// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/memory/shared_memory.h"

#include <errno.h>
#include <fcntl.h>
#include <stddef.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <unistd.h>

#include "base/files/scoped_file.h"
#include "base/logging.h"
#include "base/macros.h"
#include "base/memory/shared_memory_helper.h"
#include "base/memory/shared_memory_tracker.h"
#include "base/posix/eintr_wrapper.h"
#include "base/posix/safe_strerror.h"
#include "base/process/process_metrics.h"
#include "base/scoped_generic.h"
#include "base/strings/utf_string_conversions.h"
#include "base/threading/thread_restrictions.h"
#include "base/trace_event/trace_event.h"
#include "base/unguessable_token.h"
#include "build/build_config.h"

#if defined(OS_ANDROID)
#include "base/os_compat_android.h"
#include "third_party/ashmem/ashmem.h"
#endif

#if defined(OS_MACOSX) && !defined(OS_IOS)
#error "MacOS uses shared_memory_mac.cc"
#endif

namespace base {

SharedMemory::SharedMemory() = default;

SharedMemory::SharedMemory(const SharedMemoryHandle& handle, bool read_only)
    : shm_(handle), read_only_(read_only) {}

SharedMemory::~SharedMemory() {
  Unmap();
  Close();
}

// static
bool SharedMemory::IsHandleValid(const SharedMemoryHandle& handle) {
  return handle.IsValid();
}

// static
void SharedMemory::CloseHandle(const SharedMemoryHandle& handle) {
  DCHECK(handle.IsValid());
  handle.Close();
}

// static
SharedMemoryHandle SharedMemory::DuplicateHandle(
    const SharedMemoryHandle& handle) {
  return handle.Duplicate();
}

// static
int SharedMemory::GetFdFromSharedMemoryHandle(
    const SharedMemoryHandle& handle) {
  return handle.GetHandle();
}

bool SharedMemory::CreateAndMapAnonymous(size_t size) {
  return CreateAnonymous(size) && Map(size);
}

#if !defined(OS_ANDROID)

// This SharedMemory API uses only the unique/private shmem.
bool SharedMemory::Create(const SharedMemoryCreateOptions& options) {
  DCHECK(!shm_.IsValid());
  if (options.size == 0) return false;

  if (options.size > static_cast<size_t>(std::numeric_limits<int>::max()))
    return false;

  // This function theoretically can block on the disk, but realistically
  // the temporary files we create will just go into the buffer cache
  // and be deleted before they ever make it out to disk.
  ThreadRestrictions::ScopedAllowIO allow_io;

  ScopedFD fd;
  ScopedFD readonly_fd;
  FilePath path;
  if (!CreateAnonymousSharedMemory(options, &fd, &readonly_fd, &path))
    return false;

  if (fd.is_valid()) {
    // Get current size.
    struct stat stat;
    if (fstat(fd.get(), &stat) != 0)
      return false;
    const size_t current_size = stat.st_size;
    if (current_size != options.size) {
#if defined(OS_LINUX)
      // When /dev/shm becomes full, writing memory to a mapped region of
      // shared memory causes a SIGBUS and kills the process. This is
      // inconvenient to us because: (1) we'll get many different crash
      // reports at random places that write to shared memory, and (2)
      // process killed by SIGBUS confuses many developers. See
      // crbug.com/1014296 for details.
      //
      // Here we preallocate memory by posix_fallocate and detect OOM (ENOSPC)
      // early to avoid getting killed by SIGBUS.

      // posix_fallocate doesn't use errno and returns the error number
      // directly. Thus EINTR is handled manually.
      int result;
      do {
        result = posix_fallocate(fd.get(), 0, options.size);
        if (result != 0 && result != EINTR)
          return false;
      } while (result != 0);
#else
      if (HANDLE_EINTR(ftruncate(fd.get(), options.size)) != 0)
        return false;
#endif
    }
    requested_size_ = options.size;
  } else {
    PLOG(ERROR) << "Creating shared memory in " << path.value() << " failed";
    FilePath dir = path.DirName();
    if (access(dir.value().c_str(), W_OK | X_OK) < 0) {
      PLOG(ERROR) << "Unable to access(W_OK|X_OK) " << dir.value();
      if (dir.value() == "/dev/shm") {
        LOG(FATAL) << "This is frequently caused by incorrect permissions on "
                   << "/dev/shm.  Try 'sudo chmod 1777 /dev/shm' to fix.";
      }
    }
    return false;
  }

  int mapped_file = -1;
  int readonly_mapped_file = -1;

  bool result = PrepareMapFile(std::move(fd), std::move(readonly_fd),
                               &mapped_file, &readonly_mapped_file);
  shm_ = SharedMemoryHandle(FileDescriptor(mapped_file, false), options.size,
                            UnguessableToken::Create());
  readonly_shm_ =
      SharedMemoryHandle(FileDescriptor(readonly_mapped_file, false),
                         options.size, shm_.GetGUID());
  return result;
}
#endif  // !defined(OS_ANDROID)

bool SharedMemory::MapAt(off_t offset, size_t bytes) {
  if (!shm_.IsValid())
    return false;

  if (bytes > static_cast<size_t>(std::numeric_limits<int>::max()))
    return false;

  if (memory_)
    return false;

#if defined(OS_ANDROID)
  // On Android, Map can be called with a size and offset of zero to use the
  // ashmem-determined size.
  if (bytes == 0) {
    DCHECK_EQ(0, offset);
    int ashmem_bytes = ashmem_get_size_region(shm_.GetHandle());
    if (ashmem_bytes < 0)
      return false;
    bytes = ashmem_bytes;
  }

  // Sanity check. This shall catch invalid uses of the SharedMemory APIs
  // but will not protect against direct mmap() attempts.
  if (shm_.IsReadOnly()) {
    // Use a DCHECK() to call writable mappings with read-only descriptors
    // in debug builds immediately. Return an error for release builds
    // or during unit-testing (assuming a ScopedLogAssertHandler was installed).
    DCHECK(read_only_)
        << "Trying to map a region writable with a read-only descriptor.";
    if (!read_only_) {
      return false;
    }
    if (!shm_.SetRegionReadOnly()) {  // Ensure the region is read-only.
      return false;
    }
  }
#endif

  memory_ = mmap(nullptr, bytes, PROT_READ | (read_only_ ? 0 : PROT_WRITE),
                 MAP_SHARED, shm_.GetHandle(), offset);

  bool mmap_succeeded = memory_ && memory_ != reinterpret_cast<void*>(-1);
  if (mmap_succeeded) {
    mapped_size_ = bytes;
    mapped_id_ = shm_.GetGUID();
    DCHECK_EQ(0U,
              reinterpret_cast<uintptr_t>(memory_) &
                  (SharedMemory::MAP_MINIMUM_ALIGNMENT - 1));
    SharedMemoryTracker::GetInstance()->IncrementMemoryUsage(*this);
  } else {
    memory_ = nullptr;
  }

  return mmap_succeeded;
}

bool SharedMemory::Unmap() {
  if (!memory_)
    return false;

  SharedMemoryTracker::GetInstance()->DecrementMemoryUsage(*this);
  munmap(memory_, mapped_size_);
  memory_ = nullptr;
  mapped_size_ = 0;
  mapped_id_ = UnguessableToken();
  return true;
}

SharedMemoryHandle SharedMemory::handle() const {
  return shm_;
}

SharedMemoryHandle SharedMemory::TakeHandle() {
  SharedMemoryHandle handle_copy = shm_;
  handle_copy.SetOwnershipPassesToIPC(true);
  Unmap();
  shm_ = SharedMemoryHandle();
  return handle_copy;
}

#if !defined(OS_ANDROID)
void SharedMemory::Close() {
  if (shm_.IsValid()) {
    shm_.Close();
    shm_ = SharedMemoryHandle();
  }
  if (readonly_shm_.IsValid()) {
    readonly_shm_.Close();
    readonly_shm_ = SharedMemoryHandle();
  }
}

SharedMemoryHandle SharedMemory::GetReadOnlyHandle() const {
  CHECK(readonly_shm_.IsValid());
  return readonly_shm_.Duplicate();
}
#endif  // !defined(OS_ANDROID)

}  // namespace base
