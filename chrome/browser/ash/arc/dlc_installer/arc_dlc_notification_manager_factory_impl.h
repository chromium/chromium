// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_ARC_DLC_INSTALLER_ARC_DLC_NOTIFICATION_MANAGER_FACTORY_IMPL_H_
#define CHROME_BROWSER_ASH_ARC_DLC_INSTALLER_ARC_DLC_NOTIFICATION_MANAGER_FACTORY_IMPL_H_

#include <memory>

#include "chromeos/ash/experiences/arc/dlc_installer/arc_dlc_notification_manager_factory.h"

class AccountId;

namespace arc {

class ArcDlcInstallNotificationManager;

// Implementation of the ArcDlcInstallProfileDelegate interface, used to create
// notification managers based on a Profile associated with an AccountId.
class ArcDlcNotificationManagerFactoryImpl
    : public ArcDlcNotificationManagerFactory {
 public:
  ArcDlcNotificationManagerFactoryImpl();

  ArcDlcNotificationManagerFactoryImpl(
      const ArcDlcNotificationManagerFactoryImpl&) = delete;
  ArcDlcNotificationManagerFactoryImpl& operator=(
      const ArcDlcNotificationManagerFactoryImpl&) = delete;

  ~ArcDlcNotificationManagerFactoryImpl() override;

  // Creates and returns a notification manager for the given AccountId.
  std::unique_ptr<ArcDlcInstallNotificationManager> CreateNotificationManager(
      const AccountId& account_id) override;
};

}  // namespace arc

#endif  // CHROME_BROWSER_ASH_ARC_DLC_INSTALLER_ARC_DLC_NOTIFICATION_MANAGER_FACTORY_IMPL_H_
