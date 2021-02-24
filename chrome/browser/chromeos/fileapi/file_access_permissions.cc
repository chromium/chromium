// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/fileapi/file_access_permissions.h"

#include "base/check.h"

namespace chromeos {

FileAccessPermissions::FileAccessPermissions() {}

FileAccessPermissions::~FileAccessPermissions() {}

void FileAccessPermissions::GrantAccessPermission(const std::string& origin,
                                                  const base::FilePath& path) {
  DCHECK(!path.empty());
  base::AutoLock locker(lock_);
  path_map_[origin].insert(path);
}

bool FileAccessPermissions::HasAccessPermission(
    const std::string& origin,
    const base::FilePath& path) const {
  base::AutoLock locker(lock_);
  PathAccessMap::const_iterator path_map_iter = path_map_.find(origin);
  if (path_map_iter == path_map_.end())
    return false;
  const PathSet& path_set = path_map_iter->second;

  // Check this file and walk up its directory tree to find if this extension
  // has access to it.
  base::FilePath current_path = path.StripTrailingSeparators();
  base::FilePath last_path;
  while (current_path != last_path) {
    if (path_set.find(current_path) != path_set.end())
      return true;
    last_path = current_path;
    current_path = current_path.DirName();
  }
  return false;
}

void FileAccessPermissions::RevokePermissions(
    const std::string& extension_id) {
  base::AutoLock locker(lock_);
  path_map_.erase(extension_id);
}

}  // namespace chromeos
