// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/network/managed_sim_lock_notifier.h"

#include "ash/ash_element_identifiers.h"
#include "ash/public/cpp/network_config_service.h"
#include "ash/public/cpp/notification_utils.h"
#include "ash/public/cpp/system_tray_client.h"
#include "ash/session/session_controller_impl.h"
#include "ash/shell.h"
#include "ash/strings/grit/ash_strings.h"
#include "ash/system/model/system_tray_model.h"
#include "ash/system/tray/tray_popup_utils.h"
#include "base/functional/bind.h"
#include "chromeos/ash/components/network/cellular_metrics_logger.h"
#include "components/onc/onc_constants.h"
#include "components/session_manager/session_manager_types.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/message_center/message_center.h"
#include "ui/message_center/public/cpp/message_center_constants.h"

namespace ash {
namespace {

const char kNotifierManagedSimLock[] = "ash.managed-simlock";

chromeos::network_config::mojom::DeviceStatePropertiesPtr
GetCellularDeviceIfExists(
    std::vector<chromeos::network_config::mojom::DeviceStatePropertiesPtr>&
        devices) {
  for (auto& device : devices) {
    if (device->type == chromeos::network_config::mojom::NetworkType::kCellular)
      return std::move(device);
  }
  return nullptr;
}

}  // namespace

// static
const char ManagedSimLockNotifier::kManagedSimLockNotificationId[] =
    "cros_managed_sim_lock_notifier_ids.pin_unlock_device";

ManagedSimLockNotifier::ManagedSimLockNotifier() {
  GetNetworkConfigService(
      remote_cros_network_config_.BindNewPipeAndPassReceiver());
  remote_cros_network_config_->AddObserver(
      cros_network_config_observer_receiver_.BindNewPipeAndPassRemote());
  Shell::Get()->session_controller()->AddObserver(this);
}

ManagedSimLockNotifier::~ManagedSimLockNotifier() {
  Shell::Get()->session_controller()->RemoveObserver(this);
}

void ManagedSimLockNotifier::OnSessionStateChanged(
    session_manager::SessionState state) {
  if (Shell::Get()->session_controller()->GetSessionState() ==
      session_manager::SessionState::ACTIVE) {
    CheckGlobalNetworkConfiguration();
  }
}

void ManagedSimLockNotifier::OnDeviceStateListChanged() {
  remote_cros_network_config_->GetDeviceStateList(
      base::BindOnce(&ManagedSimLockNotifier::OnGetDeviceStateList,
                     weak_ptr_factory_.GetWeakPtr()));
}

void ManagedSimLockNotifier::OnGetDeviceStateList(
    std::vector<chromeos::network_config::mojom::DeviceStatePropertiesPtr>
        devices) {
  chromeos::network_config::mojom::DeviceStatePropertiesPtr cellular_device =
      GetCellularDeviceIfExists(devices);

  // Remove Notification and reset |primary_iccid_| if no cellular device or
  // the cellular device is currently not enabled.
  if (!cellular_device || !cellular_device->sim_lock_status ||
      !cellular_device->sim_infos ||
      cellular_device->device_state !=
          chromeos::network_config::mojom::DeviceStateType::kEnabled) {
    primary_iccid_.clear();
    RemoveNotification();
    return;
  }

  // If the SIM Lock setting is disabled, remove notification.
  if (!cellular_device->sim_lock_status->lock_enabled) {
    RemoveNotification();
    return;
  }

  // If the primary SIM changes, check if the restrict SIM Lock Global Network
  // Configuration is enabled. If it is, identify the primary cellular network,
  // and surface the notification if the SIM lock setting is enabled.
  for (const auto& sim_info : *cellular_device->sim_infos) {
    if (!sim_info->is_primary)
      continue;
    std::string old_primary_iccid = primary_iccid_;
    primary_iccid_ = sim_info->iccid;
    if (primary_iccid_ != old_primary_iccid)
      CheckGlobalNetworkConfiguration();

    return;
  }
}

void ManagedSimLockNotifier::OnPoliciesApplied(const std::string& userhash) {
  CheckGlobalNetworkConfiguration();
}

void ManagedSimLockNotifier::CheckGlobalNetworkConfiguration() {
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
    if (network->type_state->get_cellular()->sim_lock_enabled) {
      ShowNotification();
      if (network->type_state->get_cellular()->sim_locked) {
        CellularMetricsLogger::RecordSimLockNotificationLockType(
            network->type_state->get_cellular()->sim_lock_type);
      }
      return;
    }
  }

  RemoveNotification();
}

void ManagedSimLockNotifier::Close(bool by_user) {
  if (by_user) {
    CellularMetricsLogger::RecordSimLockNotificationEvent(
        CellularMetricsLogger::SimLockNotificationEvent::kDismissed);
  }
}

void ManagedSimLockNotifier::Click(const std::optional<int>& button_index,
                                   const std::optional<std::u16string>& reply) {
  CellularMetricsLogger::RecordSimLockNotificationEvent(
      CellularMetricsLogger::SimLockNotificationEvent::kClicked);

  // When clicked, open the SIM Unlock dialog in Cellular settings if
  // we can open WebUI settings, otherwise do nothing.
  if (TrayPopupUtils::CanOpenWebUISettings()) {
    Shell::Get()->system_tray_model()->client()->ShowSettingsSimUnlock();
  } else {
    LOG(WARNING) << "Cannot open Cellular settings since it's not "
                    "possible to open OS Settings";
  }
}

void ManagedSimLockNotifier::ShowNotification() {
  std::unique_ptr<message_center::Notification> notification =
      ash::CreateSystemNotificationPtr(
          message_center::NOTIFICATION_TYPE_SIMPLE,
          kManagedSimLockNotificationId,
          l10n_util::GetStringUTF16(
              IDS_ASH_NETWORK_MANAGED_SIM_LOCK_NOTIFICATION_TITLE),
          l10n_util::GetStringUTF16(
              IDS_ASH_NETWORK_MANAGED_SIM_LOCK_NOTIFICATION_MESSAGE),
          /*display_source=*/std::u16string(), GURL(),
          message_center::NotifierId(
              message_center::NotifierType::SYSTEM_COMPONENT,
              kNotifierManagedSimLock,
              NotificationCatalogName::kManagedSimLock),
          message_center::RichNotificationData(),
          base::MakeRefCounted<message_center::ThunkNotificationDelegate>(
              weak_ptr_factory_.GetWeakPtr()),
          /*small_image=*/gfx::VectorIcon(),
          message_center::SystemNotificationWarningLevel::WARNING);
  notification->set_host_view_element_id(
      kCellularManagedSimLockNotificationElementId);

  message_center::MessageCenter* message_center =
      message_center::MessageCenter::Get();
  message_center->AddNotification(std::move(notification));
  CellularMetricsLogger::RecordSimLockNotificationEvent(
      CellularMetricsLogger::SimLockNotificationEvent::kShown);
}

void ManagedSimLockNotifier::RemoveNotification() {
  message_center::MessageCenter* message_center =
      message_center::MessageCenter::Get();
  message_center->RemoveNotification(kManagedSimLockNotificationId, false);
}

}  // namespace ash
