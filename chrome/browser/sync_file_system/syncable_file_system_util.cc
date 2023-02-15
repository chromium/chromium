// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/sync_file_system/syncable_file_system_util.h"

#include <vector>

#include "base/command_line.h"
#include "base/location.h"
#include "base/strings/string_util.h"
#include "base/task/single_thread_task_runner.h"
#include "storage/browser/file_system/external_mount_points.h"
#include "storage/browser/file_system/file_observers.h"
#include "storage/browser/file_system/file_system_context.h"
#include "storage/common/file_system/file_system_util.h"
#include "third_party/blink/public/common/storage_key/storage_key.h"
#include "url/gurl.h"
#include "url/origin.h"

using storage::ExternalMountPoints;
using storage::FileSystemContext;
using storage::FileSystemURL;

namespace sync_file_system {

namespace {

const char kSyncableMountName[] = "syncfs";
const char kSyncableMountNameForInternalSync[] = "syncfs-internal";

const base::FilePath::CharType kSyncFileSystemDir[] =
    FILE_PATH_LITERAL("Sync FileSystem");

}  // namespace

void RegisterSyncableFileSystem() {
  ExternalMountPoints::GetSystemInstance()->RegisterFileSystem(
      kSyncableMountName, storage::kFileSystemTypeSyncable,
      storage::FileSystemMountOption(), base::FilePath());
  ExternalMountPoints::GetSystemInstance()->RegisterFileSystem(
      kSyncableMountNameForInternalSync,
      storage::kFileSystemTypeSyncableForInternalSync,
      storage::FileSystemMountOption(), base::FilePath());
}

void RevokeSyncableFileSystem() {
  ExternalMountPoints::GetSystemInstance()->RevokeFileSystem(
      kSyncableMountName);
  ExternalMountPoints::GetSystemInstance()->RevokeFileSystem(
      kSyncableMountNameForInternalSync);
}

GURL GetSyncableFileSystemRootURI(const GURL& origin) {
  return GURL(
      storage::GetExternalFileSystemRootURIString(origin, kSyncableMountName));
}

FileSystemURL CreateSyncableFileSystemURL(const GURL& origin,
                                          const base::FilePath& path) {
  base::FilePath path_for_url = path;
  if (storage::VirtualPath::IsAbsolute(path.value()))
    path_for_url = base::FilePath(path.value().substr(1));

  return ExternalMountPoints::GetSystemInstance()->CreateExternalFileSystemURL(
      blink::StorageKey::CreateFirstParty(url::Origin::Create(origin)),
      kSyncableMountName, path_for_url);
}

FileSystemURL CreateSyncableFileSystemURLForSync(
    storage::FileSystemContext* file_system_context,
    const FileSystemURL& syncable_url) {
  return ExternalMountPoints::GetSystemInstance()->CreateExternalFileSystemURL(
      syncable_url.storage_key(), kSyncableMountNameForInternalSync,
      syncable_url.path());
}

bool SerializeSyncableFileSystemURL(const FileSystemURL& url,
                                    std::string* serialized_url) {
  if (!url.is_valid() || url.type() != storage::kFileSystemTypeSyncable)
    return false;
  *serialized_url = GetSyncableFileSystemRootURI(url.origin().GetURL()).spec() +
                    url.path().AsUTF8Unsafe();
  return true;
}

bool DeserializeSyncableFileSystemURL(const std::string& serialized_url,
                                      FileSystemURL* url) {
#if !defined(FILE_PATH_USES_WIN_SEPARATORS)
  DCHECK(serialized_url.find('\\') == std::string::npos);
#endif  // FILE_PATH_USES_WIN_SEPARATORS

  const GURL gurl(serialized_url);
  FileSystemURL deserialized =
      ExternalMountPoints::GetSystemInstance()->CrackURL(
          gurl, blink::StorageKey::CreateFirstParty(url::Origin::Create(gurl)));
  if (!deserialized.is_valid() ||
      deserialized.type() != storage::kFileSystemTypeSyncable) {
    return false;
  }

  *url = deserialized;
  return true;
}

base::FilePath GetSyncFileSystemDir(const base::FilePath& profile_base_dir) {
  return profile_base_dir.Append(kSyncFileSystemDir);
}

void RunSoon(const base::Location& from_here, base::OnceClosure callback) {
  base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
      from_here, std::move(callback));
}

}  // namespace sync_file_system
