// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/extensions/file_manager/system_notification_manager.h"

#include <memory>

namespace file_manager {

SystemNotificationManager::SystemNotificationManager() {}

SystemNotificationManager::~SystemNotificationManager() = default;

bool SystemNotificationManager::DoFilesSwaWindowsExist() {
  return false;
}

void SystemNotificationManager::HandleDeviceEvent(
    file_manager_private::DeviceEvent event) {}

}  // namespace file_manager
