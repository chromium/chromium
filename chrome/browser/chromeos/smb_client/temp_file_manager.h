// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_SMB_CLIENT_TEMP_FILE_MANAGER_H_
#define CHROME_BROWSER_CHROMEOS_SMB_CLIENT_TEMP_FILE_MANAGER_H_

#include <vector>

#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/files/scoped_temp_dir.h"

namespace chromeos {
namespace smb_client {

// Helper class to handle construction and deletion of a temporary directory
// containing temporary files.
class TempFileManager {
 public:
  TempFileManager();
  TempFileManager(const TempFileManager&) = delete;
  TempFileManager& operator=(const TempFileManager&) = delete;
  ~TempFileManager();

  // Returns the path of the temporary directory.
  const base::FilePath& GetTempDirectoryPath() const;

  // Creates a temporary file in temp_dir_ path and writes in |data|. Before
  // returning, this seeks to the beginning of the file in preparation for
  // reading. This also calls unlink() on the created file. Returns a file
  // descriptor which will be invalid on failure.
  base::ScopedFD CreateTempFile(const std::vector<uint8_t>& data);

 private:
  // Creates a temporary file in temp_dir_ path. This also calls unlink() on the
  // created file. Returns a file descriptor which will be invalid on failure.
  base::ScopedFD CreateTempFile();

  // Scoped class that handles deletion of the temporary directory.
  base::ScopedTempDir temp_dir_;
};

}  // namespace smb_client
}  // namespace chromeos

#endif  // CHROME_BROWSER_CHROMEOS_SMB_CLIENT_TEMP_FILE_MANAGER_H_
