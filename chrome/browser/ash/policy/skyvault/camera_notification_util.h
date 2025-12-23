// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_POLICY_SKYVAULT_CAMERA_NOTIFICATION_UTIL_H_
#define CHROME_BROWSER_ASH_POLICY_SKYVAULT_CAMERA_NOTIFICATION_UTIL_H_

namespace base {
class FilePath;
}

namespace policy::skyvault_ui_utils {

struct TitleAndMessageIds {
  int title;
  int message;
};

// Takes the file name and returns title and message string IDs for camera
// OneDrive sign-in notification. File name is expected to have a supported
// extension.
TitleAndMessageIds GetCameraSignInStringsFromFilename(
    const base::FilePath& file);

}  // namespace policy::skyvault_ui_utils

#endif  // CHROME_BROWSER_ASH_POLICY_SKYVAULT_CAMERA_NOTIFICATION_UTIL_H_
