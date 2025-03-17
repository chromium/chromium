// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/arc/dlc_installer/arc_dlc_notification_manager_factory_impl.h"

#include "base/logging.h"
#include "chrome/browser/ash/arc/dlc_installer/arc_dlc_install_notification_delegate_impl.h"
#include "chrome/browser/profiles/profile.h"
#include "chromeos/ash/components/browser_context_helper/browser_context_helper.h"
#include "chromeos/ash/experiences/arc/dlc_installer/arc_dlc_install_notification_manager.h"
#include "components/account_id/account_id.h"

namespace arc {

ArcDlcNotificationManagerFactoryImpl::ArcDlcNotificationManagerFactoryImpl() =
    default;

ArcDlcNotificationManagerFactoryImpl::~ArcDlcNotificationManagerFactoryImpl() =
    default;

std::unique_ptr<ArcDlcInstallNotificationManager>
ArcDlcNotificationManagerFactoryImpl::CreateNotificationManager(
    const AccountId& account_id) {
  Profile* profile = Profile::FromBrowserContext(
      ash::BrowserContextHelper::Get()->GetBrowserContextByAccountId(
          account_id));

  if (!profile) {
    LOG(ERROR) << "Profile is null for account ID: " << account_id;
    return nullptr;
  }
  return std::make_unique<ArcDlcInstallNotificationManager>(
      std::make_unique<ArcDlcInstallNotificationManagerDelegateImpl>(profile),
      account_id);
}

}  // namespace arc
