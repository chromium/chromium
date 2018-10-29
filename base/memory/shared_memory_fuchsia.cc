// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/memory/shared_memory.h"

#include <limits>

#include <lib/zx/vmar.h>
#include <lib/zx/vmo.h>
#include <zircon/rights.h>

#include "base/bits.h"
#include "base/fuchsia/fuchsia_logging.h"
#include "base/memory/shared_memory_tracker.h"
#include "base/process/process_metrics.h"

namespace base {

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
  DCHECK(handle.IsValid());
  handle.Close();
}

// static
size_t SharedMemory::GetHandleLimit() {
  // Duplicated from the internal Magenta kernel constant kMaxHandleCount
  // (kernel/lib/zircon/zircon.cpp).
  return 256 * 1024u;
}

bool SharedMemory::CreateAndMapAnonymous(size_t size) {
  return CreateAnonymous(size) && Map(size);
}

bool SharedMemory::Create(const SharedMemoryCreateOptions& options) {
  requested_size_ = options.size;
  mapped_size_ = bits::Align(requested_size_, GetPageSize());
  zx::vmo vmo;
  zx_status_t status =
      zx::vmo::create(mapped_size_, ZX_VMO_NON_RESIZABLE, &vmo);
  if (status != ZX_OK) {
    ZX_DLOG(ERROR, status) << "zx_vmo_create";
    return false;
  }

  if (!options.executable) {
    // If options.executable isn't set, drop that permission by replacement.
    const int kNoExecFlags = ZX_DEFAULT_VMO_RIGHTS & ~ZX_RIGHT_EXECUTE;
    status = vmo.replace(kNoExecFlags, &vmo);
    if (status != ZX_OK) {
      ZX_DLOG(ERROR, status) << "zx_handle_replace";
      return false;
    }
  }

  shm_ = SharedMemoryHandle(vmo.release(), mapped_size_,
                            UnguessableToken::Create());
  return true;
}

bool SharedMemory::MapAt(off_t offset, size_t bytes) {
  if (!shm_.IsValid())
    return false;

  if (bytes > static_cast<size_t>(std::numeric_limits<int>::max()))
    return false;

  if (memory_)
    return false;

  zx_vm_option_t options = ZX_VM_REQUIRE_NON_RESIZABLE | ZX_VM_FLAG_PERM_READ;
  if (!read_only_)
    options |= ZX_VM_FLAG_PERM_WRITE;
  uintptr_t addr;
  zx_status_t status = zx::vmar::root_self()->map(
      /*vmar_offset=*/0, *zx::unowned_vmo(shm_.GetHandle()), offset, bytes,
      options, &addr);
  if (status != ZX_OK) {
    ZX_DLOG(ERROR, status) << "zx_vmar_map";
    return false;
  }
  memory_ = reinterpret_cast<void*>(addr);

  mapped_size_ = bytes;
  mapped_id_ = shm_.GetGUID();
  SharedMemoryTracker::GetInstance()->IncrementMemoryUsage(*this);
  return true;
}

bool SharedMemory::Unmap() {
  if (!memory_)
    return false;

  SharedMemoryTracker::GetInstance()->DecrementMemoryUsage(*this);

  uintptr_t addr = reinterpret_cast<uintptr_t>(memory_);
  zx_status_t status = zx::vmar::root_self()->unmap(addr, mapped_size_);
  if (status != ZX_OK) {
    ZX_DLOG(ERROR, status) << "zx_vmar_unmap";
    return false;
  }

  memory_ = nullptr;
  mapped_id_ = UnguessableToken();
  return true;
}

void SharedMemory::Close() {
  if (shm_.IsValid()) {
    shm_.Close();
    shm_ = SharedMemoryHandle();
  }
}

SharedMemoryHandle SharedMemory::handle() const {
  return shm_;
}

SharedMemoryHandle SharedMemory::TakeHandle() {
  SharedMemoryHandle handle(shm_);
  handle.SetOwnershipPassesToIPC(true);
  Unmap();
  shm_ = SharedMemoryHandle();
  return handle;
}

SharedMemoryHandle SharedMemory::DuplicateHandle(
    const SharedMemoryHandle& handle) {
  return handle.Duplicate();
}

SharedMemoryHandle SharedMemory::GetReadOnlyHandle() const {
  zx::vmo duped_handle;
  const int kNoWriteOrExec =
      ZX_DEFAULT_VMO_RIGHTS &
      ~(ZX_RIGHT_WRITE | ZX_RIGHT_EXECUTE | ZX_RIGHT_SET_PROPERTY);
  zx_status_t status = zx::unowned_vmo(shm_.GetHandle())
                           ->duplicate(kNoWriteOrExec, &duped_handle);
  if (status != ZX_OK)
    return SharedMemoryHandle();

  SharedMemoryHandle handle(duped_handle.release(), shm_.GetSize(),
                            shm_.GetGUID());
  handle.SetOwnershipPassesToIPC(true);
  return handle;
}

}  // namespace base
