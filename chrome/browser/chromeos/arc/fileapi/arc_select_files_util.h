// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_ARC_FILEAPI_ARC_SELECT_FILES_UTIL_H_
#define CHROME_BROWSER_CHROMEOS_ARC_FILEAPI_ARC_SELECT_FILES_UTIL_H_

#include <string>

#include "base/files/file_path.h"

namespace arc {

// Returns true if the given Android package should be excluded from the
// list of Android picker apps to be shown in Files app.
bool IsPickerPackageToExclude(const std::string& picker_package);

// Constructs a fake file path that represents the given Android activity.
// Returns an empty file path if invalid package_name or activity_name is given.
// Used by the file manager to pass back the selected Android activity through
// SelectFileDialog::Listener::FileSelected method.
base::FilePath ConvertAndroidActivityToFilePath(
    const std::string& package_name,
    const std::string& activity_name);

// Extracts an Android activity (${package_name}/${activity_name}) from a file
// path constructed by |ConvertAndroidActivityToFilePath|.
// Returns an empty string if the file path is invalid.
std::string ConvertFilePathToAndroidActivity(const base::FilePath& file_path);

}  // namespace arc

#endif  // CHROME_BROWSER_CHROMEOS_ARC_FILEAPI_ARC_SELECT_FILES_UTIL_H_
