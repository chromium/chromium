// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_EXTENSIONS_FILE_MANAGER_SCOPED_SUPPRESS_DRIVE_NOTIFICATIONS_FOR_PATH_H_
#define CHROME_BROWSER_ASH_EXTENSIONS_FILE_MANAGER_SCOPED_SUPPRESS_DRIVE_NOTIFICATIONS_FOR_PATH_H_

#include "base/files/file_path.h"
#include "base/memory/raw_ptr.h"
#include "chrome/browser/ash/extensions/file_manager/event_router.h"

class Profile;

namespace file_manager {

// A helper class to prevent Drive notifications for a file identified by its
// relative Drive path. Notifications are restored once the class instance is
// destroyed.
class ScopedSuppressDriveNotificationsForPath {
 public:
  ScopedSuppressDriveNotificationsForPath() = delete;
  ScopedSuppressDriveNotificationsForPath(Profile* profile,
                                          base::FilePath relative_drive_path);
  ScopedSuppressDriveNotificationsForPath(
      const ScopedSuppressDriveNotificationsForPath&) = delete;
  ScopedSuppressDriveNotificationsForPath& operator=(
      const ScopedSuppressDriveNotificationsForPath&) = delete;
  ~ScopedSuppressDriveNotificationsForPath();

 private:
  raw_ptr<Profile> profile_;
  base::FilePath relative_drive_path_;
};

}  // namespace file_manager

#endif  // CHROME_BROWSER_ASH_EXTENSIONS_FILE_MANAGER_SCOPED_SUPPRESS_DRIVE_NOTIFICATIONS_FOR_PATH_H_
