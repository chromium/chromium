// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// This file provides URL-related utilities.

#ifndef CHROME_BROWSER_ASH_FILE_MANAGER_URL_UTIL_H_
#define CHROME_BROWSER_ASH_FILE_MANAGER_URL_UTIL_H_

#include <string>
#include <vector>

#include "ui/shell_dialogs/select_file_dialog.h"
#include "url/gurl.h"

namespace file_manager {
namespace util {

// Returns the file manager's main page URL.
GURL GetFileManagerMainPageUrl();

// Returns the file manager's main page URL with parameters encoded as JSON
// in the query string section. |file_types| is optional.
GURL GetFileManagerMainPageUrlWithParams(
    ui::SelectFileDialog::Type type,
    const std::u16string& title,
    const GURL& current_directory_url,
    const GURL& selection_url,
    const std::string& target_name,
    const ui::SelectFileDialog::FileTypeInfo* file_types,
    int file_type_index,
    const std::string& search_query,
    bool show_android_picker_apps,
    std::vector<std::string> volume_filter);

}  // namespace util
}  // namespace file_manager

#endif  // CHROME_BROWSER_ASH_FILE_MANAGER_URL_UTIL_H_
