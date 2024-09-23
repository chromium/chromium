// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/memory/platform_shared_memory_region.h"

#include <aclapi.h>
#include <stddef.h>
#include <stdint.h>

#include "base/bits.h"
#include "base/logging.h"
#include "base/metrics/histogram_functions.h"
#include "base/metrics/histogram_macros.h"
#include "base/process/process_handle.h"
#include "base/strings/string_util.h"
#include "partition_alloc/page_allocator.h"

namespace base::subtle {

namespace {

typedef enum _SECTION_INFORMATION_CLASS {
  SectionBasicInformation,
} SECTION_INFORMATION_CLASS;

typedef struct _SECTION_BASIC_INFORMATION {
  PVOID BaseAddress;
  ULONG Attributes;
  LARGE_INTEGER Size;
} SECTION_BASIC_INFORMATION, *PSECTION_BASIC_INFORMATION;

typedef ULONG(__stdcall* NtQuerySectionType)(
    HANDLE SectionHandle,
    SECTION_INFORMATION_CLASS SectionInformationClass,
    PVOID SectionInformation,
    ULONG SectionInformationLength,
    PULONG ResultLength);

// Checks if the section object is safe to map. At the moment this just means
// it's not an image section.
bool IsSectionSafeToMap(HANDLE handle) {
  static NtQuerySectionType nt_query_section_func =
      reinterpret_cast<NtQuerySectionType>(
          ::GetProcAddress(::GetModuleHandle(L"ntdll.dll"), "NtQuerySection"));
  DCHECK(nt_query_section_func);

  // The handle must have SECTION_QUERY access for this to succeed.
  SECTION_BASIC_INFORMATION basic_information = {};
  ULONG status =
      nt_query_section_func(handle, SectionBasicInformation, &basic_information,
                            sizeof(basic_information), nullptr);
  if (status)
    return false;
  return (basic_information.Attributes & SEC_IMAGE) != SEC_IMAGE;
}

// Returns a HANDLE on success and |nullptr| on failure.
// This function is similar to CreateFileMapping, but removes the permissions
// WRITE_DAC, WRITE_OWNER, READ_CONTROL, and DELETE.
//
// A newly created file mapping has two sets of permissions. It has access
// control permissions (WRITE_DAC, WRITE_OWNER, READ_CONTROL, and DELETE) and
// file permissions (FILE_MAP_READ, FILE_MAP_WRITE, etc.). The Chrome sandbox
// prevents HANDLEs with the WRITE_DAC permission from being duplicated into
// unprivileged processes.
//
// In order to remove the access control permissions, after being created the
// handle is duplicated with only the file access permissions.
HANDLE CreateFileMappingWithReducedPermissions(SECURITY_ATTRIBUTES* sa,
                                               size_t rounded_size,
                                               LPCWSTR name) {
  HANDLE h = CreateFileMapping(INVALID_HANDLE_VALUE, sa, PAGE_READWRITE, 0,
                               static_cast<DWORD>(rounded_size), name);
  if (!h) {
    return nullptr;
  }

  HANDLE h2;
  ProcessHandle process = GetCurrentProcess();
  BOOL success = ::DuplicateHandle(
      process, h, process, &h2, FILE_MAP_READ | FILE_MAP_WRITE | SECTION_QUERY,
      FALSE, 0);
  BOOL rv = ::CloseHandle(h);
  DCHECK(rv);

  if (!success) {
    return nullptr;
  }

  return h2;
}

}  // namespace

// static
PlatformSharedMemoryRegion PlatformSharedMemoryRegion::Take(
    win::ScopedHandle handle,
    Mode mode,
    size_t size,
    const UnguessableToken& guid) {
  if (!handle.is_valid())
    return {};

  if (size == 0)
    return {};

  if (size > static_cast<size_t>(std::numeric_limits<int>::max()))
    return {};

  if (!IsSectionSafeToMap(handle.get()))
    return {};

  CHECK(
      CheckPlatformHandlePermissionsCorrespondToMode(handle.get(), mode, size));

  return PlatformSharedMemoryRegion(std::move(handle), mode, size, guid);
}

HANDLE PlatformSharedMemoryRegion::GetPlatformHandle() const {
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

  HANDLE duped_handle;
  ProcessHandle process = GetCurrentProcess();
  BOOL success =
      ::DuplicateHandle(process, handle_.get(), process, &duped_handle, 0,
                        FALSE, DUPLICATE_SAME_ACCESS);
  if (!success)
    return {};

  return PlatformSharedMemoryRegion(win::ScopedHandle(duped_handle), mode_,
                                    size_, guid_);
}

bool PlatformSharedMemoryRegion::ConvertToReadOnly() {
  if (!IsValid())
    return false;

  CHECK_EQ(mode_, Mode::kWritable)
      << "Only writable shared memory region can be converted to read-only";

  win::ScopedHandle handle_copy(handle_.release());

  HANDLE duped_handle;
  ProcessHandle process = GetCurrentProcess();
  BOOL success =
      ::DuplicateHandle(process, handle_copy.get(), process, &duped_handle,
                        FILE_MAP_READ | SECTION_QUERY, FALSE, 0);
  if (!success)
    return false;

  handle_.Set(duped_handle);
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
  // TODO(crbug.com/40307662): NaCl forces us to round up 64k here, wasting 32k
  // per mapping on average.
  static const size_t kSectionSize = 65536;
  if (size == 0) {
    return {};
  }

  // Aligning may overflow so check that the result doesn't decrease.
  size_t rounded_size = bits::AlignUp(size, kSectionSize);
  if (rounded_size < size ||
      rounded_size > static_cast<size_t>(std::numeric_limits<int>::max())) {
    return {};
  }

  CHECK_NE(mode, Mode::kReadOnly) << "Creating a region in read-only mode will "
                                     "lead to this region being non-modifiable";

  // Add an empty DACL to enforce anonymous read-only sections.
  ACL dacl;
  SECURITY_DESCRIPTOR sd;
  if (!InitializeAcl(&dacl, sizeof(dacl), ACL_REVISION)) {
    return {};
  }
  if (!InitializeSecurityDescriptor(&sd, SECURITY_DESCRIPTOR_REVISION)) {
    return {};
  }
  if (!SetSecurityDescriptorDacl(&sd, TRUE, &dacl, FALSE)) {
    return {};
  }

  std::u16string name;
  SECURITY_ATTRIBUTES sa = {sizeof(sa), &sd, FALSE};
  // Ask for the file mapping with reduced permisions to avoid passing the
  // access control permissions granted by default into unpriviledged process.
  HANDLE h = CreateFileMappingWithReducedPermissions(
      &sa, rounded_size, name.empty() ? nullptr : as_wcstr(name));
  if (h == nullptr) {
    // The error is logged within CreateFileMappingWithReducedPermissions().
    return {};
  }

  win::ScopedHandle scoped_h(h);
  // Check if the shared memory pre-exists.
  if (GetLastError() == ERROR_ALREADY_EXISTS) {
    return {};
  }

  return PlatformSharedMemoryRegion(std::move(scoped_h), mode, size,
                                    UnguessableToken::Create());
}

// static
bool PlatformSharedMemoryRegion::CheckPlatformHandlePermissionsCorrespondToMode(
    PlatformSharedMemoryHandle handle,
    Mode mode,
    size_t size) {
  // Call ::DuplicateHandle() with FILE_MAP_WRITE as a desired access to check
  // if the |handle| has a write access.
  ProcessHandle process = GetCurrentProcess();
  HANDLE duped_handle;
  BOOL success = ::DuplicateHandle(process, handle, process, &duped_handle,
                                   FILE_MAP_WRITE, FALSE, 0);
  if (success) {
    BOOL rv = ::CloseHandle(duped_handle);
    DCHECK(rv);
  }

  bool is_read_only = !success;
  bool expected_read_only = mode == Mode::kReadOnly;

  if (is_read_only != expected_read_only) {
    DLOG(ERROR) << "File mapping handle has wrong access rights: it is"
                << (is_read_only ? " " : " not ") << "read-only but it should"
                << (expected_read_only ? " " : " not ") << "be";
    return false;
  }

  return true;
}

PlatformSharedMemoryRegion::PlatformSharedMemoryRegion(
    win::ScopedHandle handle,
    Mode mode,
    size_t size,
    const UnguessableToken& guid)
    : handle_(std::move(handle)), mode_(mode), size_(size), guid_(guid) {}

}  // namespace base::subtle
