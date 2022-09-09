// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/arc/fileapi/arc_select_files_util.h"

#include "base/logging.h"
#include "base/strings/string_util.h"
#include "third_party/re2/src/re2/re2.h"

namespace arc {

namespace {

// These packages should be excluded from the list of picker apps to be shown in
// Files app since equivalent contents are already exposed in Files app.
constexpr const char* kPackagesToExclude[] = {
    // Default Android file picker.
    "com.android.documentsui",
    // Google Drive picker.
    "com.google.android.apps.docs",
};

constexpr char kAndroidActivityFilePathRoot[] = "/special/android-activity";

// Lightweight (incomprehensive) sanity check for Android package name and
// activity name.
constexpr char kActivityNameSanityCheckRegex[] = "[a-zA-Z0-9_.]+[a-zA-Z0-9_]";

}  // namespace

bool IsPickerPackageToExclude(const std::string& picker_package) {
  for (const char* package : kPackagesToExclude) {
    if (base::EqualsCaseInsensitiveASCII(picker_package, package))
      return true;
  }
  return false;
}

base::FilePath ConvertAndroidActivityToFilePath(
    const std::string& package_name,
    const std::string& activity_name) {
  if (!RE2::FullMatch(package_name, kActivityNameSanityCheckRegex)) {
    LOG(ERROR) << "Invalid package name: " << package_name;
    return base::FilePath();
  }
  if (!RE2::FullMatch(activity_name, kActivityNameSanityCheckRegex)) {
    LOG(ERROR) << "Invalid activity name: " << activity_name;
    return base::FilePath();
  }
  return base::FilePath(kAndroidActivityFilePathRoot)
      .Append(package_name)
      .Append(activity_name);
}

std::string ConvertFilePathToAndroidActivity(const base::FilePath& file_path) {
  base::FilePath result_path;
  if (base::FilePath(kAndroidActivityFilePathRoot)
          .AppendRelativePath(file_path, &result_path)) {
    return result_path.value();
  }
  return "";
}

}  // namespace arc
