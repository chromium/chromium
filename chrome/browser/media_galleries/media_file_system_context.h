// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_MEDIA_GALLERIES_MEDIA_FILE_SYSTEM_CONTEXT_H_
#define CHROME_BROWSER_MEDIA_GALLERIES_MEDIA_FILE_SYSTEM_CONTEXT_H_

// Manages isolated filesystem namespaces for media file systems.

#include <string>

namespace base {
class FilePath;
}

class MediaFileSystemContext {
 public:
  virtual ~MediaFileSystemContext() {}

  // Register a new media file system for |path| as |fs_name|.
  virtual bool RegisterFileSystem(const std::string& device_id,
                                  const std::string& fs_name,
                                  const base::FilePath& path) = 0;

  // Revoke the passed |fs_name|.
  virtual void RevokeFileSystem(const std::string& fs_name) = 0;

  // Return the mount point root for the given |fs_name|. Returns an empty path
  // if |fs_name| is not valid.
  virtual base::FilePath GetRegisteredPath(
      const std::string& fs_name) const = 0;
};

#endif  // CHROME_BROWSER_MEDIA_GALLERIES_MEDIA_FILE_SYSTEM_CONTEXT_H_
