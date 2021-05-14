// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/smb_client/temp_file_manager.h"

#include <stdlib.h>
#include <unistd.h>

#include <memory>
#include <string>
#include <vector>

#include "base/files/file_util.h"
#include "base/logging.h"

namespace chromeos {
namespace smb_client {

TempFileManager::TempFileManager() {
  CHECK(temp_dir_.CreateUniqueTempDir());
}

TempFileManager::~TempFileManager() = default;

const base::FilePath& TempFileManager::GetTempDirectoryPath() const {
  return temp_dir_.GetPath();
}

base::ScopedFD TempFileManager::CreateTempFile(
    const std::vector<uint8_t>& data) {
  base::ScopedFD temp_fd = CreateTempFile();

  // Write the data into the newly created file.
  if (!base::WriteFileDescriptor(temp_fd.get(), data)) {
    LOG(ERROR) << "Error writing to temporary file";
    temp_fd.reset();
    return temp_fd;
  }

  // Seek the file descriptor to the start of the file in preparation for
  // reading.
  if (lseek(temp_fd.get(), 0, SEEK_SET) < 0) {
    LOG(ERROR) << "Error seeking to beginning of temporary file";
    temp_fd.reset();
    return temp_fd;
  }

  return temp_fd;
}

base::ScopedFD TempFileManager::CreateTempFile() {
  const std::string str = temp_dir_.GetPath().Append("XXXXXX").value();
  // Make sure that the string we are passing is null-terminated.
  std::unique_ptr<char[]> file_path = std::make_unique<char[]>(str.size() + 1);
  memcpy(file_path.get(), str.c_str(), str.size());
  file_path[str.size()] = '\0';

  // mkstemp() modifies the passed in array.
  base::ScopedFD temp_fd(HANDLE_EINTR(mkstemp(file_path.get())));
  if (!temp_fd.is_valid()) {
    LOG(ERROR) << "mkstemp failed to create a temporary file";
    temp_fd.reset();
    return temp_fd;
  }

  // Unlink causes the file to be deleted when the file descriptor is closed.
  if (unlink(file_path.get()) != 0) {
    LOG(ERROR) << "Failed to unlink temporary file: " << file_path.get();
    temp_fd.reset();
    return temp_fd;
  }

  return temp_fd;
}

}  // namespace smb_client
}  // namespace chromeos
