// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_POLICY_SKYVAULT_SIGNIN_NOTIFICATION_HELPER_H_
#define CHROME_BROWSER_ASH_POLICY_SKYVAULT_SIGNIN_NOTIFICATION_HELPER_H_

#include "base/files/file.h"
#include "base/functional/callback_forward.h"
#include "chrome/browser/ash/policy/skyvault/odfs_skyvault_uploader.h"

class Profile;

namespace policy::skyvault_ui_utils {

inline constexpr char kDownloadSignInNotificationPrefix[] =
    "skyvault-download-signin-";

inline constexpr char kMigrationSignInNotification[] = "skyvault-migration";
inline constexpr char kScreenCaptureSignInNotificationIdPrefix[] =
    "skyvault-screencapture-signin-";

// The notification button index.
enum NotificationButtonIndex {
  kSignInButton = 0,
  kCancelButton = 1,
};

// Shows a notification to the user to sign in to OneDrive.
void ShowSignInNotification(
    Profile* profile,
    int64_t id,
    local_user_files::UploadTrigger trigger,
    const base::FilePath& file_path,
    base::OnceCallback<void(base::File::Error)> signin_callback,
    std::optional<const gfx::Image> thumbnail = std::nullopt);

}  // namespace policy::skyvault_ui_utils

#endif  // CHROME_BROWSER_ASH_POLICY_SKYVAULT_SIGNIN_NOTIFICATION_HELPER_H_
