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

#include <limits>

#include "base/logging.h"
#include "base/memory/shared_memory_tracker.h"

namespace base {

SharedMemory::SharedMemory()
    : mapped_size_(0), memory_(NULL), read_only_(false), requested_size_(0) {}

SharedMemory::SharedMemory(const SharedMemoryHandle& handle, bool read_only)
    : shm_(handle),
      mapped_size_(0),
      memory_(NULL),
      read_only_(read_only),
      requested_size_(0) {}

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

bool SharedMemory::CreateAndMapAnonymous(size_t size) {
  // Untrusted code can't create descriptors or handles.
  return false;
}

bool SharedMemory::Create(const SharedMemoryCreateOptions& options) {
  // Untrusted code can't create descriptors or handles.
  return false;
}

bool SharedMemory::MapAt(off_t offset, size_t bytes) {
  if (!shm_.IsValid())
    return false;

  if (bytes > static_cast<size_t>(std::numeric_limits<int>::max()))
    return false;

  if (memory_)
    return false;

  memory_ = mmap(NULL, bytes, PROT_READ | (read_only_ ? 0 : PROT_WRITE),
                 MAP_SHARED, shm_.GetHandle(), offset);

  bool mmap_succeeded = memory_ != MAP_FAILED && memory_ != NULL;
  if (mmap_succeeded) {
    mapped_size_ = bytes;
    DCHECK_EQ(0U, reinterpret_cast<uintptr_t>(memory_) &
        (SharedMemory::MAP_MINIMUM_ALIGNMENT - 1));
    mapped_id_ = shm_.GetGUID();
    SharedMemoryTracker::GetInstance()->IncrementMemoryUsage(*this);
  } else {
    memory_ = NULL;
  }

  return mmap_succeeded;
}

bool SharedMemory::Unmap() {
  if (memory_ == NULL)
    return false;

  SharedMemoryTracker::GetInstance()->DecrementMemoryUsage(*this);
  if (munmap(memory_, mapped_size_) < 0)
    DPLOG(ERROR) << "munmap";
  memory_ = NULL;
  mapped_size_ = 0;
  mapped_id_ = UnguessableToken();
  return true;
}

SharedMemoryHandle SharedMemory::handle() const {
  SharedMemoryHandle handle_copy = shm_;
  handle_copy.SetOwnershipPassesToIPC(false);
  return handle_copy;
}

SharedMemoryHandle SharedMemory::TakeHandle() {
  SharedMemoryHandle handle_copy = shm_;
  handle_copy.SetOwnershipPassesToIPC(true);
  Unmap();
  shm_ = SharedMemoryHandle();
  return handle_copy;
}

void SharedMemory::Close() {
  if (shm_.IsValid()) {
    shm_.Close();
    shm_ = SharedMemoryHandle();
  }
}

SharedMemoryHandle SharedMemory::GetReadOnlyHandle() const {
  // Untrusted code can't create descriptors or handles, which is needed to
  // drop permissions.
  return SharedMemoryHandle();
}

}  // namespace base
