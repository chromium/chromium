// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/apps/almanac_api_client/proto_file_manager.h"

#include <string>

#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/logging.h"
#include "base/system/sys_info.h"

namespace apps {
bool WriteFileToDisk(const base::FilePath& path, const std::string& data) {
  if (!base::PathExists(path)) {
    if (!base::CreateDirectory(path.DirName())) {
      LOG(ERROR) << "Directory cannot be created: " << path.value();
    }
  }

  base::FilePath temp_file;
  if (!base::CreateTemporaryFileInDir(path.DirName(), &temp_file)) {
    LOG(ERROR) << "Failed to create a temporary file: " << path.value();
    return false;
  }

  if (!base::WriteFile(temp_file, data)) {
    LOG(ERROR) << "Failed to write to temporary file: " << path.value();
    base::DeleteFile(temp_file);
    return false;
  }

  if (!base::ReplaceFile(temp_file, path, /*error=*/nullptr)) {
    LOG(ERROR) << "Failed to replace temporary file: " << path.value();
    base::DeleteFile(temp_file);
    return false;
  }

  return true;
}
}  // namespace apps
