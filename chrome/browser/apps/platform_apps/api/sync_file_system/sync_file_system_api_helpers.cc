// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/apps/platform_apps/api/sync_file_system/sync_file_system_api_helpers.h"

#include "base/notreached.h"
#include "base/values.h"
#include "storage/browser/file_system/file_system_url.h"
#include "storage/common/file_system/file_system_util.h"

namespace chrome_apps {
namespace api {

sync_file_system::ServiceStatus SyncServiceStateToExtensionEnum(
    ::sync_file_system::SyncServiceState state) {
  switch (state) {
    case ::sync_file_system::SYNC_SERVICE_RUNNING:
      return sync_file_system::ServiceStatus::kRunning;
    case ::sync_file_system::SYNC_SERVICE_AUTHENTICATION_REQUIRED:
      return sync_file_system::ServiceStatus::kAuthenticationRequired;
    case ::sync_file_system::SYNC_SERVICE_TEMPORARY_UNAVAILABLE:
      return sync_file_system::ServiceStatus::kTemporaryUnavailable;
    case ::sync_file_system::SYNC_SERVICE_DISABLED:
      return sync_file_system::ServiceStatus::kDisabled;
  }
  NOTREACHED_IN_MIGRATION() << "Invalid state: " << state;
  return sync_file_system::ServiceStatus::kNone;
}

sync_file_system::FileStatus SyncFileStatusToExtensionEnum(
    ::sync_file_system::SyncFileStatus status) {
  switch (status) {
    case ::sync_file_system::SYNC_FILE_STATUS_SYNCED:
      return sync_file_system::FileStatus::kSynced;
    case ::sync_file_system::SYNC_FILE_STATUS_HAS_PENDING_CHANGES:
      return sync_file_system::FileStatus::kPending;
    case ::sync_file_system::SYNC_FILE_STATUS_CONFLICTING:
      return sync_file_system::FileStatus::kConflicting;
    case ::sync_file_system::SYNC_FILE_STATUS_UNKNOWN:
      return sync_file_system::FileStatus::kNone;
  }
  NOTREACHED_IN_MIGRATION() << "Invalid status: " << status;
  return sync_file_system::FileStatus::kNone;
}

sync_file_system::SyncAction SyncActionToExtensionEnum(
    ::sync_file_system::SyncAction action) {
  switch (action) {
    case ::sync_file_system::SYNC_ACTION_ADDED:
      return sync_file_system::SyncAction::kAdded;
    case ::sync_file_system::SYNC_ACTION_UPDATED:
      return sync_file_system::SyncAction::kUpdated;
    case ::sync_file_system::SYNC_ACTION_DELETED:
      return sync_file_system::SyncAction::kDeleted;
    case ::sync_file_system::SYNC_ACTION_NONE:
      return sync_file_system::SyncAction::kNone;
  }
  NOTREACHED_IN_MIGRATION() << "Invalid action: " << action;
  return sync_file_system::SyncAction::kNone;
}

sync_file_system::SyncDirection SyncDirectionToExtensionEnum(
    ::sync_file_system::SyncDirection direction) {
  switch (direction) {
    case ::sync_file_system::SYNC_DIRECTION_LOCAL_TO_REMOTE:
      return sync_file_system::SyncDirection::kLocalToRemote;
    case ::sync_file_system::SYNC_DIRECTION_REMOTE_TO_LOCAL:
      return sync_file_system::SyncDirection::kRemoteToLocal;
    case ::sync_file_system::SYNC_DIRECTION_NONE:
      return sync_file_system::SyncDirection::kNone;
  }
  NOTREACHED_IN_MIGRATION() << "Invalid direction: " << direction;
  return sync_file_system::SyncDirection::kNone;
}

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
  NOTREACHED_IN_MIGRATION()
      << "Invalid conflict resolution policy: " << ToString(policy);
  return ::sync_file_system::CONFLICT_RESOLUTION_POLICY_UNKNOWN;
}

sync_file_system::ConflictResolutionPolicy
ConflictResolutionPolicyToExtensionEnum(
    ::sync_file_system::ConflictResolutionPolicy policy) {
  switch (policy) {
    case ::sync_file_system::CONFLICT_RESOLUTION_POLICY_UNKNOWN:
      return sync_file_system::ConflictResolutionPolicy::kNone;
    case ::sync_file_system::CONFLICT_RESOLUTION_POLICY_LAST_WRITE_WIN:
      return sync_file_system::ConflictResolutionPolicy::kLastWriteWin;
    case ::sync_file_system::CONFLICT_RESOLUTION_POLICY_MANUAL:
      return sync_file_system::ConflictResolutionPolicy::kManual;
    case ::sync_file_system::CONFLICT_RESOLUTION_POLICY_MAX:
      NOTREACHED_IN_MIGRATION();
      return sync_file_system::ConflictResolutionPolicy::kNone;
  }
  NOTREACHED_IN_MIGRATION() << "Invalid conflict resolution policy: " << policy;
  return sync_file_system::ConflictResolutionPolicy::kNone;
}

std::optional<base::Value::Dict> CreateDictionaryValueForFileSystemEntry(
    const storage::FileSystemURL& url,
    ::sync_file_system::SyncFileType file_type) {
  if (!url.is_valid() ||
      file_type == ::sync_file_system::SYNC_FILE_TYPE_UNKNOWN)
    return std::nullopt;

  std::string file_path =
      base::FilePath(storage::VirtualPath::GetNormalizedFilePath(url.path()))
          .AsUTF8Unsafe();

  std::string root_url =
      storage::GetFileSystemRootURI(url.origin().GetURL(), url.mount_type())
          .spec();
  if (!url.filesystem_id().empty()) {
    root_url.append(url.filesystem_id());
    root_url.append("/");
  }

  base::Value::Dict dict;
  dict.Set("fileSystemType",
           storage::GetFileSystemTypeString(url.mount_type()));
  dict.Set("fileSystemName",
           storage::GetFileSystemName(url.origin().GetURL(), url.type()));
  dict.Set("rootUrl", root_url);
  dict.Set("filePath", file_path);
  dict.Set("isDirectory",
           (file_type == ::sync_file_system::SYNC_FILE_TYPE_DIRECTORY));

  return dict;
}

}  // namespace api
}  // namespace chrome_apps
