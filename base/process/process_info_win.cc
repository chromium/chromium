// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/process/process_info.h"

#include <windows.h>

#include <optional>

#include "base/logging.h"
#include "base/notreached.h"
#include "base/process/process.h"
#include "base/win/access_token.h"

namespace base {

namespace {

IntegrityLevel GetProcessIntegrityLevelInternal(
    std::optional<win::AccessToken> token) {
  if (!token) {
    PLOG(ERROR) << "AccessToken `token` is invalid";
    return INTEGRITY_UNKNOWN;
  }
  DWORD integrity_level = token->IntegrityLevel();

  if (integrity_level < SECURITY_MANDATORY_LOW_RID)
    return UNTRUSTED_INTEGRITY;

  if (integrity_level < SECURITY_MANDATORY_MEDIUM_RID)
    return LOW_INTEGRITY;

  if (integrity_level < SECURITY_MANDATORY_HIGH_RID)
    return MEDIUM_INTEGRITY;

  if (integrity_level >= SECURITY_MANDATORY_HIGH_RID)
    return HIGH_INTEGRITY;

  NOTREACHED();
}

}  // namespace

IntegrityLevel GetProcessIntegrityLevel(ProcessId process_id) {
  auto process = Process::OpenWithAccess(process_id, PROCESS_QUERY_INFORMATION);
  return process.IsValid()
             ? GetProcessIntegrityLevelInternal(win::AccessToken::FromProcess(
                   process.Handle(),
                   /*impersonation=*/false, TOKEN_QUERY_SOURCE))
             : INTEGRITY_UNKNOWN;
}

IntegrityLevel GetCurrentProcessIntegrityLevel() {
  return GetProcessIntegrityLevelInternal(
      win::AccessToken::FromCurrentProcess());
}

bool IsCurrentProcessElevated() {
  std::optional<win::AccessToken> token =
      win::AccessToken::FromCurrentProcess();
  if (!token) {
    PLOG(ERROR) << "AccessToken::FromCurrentProcess() failed";
    return false;
  }
  return token->IsElevated();
}

bool IsCurrentProcessInAppContainer() {
  std::optional<win::AccessToken> token =
      win::AccessToken::FromCurrentProcess();
  if (!token) {
    PLOG(ERROR) << "AccessToken::FromCurrentProcess() failed";
    return false;
  }
  return token->IsAppContainer();
}

}  // namespace base
