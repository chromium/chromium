// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_FILEAPI_FILE_ACCESS_PERMISSIONS_H_
#define CHROME_BROWSER_ASH_FILEAPI_FILE_ACCESS_PERMISSIONS_H_

#include <set>

#include "base/containers/flat_map.h"
#include "base/files/file_path.h"
#include "base/synchronization/lock.h"
#include "url/origin.h"

namespace ash {

// In a thread safe manner maintains the set of paths allowed to access for
// each extension.
class FileAccessPermissions {
 public:
  FileAccessPermissions();

  FileAccessPermissions(const FileAccessPermissions&) = delete;
  FileAccessPermissions& operator=(const FileAccessPermissions&) = delete;

  virtual ~FileAccessPermissions();

  // Grants |origin| access to |path|.
  void GrantAccessPermission(const url::Origin& origin,
                             const base::FilePath& path);
  // Checks whether |origin| has permission to access to |path|.
  bool HasAccessPermission(const url::Origin& origin,
                           const base::FilePath& path) const;
  // Revokes all file permissions for |origin|.
  void RevokePermissions(const url::Origin& origin);

 private:
  typedef std::set<base::FilePath> PathSet;
  typedef base::flat_map<url::Origin, PathSet> PathAccessMap;

  mutable base::Lock lock_;  // Synchronize all access to path_map_.
  PathAccessMap path_map_;
};

}  // namespace ash

#endif  // CHROME_BROWSER_ASH_FILEAPI_FILE_ACCESS_PERMISSIONS_H_
