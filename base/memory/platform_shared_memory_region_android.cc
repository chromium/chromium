// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/memory/platform_shared_memory_region.h"

#include <sys/mman.h>

#include "base/bits.h"
#include "base/logging.h"
#include "base/memory/page_size.h"
#include "base/memory/shared_memory_tracker.h"
#include "base/metrics/histogram_macros.h"
#include "base/posix/eintr_wrapper.h"
#include "third_party/ashmem/ashmem.h"

namespace base {
namespace subtle {

// For Android, we use ashmem to implement SharedMemory. ashmem_create_region
// will automatically pin the region. We never explicitly call pin/unpin. When
// all the file descriptors from different processes associated with the region
// are closed, the memory buffer will go away.

namespace {

int GetAshmemRegionProtectionMask(int fd) {
  int prot = ashmem_get_prot_region(fd);
  if (prot < 0) {
    // TODO(crbug.com/40574272): convert to DLOG when bug fixed.
    PLOG(ERROR) << "ashmem_get_prot_region failed";
    return -1;
  }
  return prot;
}

}  // namespace

// static
PlatformSharedMemoryRegion PlatformSharedMemoryRegion::Take(
    ScopedFD fd,
    Mode mode,
    size_t size,
    const UnguessableToken& guid) {
  if (!fd.is_valid())
    return {};

  if (size == 0)
    return {};

  if (size > static_cast<size_t>(std::numeric_limits<int>::max()))
    return {};

  CHECK(CheckPlatformHandlePermissionsCorrespondToMode(fd.get(), mode, size));

  return PlatformSharedMemoryRegion(std::move(fd), mode, size, guid);
}

int PlatformSharedMemoryRegion::GetPlatformHandle() const {
  return handle_.get();
}

bool PlatformSharedMemoryRegion::IsValid() const {
  return handle_.is_valid();
}

PlatformSharedMemoryRegion PlatformSharedMemoryRegion::Duplicate() const {
  if (!IsValid())
    return {};

  CHECK_NE(mode_, Mode::kWritable)
      << "Duplicating a writable shared memory region is prohibited";

  ScopedFD duped_fd(HANDLE_EINTR(dup(handle_.get())));
  if (!duped_fd.is_valid()) {
    DPLOG(ERROR) << "dup(" << handle_.get() << ") failed";
    return {};
  }

  return PlatformSharedMemoryRegion(std::move(duped_fd), mode_, size_, guid_);
}

bool PlatformSharedMemoryRegion::ConvertToReadOnly() {
  if (!IsValid())
    return false;

  CHECK_EQ(mode_, Mode::kWritable)
      << "Only writable shared memory region can be converted to read-only";

  ScopedFD handle_copy(handle_.release());

  int prot = GetAshmemRegionProtectionMask(handle_copy.get());
  if (prot < 0)
    return false;

  prot &= ~PROT_WRITE;
  int ret = ashmem_set_prot_region(handle_copy.get(), prot);
  if (ret != 0) {
    DPLOG(ERROR) << "ashmem_set_prot_region failed";
    return false;
  }

  handle_ = std::move(handle_copy);
  mode_ = Mode::kReadOnly;
  return true;
}

bool PlatformSharedMemoryRegion::ConvertToUnsafe() {
  if (!IsValid())
    return false;

  CHECK_EQ(mode_, Mode::kWritable)
      << "Only writable shared memory region can be converted to unsafe";

  mode_ = Mode::kUnsafe;
  return true;
}

// static
PlatformSharedMemoryRegion PlatformSharedMemoryRegion::Create(Mode mode,
                                                              size_t size) {
  if (size == 0) {
    return {};
  }

  // Align size as required by ashmem_create_region() API documentation. This
  // operation may overflow so check that the result doesn't decrease.
  size_t rounded_size = bits::AlignUp(size, GetPageSize());
  if (rounded_size < size ||
      rounded_size > static_cast<size_t>(std::numeric_limits<int>::max())) {
    return {};
  }

  CHECK_NE(mode, Mode::kReadOnly) << "Creating a region in read-only mode will "
                                     "lead to this region being non-modifiable";

  UnguessableToken guid = UnguessableToken::Create();

  int fd = ashmem_create_region(
      SharedMemoryTracker::GetDumpNameForTracing(guid).c_str(), rounded_size);
  if (fd < 0) {
    DPLOG(ERROR) << "ashmem_create_region failed";
    return {};
  }

  ScopedFD scoped_fd(fd);
  int err = ashmem_set_prot_region(scoped_fd.get(), PROT_READ | PROT_WRITE);
  if (err < 0) {
    DPLOG(ERROR) << "ashmem_set_prot_region failed";
    return {};
  }

  return PlatformSharedMemoryRegion(std::move(scoped_fd), mode, size, guid);
}

bool PlatformSharedMemoryRegion::CheckPlatformHandlePermissionsCorrespondToMode(
    PlatformSharedMemoryHandle handle,
    Mode mode,
    size_t size) {
  int prot = GetAshmemRegionProtectionMask(handle);
  if (prot < 0)
    return false;

  bool is_read_only = (prot & PROT_WRITE) == 0;
  bool expected_read_only = mode == Mode::kReadOnly;

  if (is_read_only != expected_read_only) {
    // TODO(crbug.com/40574272): convert to DLOG when bug fixed.
    LOG(ERROR) << "Ashmem region has a wrong protection mask: it is"
               << (is_read_only ? " " : " not ") << "read-only but it should"
               << (expected_read_only ? " " : " not ") << "be";
    return false;
  }

  return true;
}

PlatformSharedMemoryRegion::PlatformSharedMemoryRegion(
    ScopedFD fd,
    Mode mode,
    size_t size,
    const UnguessableToken& guid)
    : handle_(std::move(fd)), mode_(mode), size_(size), guid_(guid) {}

}  // namespace subtle
}  // namespace base
