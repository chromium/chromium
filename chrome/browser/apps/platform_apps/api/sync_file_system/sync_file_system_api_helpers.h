// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_APPS_PLATFORM_APPS_API_SYNC_FILE_SYSTEM_SYNC_FILE_SYSTEM_API_HELPERS_H_
#define CHROME_BROWSER_APPS_PLATFORM_APPS_API_SYNC_FILE_SYSTEM_SYNC_FILE_SYSTEM_API_HELPERS_H_

#include <memory>

#include "chrome/browser/sync_file_system/conflict_resolution_policy.h"
#include "chrome/browser/sync_file_system/sync_action.h"
#include "chrome/browser/sync_file_system/sync_direction.h"
#include "chrome/browser/sync_file_system/sync_file_status.h"
#include "chrome/browser/sync_file_system/sync_file_type.h"
#include "chrome/browser/sync_file_system/sync_service_state.h"
#include "chrome/common/apps/platform_apps/api/sync_file_system.h"

namespace storage {
class FileSystemURL;
}

namespace base {
class DictionaryValue;
}

namespace chrome_apps {
namespace api {

// ::chrome_apps::api::sync_file_system <-> ::sync_file_system enum conversion
// functions.
sync_file_system::ServiceStatus SyncServiceStateToExtensionEnum(
    ::sync_file_system::SyncServiceState state);

sync_file_system::FileStatus SyncFileStatusToExtensionEnum(
    ::sync_file_system::SyncFileStatus status);

sync_file_system::SyncAction SyncActionToExtensionEnum(
    ::sync_file_system::SyncAction action);

sync_file_system::SyncDirection SyncDirectionToExtensionEnum(
    ::sync_file_system::SyncDirection direction);

sync_file_system::ConflictResolutionPolicy
ConflictResolutionPolicyToExtensionEnum(
    ::sync_file_system::ConflictResolutionPolicy policy);

::sync_file_system::ConflictResolutionPolicy
    ExtensionEnumToConflictResolutionPolicy(
        sync_file_system::ConflictResolutionPolicy);

// Creates a dictionary for FileSystem Entry from given |url|.
// This will create a dictionary which has 'fileSystemType', 'fileSystemName',
// 'rootUrl', 'filePath' and 'isDirectory' fields.
// The returned dictionary is supposed to be interpreted
// in the renderer's customer binding to create a FileEntry object.
// This returns NULL if the given |url| is not valid or |file_type| is
// SYNC_FILE_TYPE_UNKNOWN.
std::unique_ptr<base::DictionaryValue> CreateDictionaryValueForFileSystemEntry(
    const storage::FileSystemURL& url,
    ::sync_file_system::SyncFileType file_type);

}  // namespace api
}  // namespace chrome_apps

#endif  // CHROME_BROWSER_APPS_PLATFORM_APPS_API_SYNC_FILE_SYSTEM_SYNC_FILE_SYSTEM_API_HELPERS_H_
