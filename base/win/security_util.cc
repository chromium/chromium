// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/win/security_util.h"

#include <aclapi.h>
#include <windows.h>

#include <string>

#include "base/check.h"
#include "base/files/file_path.h"
#include "base/logging.h"
#include "base/optional.h"
#include "base/win/scoped_localalloc.h"
#include "base/win/win_util.h"

namespace base {
namespace win {

bool GrantAccessToPath(const FilePath& path,
                       const std::vector<Sid>& sids,
                       DWORD access_mask,
                       DWORD inheritance) {
  DCHECK(!path.empty());
  if (sids.empty())
    return true;

  std::wstring object_name = path.value();
  PSECURITY_DESCRIPTOR sd = nullptr;
  PACL dacl = nullptr;

  // Get the existing DACL.
  DWORD error = ::GetNamedSecurityInfo(object_name.c_str(), SE_FILE_OBJECT,
                                       DACL_SECURITY_INFORMATION, nullptr,
                                       nullptr, &dacl, nullptr, &sd);
  if (error != ERROR_SUCCESS) {
    ::SetLastError(error);
    DPLOG(ERROR) << "Failed getting DACL for path \"" << path.value() << "\"";
    return false;
  }
  auto sd_ptr = TakeLocalAlloc(sd);
  std::vector<EXPLICIT_ACCESS> access_entries(sids.size());
  auto entries_interator = access_entries.begin();
  for (const Sid& sid : sids) {
    EXPLICIT_ACCESS& new_access = *entries_interator++;
    new_access.grfAccessMode = GRANT_ACCESS;
    new_access.grfAccessPermissions = access_mask;
    new_access.grfInheritance = inheritance;
    ::BuildTrusteeWithSid(&new_access.Trustee, sid.GetPSID());
  }

  PACL new_dacl = nullptr;
  error = ::SetEntriesInAcl(access_entries.size(), access_entries.data(), dacl,
                            &new_dacl);
  if (error != ERROR_SUCCESS) {
    ::SetLastError(error);
    DPLOG(ERROR) << "Failed adding ACEs to DACL for path \"" << path.value()
                 << "\"";
    return false;
  }
  auto new_dacl_ptr = TakeLocalAlloc(new_dacl);

  error = ::SetNamedSecurityInfo(&object_name[0], SE_FILE_OBJECT,
                                 DACL_SECURITY_INFORMATION, nullptr, nullptr,
                                 new_dacl_ptr.get(), nullptr);
  if (error != ERROR_SUCCESS) {
    ::SetLastError(error);
    DPLOG(ERROR) << "Failed setting DACL for path \"" << path.value() << "\"";
    return false;
  }

  return true;
}

}  // namespace win
}  // namespace base
