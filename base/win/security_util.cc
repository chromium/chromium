// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/win/security_util.h"

#include <windows.h>
#include <winternl.h>

#include "base/check.h"
#include "base/files/file_path.h"
#include "base/logging.h"
#include "base/threading/scoped_blocking_call.h"
#include "base/win/access_control_list.h"
#include "base/win/scoped_handle.h"
#include "base/win/security_descriptor.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

namespace base {
namespace win {

namespace {

bool AddACEToPath(const FilePath& path,
                  const std::vector<Sid>& sids,
                  DWORD access_mask,
                  DWORD inheritance,
                  bool recursive,
                  SecurityAccessMode access_mode) {
  DCHECK(!path.empty());
  if (sids.empty()) {
    return true;
  }
  base::ScopedBlockingCall scoped_blocking_call(FROM_HERE,
                                                base::BlockingType::MAY_BLOCK);

  absl::optional<SecurityDescriptor> sd =
      SecurityDescriptor::FromFile(path, DACL_SECURITY_INFORMATION);
  if (!sd) {
    return false;
  }

  std::vector<ExplicitAccessEntry> entries;
  for (const Sid& sid : sids) {
    entries.emplace_back(sid, access_mode, access_mask, inheritance);
  }

  if (!sd->SetDaclEntries(entries)) {
    return false;
  }

  if (recursive) {
    return sd->WriteToFile(path, DACL_SECURITY_INFORMATION);
  }

  ScopedHandle handle(::CreateFile(path.value().c_str(), WRITE_DAC, 0, nullptr,
                                   OPEN_EXISTING, FILE_FLAG_BACKUP_SEMANTICS,
                                   nullptr));
  if (!handle.is_valid()) {
    DPLOG(ERROR) << "Failed opening path \"" << path.value()
                 << "\" to write DACL";
    return false;
  }
  return sd->WriteToHandle(handle.get(), SecurityObjectType::kKernel,
                           DACL_SECURITY_INFORMATION);
}

}  // namespace

bool GrantAccessToPath(const FilePath& path,
                       const std::vector<Sid>& sids,
                       DWORD access_mask,
                       DWORD inheritance,
                       bool recursive) {
  return AddACEToPath(path, sids, access_mask, inheritance, recursive,
                      SecurityAccessMode::kGrant);
}

bool DenyAccessToPath(const FilePath& path,
                      const std::vector<Sid>& sids,
                      DWORD access_mask,
                      DWORD inheritance,
                      bool recursive) {
  return AddACEToPath(path, sids, access_mask, inheritance, recursive,
                      SecurityAccessMode::kDeny);
}

std::vector<Sid> CloneSidVector(const std::vector<Sid>& sids) {
  std::vector<Sid> clone;
  clone.reserve(sids.size());
  for (const Sid& sid : sids) {
    clone.push_back(sid.Clone());
  }
  return clone;
}

void AppendSidVector(std::vector<Sid>& base_sids,
                     const std::vector<Sid>& append_sids) {
  for (const Sid& sid : append_sids) {
    base_sids.push_back(sid.Clone());
  }
}

absl::optional<ACCESS_MASK> GetGrantedAccess(HANDLE handle) {
  PUBLIC_OBJECT_BASIC_INFORMATION basic_info = {};
  if (!NT_SUCCESS(::NtQueryObject(handle, ObjectBasicInformation, &basic_info,
                                  sizeof(basic_info), nullptr))) {
    return absl::nullopt;
  }
  return basic_info.GrantedAccess;
}

}  // namespace win
}  // namespace base
