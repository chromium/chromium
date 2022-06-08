// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/file_manager/trash_common_util.h"

namespace file_manager {
namespace io_task {

constexpr char kTrashFolderName[] = ".Trash";
constexpr char kInfoFolderName[] = "info";
constexpr char kFilesFolderName[] = "files";

const base::FilePath GenerateTrashPath(const base::FilePath& trash_path,
                                       const std::string& subdir,
                                       const std::string& file_name) {
  base::FilePath path = trash_path.Append(subdir).Append(file_name);
  // The metadata file in .Trash/info always has the .trashinfo extension.
  if (subdir == kInfoFolderName) {
    path = path.AddExtension(".trashinfo");
  }
  return path;
}

}  // namespace io_task
}  // namespace file_manager
