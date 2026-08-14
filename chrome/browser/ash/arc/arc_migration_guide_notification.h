// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_ARC_ARC_MIGRATION_GUIDE_NOTIFICATION_H_
#define CHROME_BROWSER_ASH_ARC_ARC_MIGRATION_GUIDE_NOTIFICATION_H_

namespace user_manager {
class User;
}  // namespace user_manager

namespace arc {

// The notification ID used for the file system migration guide notification.
inline constexpr char kSuggestNotificationId[] = "arc_fs_migration/suggest";

// Shows a notification for guiding the user for file system migration.
// This is used when an ARC app is launched while ARC is temporarily disabled
// due to the file system incompatibility.
void ShowArcMigrationGuideNotification(const user_manager::User& user);

}  // namespace arc

#endif  // CHROME_BROWSER_ASH_ARC_ARC_MIGRATION_GUIDE_NOTIFICATION_H_
