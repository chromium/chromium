// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_ARC_FILE_SYSTEM_WATCHER_ARC_FILE_SYSTEM_WATCHER_UTIL_H_
#define CHROME_BROWSER_ASH_ARC_FILE_SYSTEM_WATCHER_ARC_FILE_SYSTEM_WATCHER_UTIL_H_

#include "base/files/file_path.h"

namespace arc {

// The removable media path in ChromeOS. This is the actual directory to be
// watched.
constexpr base::FilePath::CharType kCrosRemovableMediaDir[] =
    FILE_PATH_LITERAL("/media/removable");

// The prefix for device label used in Android paths for removable media.
// A removable device mounted at /media/removable/UNTITLED is mounted at
// /storage/removable_UNTITLED in Android.
constexpr char kRemovableMediaLabelPrefix[] = "removable_";

// Appends |cros_path|'s relative path from "/media/removable" to |android_path|
// with the altered device label which is used in Android removable media paths.
bool AppendRelativePathForRemovableMedia(const base::FilePath& cros_path,
                                         base::FilePath* android_path);

// Returns the android file path for |cros_path| by replacing |cros_dir|
// prefix with |android_dir|.
// If |cros_path| is a removable media path, the prefix "removable_" is appended
// to the device name.
//
// If the function fails, i.e. AppendRelativePathForRemovableMedia returns
// false, it returns an empty FilePath.
base::FilePath GetAndroidPath(const base::FilePath& cros_path,
                              const base::FilePath& cros_dir,
                              const base::FilePath& android_dir);

// Returns true if the file path has a media extension supported by Android.
bool HasAndroidSupportedMediaExtension(const base::FilePath& path);

}  // namespace arc

#endif  // CHROME_BROWSER_ASH_ARC_FILE_SYSTEM_WATCHER_ARC_FILE_SYSTEM_WATCHER_UTIL_H_
