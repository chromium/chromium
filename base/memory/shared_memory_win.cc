// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/memory/shared_memory.h"

#include <aclapi.h>
#include <stddef.h>
#include <stdint.h>

#include "base/allocator/partition_allocator/page_allocator.h"
#include "base/logging.h"
#include "base/memory/shared_memory_tracker.h"
#include "base/metrics/histogram_functions.h"
#include "base/metrics/histogram_macros.h"
#include "base/rand_util.h"
#include "base/strings/string_util.h"
#include "base/strings/stringprintf.h"
#include "base/strings/utf_string_conversions.h"
#include "base/unguessable_token.h"
#include "base/win/windows_version.h"

namespace base {
namespace {

// Errors that can occur during Shared Memory construction.
// These match tools/metrics/histograms/histograms.xml.
// This enum is append-only.
enum CreateError {
  SUCCESS = 0,
  SIZE_ZERO = 1,
  SIZE_TOO_LARGE = 2,
  INITIALIZE_ACL_FAILURE = 3,
  INITIALIZE_SECURITY_DESC_FAILURE = 4,
  SET_SECURITY_DESC_FAILURE = 5,
  CREATE_FILE_MAPPING_FAILURE = 6,
  REDUCE_PERMISSIONS_FAILURE = 7,
  ALREADY_EXISTS = 8,
  CREATE_ERROR_LAST = ALREADY_EXISTS
};

// Emits UMA metrics about encountered errors. Pass zero (0) for |winerror|
// if there is no associated Windows error.
void LogError(CreateError error, DWORD winerror) {
  UMA_HISTOGRAM_ENUMERATION("SharedMemory.CreateError", error,
                            CREATE_ERROR_LAST + 1);
  static_assert(ERROR_SUCCESS == 0, "Windows error code changed!");
  if (winerror != ERROR_SUCCESS)
    UmaHistogramSparse("SharedMemory.CreateWinError", winerror);
}

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

// Returns the length of the memory section starting at the supplied address.
size_t GetMemorySectionSize(void* address) {
  MEMORY_BASIC_INFORMATION memory_info;
  if (!::VirtualQuery(address, &memory_info, sizeof(memory_info)))
    return 0;
  return memory_info.RegionSize - (static_cast<char*>(address) -
         static_cast<char*>(memory_info.AllocationBase));
}

// Checks if the section object is safe to map. At the moment this just means
// it's not an image section.
bool IsSectionSafeToMap(HANDLE handle) {
  static NtQuerySectionType nt_query_section_func;
  if (!nt_query_section_func) {
    nt_query_section_func = reinterpret_cast<NtQuerySectionType>(
        ::GetProcAddress(::GetModuleHandle(L"ntdll.dll"), "NtQuerySection"));
    DCHECK(nt_query_section_func);
  }

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
// file permissions (FILE_MAP_READ, FILE_MAP_WRITE, etc.). ::DuplicateHandle()
// with the parameter DUPLICATE_SAME_ACCESS copies both sets of permissions.
//
// The Chrome sandbox prevents HANDLEs with the WRITE_DAC permission from being
// duplicated into unprivileged processes. But the only way to copy file
// permissions is with the parameter DUPLICATE_SAME_ACCESS. This means that
// there is no way for a privileged process to duplicate a file mapping into an
// unprivileged process while maintaining the previous file permissions.
//
// By removing all access control permissions of a file mapping immediately
// after creation, ::DuplicateHandle() effectively only copies the file
// permissions.
HANDLE CreateFileMappingWithReducedPermissions(SECURITY_ATTRIBUTES* sa,
                                               size_t rounded_size,
                                               LPCWSTR name) {
  HANDLE h = CreateFileMapping(INVALID_HANDLE_VALUE, sa, PAGE_READWRITE, 0,
                               static_cast<DWORD>(rounded_size), name);
  if (!h) {
    LogError(CREATE_FILE_MAPPING_FAILURE, GetLastError());
    return nullptr;
  }

  HANDLE h2;
  BOOL success = ::DuplicateHandle(
      GetCurrentProcess(), h, GetCurrentProcess(), &h2,
      FILE_MAP_READ | FILE_MAP_WRITE | SECTION_QUERY, FALSE, 0);
  BOOL rv = ::CloseHandle(h);
  DCHECK(rv);

  if (!success) {
    LogError(REDUCE_PERMISSIONS_FAILURE, GetLastError());
    return nullptr;
  }

  return h2;
}

}  // namespace.

SharedMemory::SharedMemory() {}

SharedMemory::SharedMemory(const SharedMemoryHandle& handle, bool read_only)
    : external_section_(true), shm_(handle), read_only_(read_only) {}

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

bool SharedMemory::Create(const SharedMemoryCreateOptions& options) {
  // TODO(crbug.com/210609): NaCl forces us to round up 64k here, wasting 32k
  // per mapping on average.
  static const size_t kSectionMask = 65536 - 1;
  DCHECK(!options.executable);
  DCHECK(!shm_.IsValid());
  if (options.size == 0) {
    LogError(SIZE_ZERO, 0);
    return false;
  }

  // Check maximum accounting for overflow.
  if (options.size >
      static_cast<size_t>(std::numeric_limits<int>::max()) - kSectionMask) {
    LogError(SIZE_TOO_LARGE, 0);
    return false;
  }

  size_t rounded_size = (options.size + kSectionMask) & ~kSectionMask;
  SECURITY_ATTRIBUTES sa = {sizeof(sa), nullptr, FALSE};
  SECURITY_DESCRIPTOR sd;
  ACL dacl;

  // Add an empty DACL to enforce anonymous read-only sections.
  sa.lpSecurityDescriptor = &sd;
  if (!InitializeAcl(&dacl, sizeof(dacl), ACL_REVISION)) {
    LogError(INITIALIZE_ACL_FAILURE, GetLastError());
    return false;
  }
  if (!InitializeSecurityDescriptor(&sd, SECURITY_DESCRIPTOR_REVISION)) {
    LogError(INITIALIZE_SECURITY_DESC_FAILURE, GetLastError());
    return false;
  }
  if (!SetSecurityDescriptorDacl(&sd, TRUE, &dacl, FALSE)) {
    LogError(SET_SECURITY_DESC_FAILURE, GetLastError());
    return false;
  }

  string16 name;
  if (win::GetVersion() < win::Version::WIN8_1) {
    // Windows < 8.1 ignores DACLs on certain unnamed objects (like shared
    // sections). So, we generate a random name when we need to enforce
    // read-only.
    uint64_t rand_values[4];
    RandBytes(&rand_values, sizeof(rand_values));
    name = ASCIIToUTF16(StringPrintf("CrSharedMem_%016llx%016llx%016llx%016llx",
                                     rand_values[0], rand_values[1],
                                     rand_values[2], rand_values[3]));
    DCHECK(!name.empty());
  }

  shm_ = SharedMemoryHandle(
      CreateFileMappingWithReducedPermissions(
          &sa, rounded_size, name.empty() ? nullptr : as_wcstr(name)),
      rounded_size, UnguessableToken::Create());
  if (!shm_.IsValid()) {
    // The error is logged within CreateFileMappingWithReducedPermissions().
    return false;
  }

  requested_size_ = options.size;

  // If the shared memory already exists, something has gone wrong.
  if (GetLastError() == ERROR_ALREADY_EXISTS) {
    Close();
    // From "if" above: GetLastError() == ERROR_ALREADY_EXISTS.
    LogError(ALREADY_EXISTS, ERROR_ALREADY_EXISTS);
    return false;
  }

  LogError(SUCCESS, ERROR_SUCCESS);
  return true;
}

bool SharedMemory::MapAt(off_t offset, size_t bytes) {
  if (!shm_.IsValid()) {
    DLOG(ERROR) << "Invalid SharedMemoryHandle.";
    return false;
  }

  if (bytes > static_cast<size_t>(std::numeric_limits<int>::max())) {
    DLOG(ERROR) << "Bytes required exceeds the 2G limitation.";
    return false;
  }

  if (memory_) {
    DLOG(ERROR) << "The SharedMemory has been mapped already.";
    return false;
  }

  if (external_section_ && !IsSectionSafeToMap(shm_.GetHandle())) {
    DLOG(ERROR) << "SharedMemoryHandle is not safe to be mapped.";
    return false;
  }

  // Try to map the shared memory. On the first failure, release any reserved
  // address space for a single retry.
  for (int i = 0; i < 2; ++i) {
    memory_ = MapViewOfFile(
        shm_.GetHandle(),
        read_only_ ? FILE_MAP_READ : FILE_MAP_READ | FILE_MAP_WRITE,
        static_cast<uint64_t>(offset) >> 32, static_cast<DWORD>(offset), bytes);
    if (memory_)
      break;
    ReleaseReservation();
  }
  if (!memory_) {
    DPLOG(ERROR) << "Failed executing MapViewOfFile";
    return false;
  }

  DCHECK_EQ(0U, reinterpret_cast<uintptr_t>(memory_) &
                    (SharedMemory::MAP_MINIMUM_ALIGNMENT - 1));
  mapped_size_ = GetMemorySectionSize(memory_);
  mapped_id_ = shm_.GetGUID();
  SharedMemoryTracker::GetInstance()->IncrementMemoryUsage(*this);
  return true;
}

bool SharedMemory::Unmap() {
  if (!memory_)
    return false;

  SharedMemoryTracker::GetInstance()->DecrementMemoryUsage(*this);
  UnmapViewOfFile(memory_);
  memory_ = nullptr;
  mapped_id_ = UnguessableToken();
  return true;
}

SharedMemoryHandle SharedMemory::GetReadOnlyHandle() const {
  HANDLE result;
  ProcessHandle process = GetCurrentProcess();
  if (!::DuplicateHandle(process, shm_.GetHandle(), process, &result,
                         FILE_MAP_READ | SECTION_QUERY, FALSE, 0)) {
    return SharedMemoryHandle();
  }
  SharedMemoryHandle handle =
      SharedMemoryHandle(result, shm_.GetSize(), shm_.GetGUID());
  handle.SetOwnershipPassesToIPC(true);
  return handle;
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

}  // namespace base
