// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/process/process_info.h"

#include <windows.h>

#include <optional>

#include "base/logging.h"
#include "base/notreached.h"
#include "base/win/access_token.h"

namespace base {

IntegrityLevel GetCurrentProcessIntegrityLevel() {
  std::optional<base::win::AccessToken> token =
      base::win::AccessToken::FromCurrentProcess();
  if (!token) {
    PLOG(ERROR) << "AccessToken::FromCurrentProcess() failed";
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

  NOTREACHED_IN_MIGRATION();
  return INTEGRITY_UNKNOWN;
}

bool IsCurrentProcessElevated() {
  std::optional<base::win::AccessToken> token =
      base::win::AccessToken::FromCurrentProcess();
  if (!token) {
    PLOG(ERROR) << "AccessToken::FromCurrentProcess() failed";
    return false;
  }
  return token->IsElevated();
}

bool IsCurrentProcessInAppContainer() {
  std::optional<base::win::AccessToken> token =
      base::win::AccessToken::FromCurrentProcess();
  if (!token) {
    PLOG(ERROR) << "AccessToken::FromCurrentProcess() failed";
    return false;
  }
  return token->IsAppContainer();
}

}  // namespace base
