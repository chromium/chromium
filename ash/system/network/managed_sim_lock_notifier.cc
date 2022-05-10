// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/network/managed_sim_lock_notifier.h"

#include "ash/public/cpp/network_config_service.h"
#include "ash/public/cpp/notification_utils.h"
#include "ash/public/cpp/system_tray_client.h"
#include "ash/shell.h"
#include "ash/strings/grit/ash_strings.h"
#include "ash/system/model/system_tray_model.h"
#include "ash/system/tray/tray_popup_utils.h"
#include "base/bind.h"
#include "components/onc/onc_constants.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/message_center/message_center.h"
#include "ui/message_center/public/cpp/message_center_constants.h"

namespace ash {
namespace {

const char kNotifierManagedSimLock[] = "ash.managed-simlock";

}  // namespace

// static
const char ManagedSimLockNotifier::kManagedSimLockNotificationId[] =
    "cros_managed_sim_lock_notifier_ids.pin_unlock_device";

ManagedSimLockNotifier::ManagedSimLockNotifier() {
  GetNetworkConfigService(
      remote_cros_network_config_.BindNewPipeAndPassReceiver());
  remote_cros_network_config_->AddObserver(
      cros_network_config_observer_receiver_.BindNewPipeAndPassRemote());
}

ManagedSimLockNotifier::~ManagedSimLockNotifier() {}

void ManagedSimLockNotifier::OnPoliciesApplied(const std::string& userhash) {
  CheckGlobalNetworkConfiguarion();
}

void ManagedSimLockNotifier::CheckGlobalNetworkConfiguarion() {
  remote_cros_network_config_->GetGlobalPolicy(
      base::BindOnce(&ManagedSimLockNotifier::OnGetGlobalPolicy,
                     weak_ptr_factory_.GetWeakPtr()));
}

void ManagedSimLockNotifier::OnGetGlobalPolicy(
    chromeos::network_config::mojom::GlobalPolicyPtr global_policy) {
  if (!global_policy->allow_cellular_sim_lock) {
    MaybeShowNotification();
    return;
  }

  RemoveNotification();
}

void ManagedSimLockNotifier::MaybeShowNotification() {
  remote_cros_network_config_->GetNetworkStateList(
      chromeos::network_config::mojom::NetworkFilter::New(
          chromeos::network_config::mojom::FilterType::kAll,
          chromeos::network_config::mojom::NetworkType::kCellular,
          chromeos::network_config::mojom::kNoLimit),
      base::BindOnce(&ManagedSimLockNotifier::OnCellularNetworksList,
                     weak_ptr_factory_.GetWeakPtr()));
}

void ManagedSimLockNotifier::OnCellularNetworksList(
    std::vector<chromeos::network_config::mojom::NetworkStatePropertiesPtr>
        networks) {
  // Check if there are any PIN locked pSIM or eSIM networks.
  for (auto& network : networks) {
    if (network->type_state->get_cellular()->sim_locked) {
      ShowNotification();
      return;
    }
  }

  RemoveNotification();
}

void ManagedSimLockNotifier::ShowNotification() {
  scoped_refptr<message_center::NotificationDelegate> delegate =
      base::MakeRefCounted<message_center::HandleNotificationClickDelegate>(
          base::BindRepeating([](absl::optional<int> button_index) {
            // When clicked, open the SIM Unlock dialog in Cellular settings if
            // we can open WebUI settings, otherwise do nothing.
            if (TrayPopupUtils::CanOpenWebUISettings()) {
              Shell::Get()
                  ->system_tray_model()
                  ->client()
                  ->ShowSettingsSimUnlock();
            } else {
              LOG(WARNING) << "Cannot open Cellular settings since it's not "
                              "possible to open OS Settings";
            }
          }));

  std::unique_ptr<message_center::Notification> notification =
      ash::CreateSystemNotification(
          message_center::NOTIFICATION_TYPE_SIMPLE,
          kManagedSimLockNotificationId,
          l10n_util::GetStringUTF16(
              IDS_ASH_NETWORK_MANAGED_SIM_LOCK_NOTIFICATION_TITLE),
          l10n_util::GetStringUTF16(
              IDS_ASH_NETWORK_MANAGED_SIM_LOCK_NOTIFICATION_MESSAGE),
          /*display_source=*/std::u16string(), GURL(),
          message_center::NotifierId(
              message_center::NotifierType::SYSTEM_COMPONENT,
              kNotifierManagedSimLock),
          message_center::RichNotificationData(), std::move(delegate),
          /*small_image=*/gfx::VectorIcon(),
          message_center::SystemNotificationWarningLevel::WARNING);

  message_center::MessageCenter* message_center =
      message_center::MessageCenter::Get();
  message_center->AddNotification(std::move(notification));
}

void ManagedSimLockNotifier::RemoveNotification() {
  message_center::MessageCenter* message_center =
      message_center::MessageCenter::Get();
  message_center->RemoveNotification(kManagedSimLockNotificationId, false);
}

}  // namespace ash
