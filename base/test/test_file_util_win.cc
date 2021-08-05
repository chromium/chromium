// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/test/test_file_util.h"

#include <aclapi.h>
#include <stddef.h>
#include <wchar.h>
#include <windows.h>

#include <memory>

#include "base/check_op.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/memory/ptr_util.h"
#include "base/notreached.h"
#include "base/strings/string_split.h"
#include "base/strings/string_util.h"
#include "base/threading/platform_thread.h"
#include "base/win/scoped_handle.h"
#include "base/win/shlwapi.h"

namespace base {

namespace {

struct PermissionInfo {
  PSECURITY_DESCRIPTOR security_descriptor;
  ACL dacl;
};

// Gets a blob indicating the permission information for |path|.
// |length| is the length of the blob.  Zero on failure.
// Returns the blob pointer, or NULL on failure.
void* GetPermissionInfo(const FilePath& path, size_t* length) {
  DCHECK(length);
  *length = 0;
  PACL dacl = nullptr;
  PSECURITY_DESCRIPTOR security_descriptor;
  if (GetNamedSecurityInfo(path.value().c_str(), SE_FILE_OBJECT,
                           DACL_SECURITY_INFORMATION, nullptr, nullptr, &dacl,
                           nullptr, &security_descriptor) != ERROR_SUCCESS) {
    return nullptr;
  }
  DCHECK(dacl);

  *length = sizeof(PSECURITY_DESCRIPTOR) + dacl->AclSize;
  PermissionInfo* info = reinterpret_cast<PermissionInfo*>(new char[*length]);
  info->security_descriptor = security_descriptor;
  memcpy(&info->dacl, dacl, dacl->AclSize);

  return info;
}

// Restores the permission information for |path|, given the blob retrieved
// using |GetPermissionInfo()|.
// |info| is the pointer to the blob.
// |length| is the length of the blob.
// Either |info| or |length| may be NULL/0, in which case nothing happens.
bool RestorePermissionInfo(const FilePath& path, void* info, size_t length) {
  if (!info || !length)
    return false;

  PermissionInfo* perm = reinterpret_cast<PermissionInfo*>(info);

  DWORD rc = SetNamedSecurityInfo(const_cast<wchar_t*>(path.value().c_str()),
                                  SE_FILE_OBJECT, DACL_SECURITY_INFORMATION,
                                  nullptr, nullptr, &perm->dacl, nullptr);
  LocalFree(perm->security_descriptor);

  char* char_array = reinterpret_cast<char*>(info);
  delete [] char_array;

  return rc == ERROR_SUCCESS;
}

std::unique_ptr<wchar_t[]> ToCStr(const std::basic_string<wchar_t>& str) {
  size_t size = str.size() + 1;
  std::unique_ptr<wchar_t[]> ptr = std::make_unique<wchar_t[]>(size);
  wcsncpy(ptr.get(), str.c_str(), size);
  return ptr;
}

}  // namespace

bool DieFileDie(const FilePath& file, bool recurse) {
  // It turns out that to not induce flakiness a long timeout is needed.
  const int kIterations = 25;
  const TimeDelta kTimeout = TimeDelta::FromSeconds(10) / kIterations;

  if (!PathExists(file))
    return true;

  // Sometimes Delete fails, so try a few more times. Divide the timeout
  // into short chunks, so that if a try succeeds, we won't delay the test
  // for too long.
  for (int i = 0; i < kIterations; ++i) {
    bool success;
    if (recurse)
      success = DeletePathRecursively(file);
    else
      success = DeleteFile(file);
    if (success)
      return true;
    PlatformThread::Sleep(kTimeout);
  }
  return false;
}

void SyncPageCacheToDisk() {
  // Approximating this with noop. The proper implementation would require
  // administrator privilege:
  // https://docs.microsoft.com/en-us/windows/desktop/api/FileAPI/nf-fileapi-flushfilebuffers
}

bool EvictFileFromSystemCache(const FilePath& file) {
  win::ScopedHandle file_handle(
      CreateFile(file.value().c_str(), GENERIC_READ | GENERIC_WRITE, 0, nullptr,
                 OPEN_EXISTING, FILE_FLAG_NO_BUFFERING, nullptr));
  if (!file_handle.IsValid())
    return false;

  // Re-write the file time information to trigger cache eviction for the file.
  // This function previously overwrote the entire file without buffering, but
  // local experimentation validates this simplified and *much* faster approach:
  // [1] Sysinternals RamMap no longer lists these files as cached afterwards.
  // [2] Telemetry performance test startup.cold.blank_page reports sane values.
  BY_HANDLE_FILE_INFORMATION bhi = {0};
  CHECK(::GetFileInformationByHandle(file_handle.Get(), &bhi));
  CHECK(::SetFileTime(file_handle.Get(), &bhi.ftCreationTime,
                      &bhi.ftLastAccessTime, &bhi.ftLastWriteTime));
  return true;
}

// Deny |permission| on the file |path|, for the current user.
bool DenyFilePermission(const FilePath& path, DWORD permission) {
  PACL old_dacl;
  PSECURITY_DESCRIPTOR security_descriptor;

  std::unique_ptr<TCHAR[]> path_ptr = ToCStr(path.value().c_str());
  if (GetNamedSecurityInfo(path_ptr.get(), SE_FILE_OBJECT,
                           DACL_SECURITY_INFORMATION, nullptr, nullptr,
                           &old_dacl, nullptr,
                           &security_descriptor) != ERROR_SUCCESS) {
    return false;
  }

  std::unique_ptr<TCHAR[]> current_user = ToCStr(std::wstring(L"CURRENT_USER"));
  EXPLICIT_ACCESS new_access = {
      permission,
      DENY_ACCESS,
      0,
      {nullptr, NO_MULTIPLE_TRUSTEE, TRUSTEE_IS_NAME, TRUSTEE_IS_USER,
       current_user.get()}};

  PACL new_dacl;
  if (SetEntriesInAcl(1, &new_access, old_dacl, &new_dacl) != ERROR_SUCCESS) {
    LocalFree(security_descriptor);
    return false;
  }

  DWORD rc = SetNamedSecurityInfo(path_ptr.get(), SE_FILE_OBJECT,
                                  DACL_SECURITY_INFORMATION, nullptr, nullptr,
                                  new_dacl, nullptr);
  LocalFree(security_descriptor);
  LocalFree(new_dacl);

  return rc == ERROR_SUCCESS;
}

bool MakeFileUnreadable(const FilePath& path) {
  return DenyFilePermission(path, GENERIC_READ);
}

bool MakeFileUnwritable(const FilePath& path) {
  return DenyFilePermission(path, GENERIC_WRITE);
}

FilePermissionRestorer::FilePermissionRestorer(const FilePath& path)
    : path_(path), info_(nullptr), length_(0) {
  info_ = GetPermissionInfo(path_, &length_);
  DCHECK(info_);
  DCHECK_NE(0u, length_);
}

FilePermissionRestorer::~FilePermissionRestorer() {
  if (!RestorePermissionInfo(path_, info_, length_))
    NOTREACHED();
}

}  // namespace base
