// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/memory/shared_memory.h"

#include <stddef.h>
#include <sys/mman.h>

#include "base/bits.h"
#include "base/logging.h"
#include "base/process/process_metrics.h"
#include "third_party/ashmem/ashmem.h"

namespace base {

// For Android, we use ashmem to implement SharedMemory. ashmem_create_region
// will automatically pin the region. We never explicitly call pin/unpin. When
// all the file descriptors from different processes associated with the region
// are closed, the memory buffer will go away.

bool SharedMemory::Create(const SharedMemoryCreateOptions& options) {
  DCHECK(!shm_.IsValid());

  // Align size as required by ashmem_create_region() API documentation.
  size_t rounded_size = bits::Align(options.size, GetPageSize());

  if (rounded_size > static_cast<size_t>(std::numeric_limits<int>::max()))
    return false;

  // "name" is just a label in ashmem. It is visible in /proc/pid/maps.
  int fd = ashmem_create_region(
      options.name_deprecated ? options.name_deprecated->c_str() : "",
      rounded_size);
  shm_ = SharedMemoryHandle::ImportHandle(fd, options.size);
  if (!shm_.IsValid()) {
    DLOG(ERROR) << "Shared memory creation failed";
    return false;
  }

  int flags = PROT_READ | PROT_WRITE | (options.executable ? PROT_EXEC : 0);
  int err = ashmem_set_prot_region(shm_.GetHandle(), flags);
  if (err < 0) {
    DLOG(ERROR) << "Error " << err << " when setting protection of ashmem";
    return false;
  }

  requested_size_ = options.size;

  return true;
}

bool SharedMemory::Delete(const std::string& name) {
  // Like on Windows, this is intentionally returning true as ashmem will
  // automatically releases the resource when all FDs on it are closed.
  return true;
}

bool SharedMemory::Open(const std::string& name, bool read_only) {
  // ashmem doesn't support name mapping
  NOTIMPLEMENTED();
  return false;
}

void SharedMemory::Close() {
  if (shm_.IsValid()) {
    shm_.Close();
    shm_ = SharedMemoryHandle();
  }
}

SharedMemoryHandle SharedMemory::GetReadOnlyHandle() const {
  // There are no read-only Ashmem descriptors on Android.
  // Instead, the protection mask is a property of the region itself.
  SharedMemoryHandle handle = shm_.Duplicate();
  handle.SetReadOnly();
  return handle;
}

}  // namespace base
