// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/crosapi/one_drive_notification_service_ash.h"

#include <string>

#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/ui/webui/ash/cloud_upload/automated_mount_error_notification.h"

namespace crosapi {

OneDriveNotificationServiceAsh::OneDriveNotificationServiceAsh() = default;
OneDriveNotificationServiceAsh::~OneDriveNotificationServiceAsh() = default;

void OneDriveNotificationServiceAsh::BindReceiver(
    mojo::PendingReceiver<mojom::OneDriveNotificationService> receiver) {
  one_drive_notification_service_receiver_set_.Add(this, std::move(receiver));
}

void OneDriveNotificationServiceAsh::ShowAutomatedMountError() {
  ash::cloud_upload::ShowAutomatedMountErrorNotification(
      *ProfileManager::GetPrimaryUserProfile());
}

}  // namespace crosapi
