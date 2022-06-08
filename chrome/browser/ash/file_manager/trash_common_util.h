// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_FILE_MANAGER_TRASH_COMMON_UTIL_H_
#define CHROME_BROWSER_ASH_FILE_MANAGER_TRASH_COMMON_UTIL_H_

#include "base/files/file_path.h"

namespace file_manager {
namespace io_task {

// Constant representing the Trash folder name.
extern const char kTrashFolderName[];

// Constant representing the "info" folder name inside .Trash.
extern const char kInfoFolderName[];

// Constant representing the "files" folder name inside .Trash.
extern const char kFilesFolderName[];

// Helper to create a destination path for a file in one of the .Trash folders.
const base::FilePath GenerateTrashPath(const base::FilePath& trash_path,
                                       const std::string& subdir,
                                       const std::string& file_name);

}  // namespace io_task
}  // namespace file_manager

#endif  // CHROME_BROWSER_ASH_FILE_MANAGER_TRASH_COMMON_UTIL_H_
