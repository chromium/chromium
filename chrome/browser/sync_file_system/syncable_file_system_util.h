// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SYNC_FILE_SYSTEM_SYNCABLE_FILE_SYSTEM_UTIL_H_
#define CHROME_BROWSER_SYNC_FILE_SYSTEM_SYNCABLE_FILE_SYSTEM_UTIL_H_

#include <string>

#include "base/files/file_path.h"
#include "base/functional/callback_forward.h"
#include "storage/browser/file_system/file_system_url.h"

namespace storage {
class FileSystemContext;
class FileSystemURL;
}

namespace base {
class Location;
}

namespace sync_file_system {

// Registers a syncable filesystem.
void RegisterSyncableFileSystem();

// Revokes the syncable filesystem.
void RevokeSyncableFileSystem();

// Returns the root URI of the syncable filesystem for |origin|.
GURL GetSyncableFileSystemRootURI(const GURL& origin);

// Creates a FileSystem URL for the |path| in a syncable filesystem for
// |origin|.
//
// Example: Assume following arguments are given:
//   origin: 'http://www.example.com/',
//   path: '/foo/bar',
// returns 'filesystem:http://www.example.com/external/syncfs/foo/bar'
storage::FileSystemURL CreateSyncableFileSystemURL(const GURL& origin,
                                                   const base::FilePath& path);

// Creates a special filesystem URL for synchronizing |syncable_url|.
storage::FileSystemURL CreateSyncableFileSystemURLForSync(
    storage::FileSystemContext* file_system_context,
    const storage::FileSystemURL& syncable_url);

// Serializes a given FileSystemURL |url| and sets the serialized string to
// |serialized_url|. If the URL does not represent a syncable filesystem,
// |serialized_url| is not filled in, and returns false. Separators of the
// path will be normalized depending on its platform.
//
// Example: Assume a following FileSystemURL object is given:
//   origin() returns 'http://www.example.com/',
//   type() returns the kFileSystemTypeSyncable,
//   filesystem_id() returns 'syncfs',
//   path() returns '/foo/bar',
// this URL will be serialized to
// (on Windows)
//   'filesystem:http://www.example.com/external/syncfs/foo\\bar'
// (on others)
//   'filesystem:http://www.example.com/external/syncfs/foo/bar'
bool SerializeSyncableFileSystemURL(const storage::FileSystemURL& url,
                                    std::string* serialized_url);

// Deserializes a serialized FileSystem URL string |serialized_url| and sets the
// deserialized value to |url|. If the reconstructed object is invalid or does
// not represent a syncable filesystem, returns false.
//
// NOTE: On any platform other than Windows, this function assumes that
// |serialized_url| does not contain '\\'. If it contains '\\' on such
// platforms, '\\' may be replaced with '/' (It would not be an expected
// behavior).
//
// See the comment of SerializeSyncableFileSystemURL() for more details.
bool DeserializeSyncableFileSystemURL(const std::string& serialized_url,
                                      storage::FileSystemURL* url);

// Returns SyncFileSystem sub-directory path.
base::FilePath GetSyncFileSystemDir(const base::FilePath& profile_base_dir);

// Posts |callback| to the current thread.
void RunSoon(const base::Location& from_here, base::OnceClosure callback);

}  // namespace sync_file_system

#endif  // CHROME_BROWSER_SYNC_FILE_SYSTEM_SYNCABLE_FILE_SYSTEM_UTIL_H_
