// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/memory/shared_memory.h"

#include <errno.h>
#include <mach/mach_vm.h>
#include <stddef.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <unistd.h>

#include "base/logging.h"
#include "base/mac/foundation_util.h"
#include "base/mac/mac_util.h"
#include "base/mac/scoped_mach_vm.h"
#include "base/memory/shared_memory_helper.h"
#include "base/memory/shared_memory_tracker.h"
#include "base/metrics/field_trial.h"
#include "base/metrics/histogram_macros.h"
#include "base/posix/safe_strerror.h"
#include "base/process/process_metrics.h"
#include "base/scoped_generic.h"
#include "base/strings/utf_string_conversions.h"
#include "base/unguessable_token.h"
#include "build/build_config.h"

#if defined(OS_IOS)
#error "MacOS only - iOS uses shared_memory_posix.cc"
#endif

namespace base {

namespace {

// Returns whether the operation succeeded.
// |new_handle| is an output variable, populated on success. The caller takes
// ownership of the underlying memory object.
// |handle| is the handle to copy.
// If |handle| is already mapped, |mapped_addr| is its mapped location.
// Otherwise, |mapped_addr| should be |nullptr|.
bool MakeMachSharedMemoryHandleReadOnly(SharedMemoryHandle* new_handle,
                                        SharedMemoryHandle handle,
                                        void* mapped_addr) {
  if (!handle.IsValid())
    return false;

  size_t size = handle.GetSize();

  // Map if necessary.
  void* temp_addr = mapped_addr;
  base::mac::ScopedMachVM scoper;
  if (!temp_addr) {
    // Intentionally lower current prot and max prot to |VM_PROT_READ|.
    kern_return_t kr = mach_vm_map(
        mach_task_self(), reinterpret_cast<mach_vm_address_t*>(&temp_addr),
        size, 0, VM_FLAGS_ANYWHERE, handle.GetMemoryObject(), 0, FALSE,
        VM_PROT_READ, VM_PROT_READ, VM_INHERIT_NONE);
    if (kr != KERN_SUCCESS)
      return false;
    scoper.reset(reinterpret_cast<vm_address_t>(temp_addr),
                 mach_vm_round_page(size));
  }

  // Make new memory object.
  mach_port_t named_right;
  kern_return_t kr = mach_make_memory_entry_64(
      mach_task_self(), reinterpret_cast<memory_object_size_t*>(&size),
      reinterpret_cast<memory_object_offset_t>(temp_addr), VM_PROT_READ,
      &named_right, MACH_PORT_NULL);
  if (kr != KERN_SUCCESS)
    return false;

  *new_handle = SharedMemoryHandle(named_right, size, handle.GetGUID());
  return true;
}

}  // namespace

SharedMemory::SharedMemory() {}

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
  handle.Close();
}

// static
SharedMemoryHandle SharedMemory::DuplicateHandle(
    const SharedMemoryHandle& handle) {
  return handle.Duplicate();
}

bool SharedMemory::CreateAndMapAnonymous(size_t size) {
  return CreateAnonymous(size) && Map(size);
}

// Chromium mostly only uses the unique/private shmem as specified by
// "name == L"". The exception is in the StatsTable.
bool SharedMemory::Create(const SharedMemoryCreateOptions& options) {
  DCHECK(!shm_.IsValid());
  if (options.size == 0)
    return false;

  if (options.size > static_cast<size_t>(std::numeric_limits<int>::max()))
    return false;

  shm_ = SharedMemoryHandle(options.size, UnguessableToken::Create());
  requested_size_ = options.size;
  return shm_.IsValid();
}

bool SharedMemory::MapAt(off_t offset, size_t bytes) {
  if (!shm_.IsValid())
    return false;
  if (bytes > static_cast<size_t>(std::numeric_limits<int>::max()))
    return false;
  if (memory_)
    return false;

  bool success = shm_.MapAt(offset, bytes, &memory_, read_only_);
  if (success) {
    mapped_size_ = bytes;
    DCHECK_EQ(0U, reinterpret_cast<uintptr_t>(memory_) &
                      (SharedMemory::MAP_MINIMUM_ALIGNMENT - 1));
    mapped_id_ = shm_.GetGUID();
    SharedMemoryTracker::GetInstance()->IncrementMemoryUsage(*this);
  } else {
    memory_ = nullptr;
  }

  return success;
}

bool SharedMemory::Unmap() {
  if (!memory_)
    return false;

  SharedMemoryTracker::GetInstance()->DecrementMemoryUsage(*this);
      mach_vm_deallocate(mach_task_self(),
                         reinterpret_cast<mach_vm_address_t>(memory_),
                         mapped_size_);
  memory_ = nullptr;
  mapped_size_ = 0;
  mapped_id_ = UnguessableToken();
  return true;
}

SharedMemoryHandle SharedMemory::handle() const {
  return shm_;
}

SharedMemoryHandle SharedMemory::TakeHandle() {
  SharedMemoryHandle dup = DuplicateHandle(handle());
  Unmap();
  Close();
  return dup;
}

void SharedMemory::Close() {
  shm_.Close();
  shm_ = SharedMemoryHandle();
}

SharedMemoryHandle SharedMemory::GetReadOnlyHandle() const {
  DCHECK(shm_.IsValid());
  SharedMemoryHandle new_handle;
  bool success = MakeMachSharedMemoryHandleReadOnly(&new_handle, shm_, memory_);
  if (success)
    new_handle.SetOwnershipPassesToIPC(true);
  return new_handle;
}

}  // namespace base
