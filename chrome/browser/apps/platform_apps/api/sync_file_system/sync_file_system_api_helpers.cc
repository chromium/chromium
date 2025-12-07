// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/apps/platform_apps/api/sync_file_system/sync_file_system_api_helpers.h"

#include "base/notreached.h"
#include "storage/common/file_system/file_system_util.h"

namespace chrome_apps {
namespace api {

::sync_file_system::ConflictResolutionPolicy
ExtensionEnumToConflictResolutionPolicy(
    sync_file_system::ConflictResolutionPolicy policy) {
  switch (policy) {
    case sync_file_system::ConflictResolutionPolicy::kNone:
      return ::sync_file_system::CONFLICT_RESOLUTION_POLICY_UNKNOWN;
    case sync_file_system::ConflictResolutionPolicy::kLastWriteWin:
      return ::sync_file_system::CONFLICT_RESOLUTION_POLICY_LAST_WRITE_WIN;
    case sync_file_system::ConflictResolutionPolicy::kManual:
      return ::sync_file_system::CONFLICT_RESOLUTION_POLICY_MANUAL;
  }
  NOTREACHED() << "Invalid conflict resolution policy: " << ToString(policy);
}

}  // namespace api
}  // namespace chrome_apps
