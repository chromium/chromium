// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_FILE_SYSTEM_PROVIDER_MOUNT_PATH_UTIL_H_
#define CHROME_BROWSER_CHROMEOS_FILE_SYSTEM_PROVIDER_MOUNT_PATH_UTIL_H_

#include <string>

#include "base/files/file_path.h"
#include "base/macros.h"
#include "storage/browser/file_system/file_system_url.h"

class Profile;

namespace chromeos {
namespace file_system_provider {

class ProvidedFileSystemInterface;
class ProviderId;

namespace util {

// Constructs a safe mount point path for the provided file system.
base::FilePath GetMountPath(Profile* profile,
                            const ProviderId& provider_id,
                            const std::string& file_system_id);

// Checks whether a local path is handled by File System Provider API or not.
bool IsFileSystemProviderLocalPath(const base::FilePath& local_path);

// Finds a file system, which is responsible for handling the specified |url| by
// analysing the mount path. Also, extract the file path from the virtual path
// to be used by the file system operations.
class FileSystemURLParser {
 public:
  explicit FileSystemURLParser(const storage::FileSystemURL& url);
  virtual ~FileSystemURLParser();

  // Parses the |url| passed to the constructor. If parsing succeeds, then
  // returns true. Otherwise, false. Must be called on UI thread.
  bool Parse();

  ProvidedFileSystemInterface* file_system() const { return file_system_; }
  const base::FilePath& file_path() const { return file_path_; }

 private:
  storage::FileSystemURL url_;
  ProvidedFileSystemInterface* file_system_;
  base::FilePath file_path_;

  DISALLOW_COPY_AND_ASSIGN(FileSystemURLParser);
};

// Finds a file system, which is responsible for handling the specified
// |local_path| by analysing the mount point name. Alsoo, extract the file path
// from the local path to be used by the file system operations.
class LocalPathParser {
 public:
  LocalPathParser(Profile* profile, const base::FilePath& local_path);
  virtual ~LocalPathParser();

  // Parses the |local_path| passed to the constructor. If parsing succeeds,
  // then returns true. Otherwise, false. Must be called on UI thread.
  bool Parse();

  ProvidedFileSystemInterface* file_system() const { return file_system_; }
  const base::FilePath& file_path() const { return file_path_; }

 private:
  Profile* profile_;
  base::FilePath local_path_;
  ProvidedFileSystemInterface* file_system_;
  base::FilePath file_path_;

  DISALLOW_COPY_AND_ASSIGN(LocalPathParser);
};
}  // namespace util
}  // namespace file_system_provider
}  // namespace chromeos

#endif  // CHROME_BROWSER_CHROMEOS_FILE_SYSTEM_PROVIDER_MOUNT_PATH_UTIL_H_
