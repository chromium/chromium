// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_CROSAPI_ONE_DRIVE_NOTIFICATION_SERVICE_ASH_H_
#define CHROME_BROWSER_ASH_CROSAPI_ONE_DRIVE_NOTIFICATION_SERVICE_ASH_H_

#include "chromeos/crosapi/mojom/one_drive_notification_service.mojom.h"
#include "mojo/public/cpp/bindings/receiver_set.h"

namespace crosapi {

// Implements the UI elements for the OneDrive integration.
class OneDriveNotificationServiceAsh
    : public mojom::OneDriveNotificationService {
 public:
  OneDriveNotificationServiceAsh();
  ~OneDriveNotificationServiceAsh() override;

  void BindReceiver(
      mojo::PendingReceiver<mojom::OneDriveNotificationService> receiver);

  // mojom::OneDriveNotificationService:
  void ShowAutomatedMountError() override;

 private:
  mojo::ReceiverSet<mojom::OneDriveNotificationService>
      one_drive_notification_service_receiver_set_;
};

}  // namespace crosapi

#endif  // CHROME_BROWSER_ASH_CROSAPI_ONE_DRIVE_NOTIFICATION_SERVICE_ASH_H_
