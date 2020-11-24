// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_SCANNING_SCANNING_UTIL_H_
#define CHROME_BROWSER_CHROMEOS_SCANNING_SCANNING_UTIL_H_

namespace base {
class FilePath;
}  // namespace base

namespace content {
class WebUI;
}  // namespace content

class Profile;

namespace chromeos {
namespace scanning {

// Opens the Files app with |path_to_file| highlighted if |path_to_file| is
// found in the filesystem. Users are only allowed to save and open scans under
// the |drive_path| and |my_files_path| paths.
bool ShowFileInFilesApp(const base::FilePath& drive_path,
                        const base::FilePath& my_files_path,
                        content::WebUI* web_ui,
                        const base::FilePath& path_to_file);

// Returns the Google Drive mount path.
base::FilePath GetDrivePath(Profile* profile);

}  // namespace scanning
}  // namespace chromeos

#endif  // CHROME_BROWSER_CHROMEOS_SCANNING_SCANNING_UTIL_H_
