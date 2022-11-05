// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/extensions/file_manager/scoped_suppress_drive_notifications_for_path.h"

#include "chrome/browser/ash/extensions/file_manager/event_router_factory.h"

namespace file_manager {

ScopedSuppressDriveNotificationsForPath::
    ScopedSuppressDriveNotificationsForPath(Profile* profile,
                                            base::FilePath relative_drive_path)
    : profile_(profile), relative_drive_path_(relative_drive_path) {
  file_manager::EventRouter* event_router =
      file_manager::EventRouterFactory::GetForProfile(profile_);
  if (event_router) {
    event_router->SuppressDriveNotificationsForFilePath(relative_drive_path_);
  }
}

ScopedSuppressDriveNotificationsForPath::
    ~ScopedSuppressDriveNotificationsForPath() {
  file_manager::EventRouter* event_router =
      file_manager::EventRouterFactory::GetForProfile(profile_);
  if (event_router) {
    event_router->RestoreDriveNotificationsForFilePath(relative_drive_path_);
  }
}

}  // namespace file_manager
