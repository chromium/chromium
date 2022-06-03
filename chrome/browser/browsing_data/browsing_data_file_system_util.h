// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_BROWSING_DATA_BROWSING_DATA_FILE_SYSTEM_UTIL_H_
#define CHROME_BROWSER_BROWSING_DATA_BROWSING_DATA_FILE_SYSTEM_UTIL_H_

#include <vector>

#include "storage/common/file_system/file_system_types.h"

namespace browsing_data_file_system_util {

// Returns chrome-specific file system types to use when constructing a
// browsing_data::FileSystemHelper.
std::vector<storage::FileSystemType> GetAdditionalFileSystemTypes();

}  // namespace browsing_data_file_system_util

#endif  // CHROME_BROWSER_BROWSING_DATA_BROWSING_DATA_FILE_SYSTEM_UTIL_H_
