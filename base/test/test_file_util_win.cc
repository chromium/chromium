// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/test/test_file_util.h"

#include <windows.h>

#include <memory>

#include "base/check.h"
#include "base/containers/span.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/memory/ptr_util.h"
#include "base/strings/cstring_view.h"
#include "base/threading/platform_thread.h"
#include "base/win/access_token.h"
#include "base/win/scoped_handle.h"
#include "base/win/security_descriptor.h"

namespace base {

bool DieFileDie(const FilePath& file, bool recurse) {
  // It turns out that to not induce flakiness a long timeout is needed.
  const int kIterations = 25;
  const TimeDelta kTimeout = Seconds(10) / kIterations;

  if (!PathExists(file)) {
    return true;
  }

  // Sometimes Delete fails, so try a few more times. Divide the timeout
  // into short chunks, so that if a try succeeds, we won't delay the test
  // for too long.
  for (int i = 0; i < kIterations; ++i) {
    bool success;
    if (recurse) {
      success = DeletePathRecursively(file);
    } else {
      success = DeleteFile(file);
    }
    if (success) {
      return true;
    }
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
  FilePath::StringType file_value = file.value();
  if (file_value.length() >= MAX_PATH && file.IsAbsolute()) {
    file_value.insert(0, L"\\\\?\\");
  }
  win::ScopedHandle file_handle(
      ::CreateFile(file_value.c_str(), GENERIC_READ | GENERIC_WRITE, 0, nullptr,
                   OPEN_EXISTING, FILE_FLAG_NO_BUFFERING, nullptr));
  if (!file_handle.is_valid()) {
    return false;
  }

  // Re-write the file time information to trigger cache eviction for the file.
  // This function previously overwrote the entire file without buffering, but
  // local experimentation validates this simplified and *much* faster approach:
  // [1] Sysinternals RamMap no longer lists these files as cached afterwards.
  // [2] Telemetry performance test startup.cold.blank_page reports sane values.
  BY_HANDLE_FILE_INFORMATION bhi = {0};
  CHECK(::GetFileInformationByHandle(file_handle.get(), &bhi));
  CHECK(::SetFileTime(file_handle.get(), &bhi.ftCreationTime,
                      &bhi.ftLastAccessTime, &bhi.ftLastWriteTime));
  return true;
}

// Deny `permission` on the file `path`, for the current user.
bool DenyFilePermission(const FilePath& path, DWORD permission) {
  auto sd = win::SecurityDescriptor::FromFile(path, DACL_SECURITY_INFORMATION);
  auto token = win::AccessToken::FromCurrentProcess();
  if (!sd || !token) {
    return false;
  }
  if (!sd->SetDaclEntry(token->User(), win::SecurityAccessMode::kDeny,
                        permission, /*inheritance=*/0)) {
    return false;
  }
  return sd->WriteToFile(path, DACL_SECURITY_INFORMATION);
}

bool MakeFileUnreadable(const FilePath& path) {
  return DenyFilePermission(path, GENERIC_READ);
}

bool MakeFileUnwritable(const FilePath& path) {
  return DenyFilePermission(path, GENERIC_WRITE);
}

struct FilePermissionRestorer::SavedFilePermissions {
  explicit SavedFilePermissions(win::SecurityDescriptor sd)
      : sd_(std::move(sd)) {}
  win::SecurityDescriptor sd_;
};

FilePermissionRestorer::FilePermissionRestorer(const FilePath& path)
    : path_(path) {
  auto sd = win::SecurityDescriptor::FromFile(path, DACL_SECURITY_INFORMATION);
  CHECK(sd);
  permissions_ = std::make_unique<SavedFilePermissions>(std::move(*sd));
}

FilePermissionRestorer::~FilePermissionRestorer() {
  CHECK(permissions_);
  CHECK(permissions_->sd_.WriteToFile(path_, DACL_SECURITY_INFORMATION));
}

std::wstring GetFileDacl(const FilePath& path) {
  auto sd = win::SecurityDescriptor::FromFile(path, DACL_SECURITY_INFORMATION);
  if (!sd) {
    return {};
  }
  auto sddl = sd->ToSddl(DACL_SECURITY_INFORMATION);
  if (!sddl) {
    return {};
  }
  return *sddl;
}

bool CreateWithDacl(const FilePath& path, wcstring_view sddl, bool directory) {
  auto sd = win::SecurityDescriptor::FromSddl(sddl);
  if (!sd) {
    return false;
  }
  SECURITY_DESCRIPTOR sd_abs = sd->ToAbsolute();
  SECURITY_ATTRIBUTES security_attr = {};
  security_attr.nLength = sizeof(security_attr);
  security_attr.lpSecurityDescriptor = &sd_abs;
  if (directory) {
    return !!::CreateDirectory(path.value().c_str(), &security_attr);
  }

  return win::ScopedHandle(::CreateFile(path.value().c_str(), GENERIC_ALL, 0,
                                        &security_attr, CREATE_ALWAYS, 0,
                                        nullptr))
      .is_valid();
}

}  // namespace base
