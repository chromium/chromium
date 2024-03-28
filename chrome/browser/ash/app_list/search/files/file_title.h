// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_APP_LIST_SEARCH_FILES_FILE_TITLE_H_
#define CHROME_BROWSER_ASH_APP_LIST_SEARCH_FILES_FILE_TITLE_H_

#include <string>

#include "base/files/file_path.h"

namespace app_list {

// Returns a string representing the title of `file_path`, suitable for display.
std::u16string GetFileTitle(const base::FilePath& file_path);

}  // namespace app_list

#endif  // CHROME_BROWSER_ASH_APP_LIST_SEARCH_FILES_FILE_TITLE_H_
