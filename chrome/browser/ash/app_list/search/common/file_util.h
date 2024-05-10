// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_APP_LIST_SEARCH_COMMON_FILE_UTIL_H_
#define CHROME_BROWSER_ASH_APP_LIST_SEARCH_COMMON_FILE_UTIL_H_

#include <vector>

#include "base/files/file_path.h"

class Profile;

namespace app_list {

// Gets the list of paths for trashing as we need to exclude them from launcher
// search.
std::vector<base::FilePath> GetTrashPaths(Profile* profile);

}  // namespace app_list

#endif  // CHROME_BROWSER_ASH_APP_LIST_SEARCH_COMMON_FILE_UTIL_H_
