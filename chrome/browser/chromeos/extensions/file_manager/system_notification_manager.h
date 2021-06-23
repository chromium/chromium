// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_EXTENSIONS_FILE_MANAGER_SYSTEM_NOTIFICATION_MANAGER_H_
#define CHROME_BROWSER_CHROMEOS_EXTENSIONS_FILE_MANAGER_SYSTEM_NOTIFICATION_MANAGER_H_

namespace file_manager {

// Manages creation/deletion and update of system notifications on behalf
// of the File Manager application.
class SystemNotificationManager {
 public:
  SystemNotificationManager();
  ~SystemNotificationManager();
};

}  // namespace file_manager

#endif  // CHROME_BROWSER_CHROMEOS_EXTENSIONS_FILE_MANAGER_SYSTEM_NOTIFICATION_MANAGER_H_
