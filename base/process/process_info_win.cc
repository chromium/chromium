// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/process/process_info.h"

#include <windows.h>
#include <memory>

#include "base/logging.h"
#include "base/memory/ptr_util.h"
#include "base/notreached.h"
#include "base/time/time.h"
#include "base/win/scoped_handle.h"

namespace base {

namespace {

base::win::ScopedHandle CreateCurrentProcessToken() {
  // OpenProcessToken can fail due to permission or allocation failures
  // for the kernel object. Return nullptr in those cases.
  HANDLE process_token = nullptr;
  OpenProcessToken(GetCurrentProcess(), TOKEN_QUERY, &process_token);
  return base::win::ScopedHandle(process_token);
}

}  // namespace

IntegrityLevel GetCurrentProcessIntegrityLevel() {
  base::win::ScopedHandle scoped_process_token(CreateCurrentProcessToken());

  DWORD token_info_length = 0;
  if (::GetTokenInformation(scoped_process_token.Get(), TokenIntegrityLevel,
                            nullptr, 0, &token_info_length) ||
      ::GetLastError() != ERROR_INSUFFICIENT_BUFFER) {
    return INTEGRITY_UNKNOWN;
  }

  auto token_label_bytes = std::make_unique<char[]>(token_info_length);
  TOKEN_MANDATORY_LABEL* token_label =
      reinterpret_cast<TOKEN_MANDATORY_LABEL*>(token_label_bytes.get());
  if (!::GetTokenInformation(scoped_process_token.Get(), TokenIntegrityLevel,
                             token_label, token_info_length,
                             &token_info_length)) {
    return INTEGRITY_UNKNOWN;
  }
  DWORD integrity_level = *::GetSidSubAuthority(
      token_label->Label.Sid,
      static_cast<DWORD>(*::GetSidSubAuthorityCount(token_label->Label.Sid) -
                         1));

  if (integrity_level < SECURITY_MANDATORY_LOW_RID)
    return UNTRUSTED_INTEGRITY;

  if (integrity_level < SECURITY_MANDATORY_MEDIUM_RID)
    return LOW_INTEGRITY;

  if (integrity_level >= SECURITY_MANDATORY_MEDIUM_RID &&
      integrity_level < SECURITY_MANDATORY_HIGH_RID) {
    return MEDIUM_INTEGRITY;
  }

  if (integrity_level >= SECURITY_MANDATORY_HIGH_RID)
    return HIGH_INTEGRITY;

  NOTREACHED();
  return INTEGRITY_UNKNOWN;
}

bool IsCurrentProcessElevated() {
  base::win::ScopedHandle scoped_process_token(CreateCurrentProcessToken());

  // Unlike TOKEN_ELEVATION_TYPE which returns TokenElevationTypeDefault when
  // UAC is turned off, TOKEN_ELEVATION returns whether the process is elevated.
  DWORD size;
  TOKEN_ELEVATION elevation;
  if (!GetTokenInformation(scoped_process_token.Get(), TokenElevation,
                           &elevation, sizeof(elevation), &size)) {
    PLOG(ERROR) << "GetTokenInformation() failed";
    return false;
  }
  return !!elevation.TokenIsElevated;
}

bool IsCurrentProcessInAppContainer() {
  base::win::ScopedHandle scoped_process_token(CreateCurrentProcessToken());

  DWORD size;
  DWORD appcontainer;
  if (!GetTokenInformation(scoped_process_token.Get(), ::TokenIsAppContainer,
                           &appcontainer, sizeof(appcontainer), &size)) {
    PLOG(ERROR) << "GetTokenInformation() failed";
    return false;
  }
  return !!appcontainer;
}

}  // namespace base
