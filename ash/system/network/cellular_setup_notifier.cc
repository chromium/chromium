// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/network/cellular_setup_notifier.h"

#include "ash/public/cpp/network_config_service.h"
#include "ash/public/cpp/notification_utils.h"
#include "ash/session/session_controller_impl.h"
#include "ash/shell.h"
#include "ash/strings/grit/ash_strings.h"
#include "base/timer/timer.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/message_center/message_center.h"
#include "ui/message_center/public/cpp/message_center_constants.h"

namespace ash {
namespace {

const char kNotifierCellularSetup[] = "ash.cellular-setup";

// Delay after OOBE until notification should be shown.
constexpr base::TimeDelta kNotificationDelay = base::TimeDelta::FromMinutes(15);

bool DoesCellularDeviceExist(
    const std::vector<
        chromeos::network_config::mojom::DeviceStatePropertiesPtr>& devices) {
  for (const auto& device : devices) {
    if (device->type ==
        chromeos::network_config::mojom::NetworkType::kCellular) {
      return true;
    }
  }
  return false;
}

void OnCellularSetupNotificationClicked() {
  // TODO(crbug.com/1093185) Handle the notification being clicked.
}

}  // namespace

// static
const char CellularSetupNotifier::kCellularSetupNotificationId[] =
    "cros_cellular_setup_notifier_ids.setup_network";

CellularSetupNotifier::CellularSetupNotifier()
    : timer_(std::make_unique<base::OneShotTimer>()) {
  GetNetworkConfigService(
      remote_cros_network_config_.BindNewPipeAndPassReceiver());
  Shell::Get()->session_controller()->AddObserver(this);
}

CellularSetupNotifier::~CellularSetupNotifier() {
  Shell::Get()->session_controller()->RemoveObserver(this);
}

void CellularSetupNotifier::OnSessionStateChanged(
    session_manager::SessionState state) {
  if (Shell::Get()->session_controller()->GetSessionState() !=
      session_manager::SessionState::ACTIVE) {
    timer_->Stop();
    return;
  }

  // TODO(crbug.com/1093185) Save to prefs if this notification has been
  // shown so we only show it once. Check if this notification has been shown
  // here.

  // Wait |kNotificationDelay| after the user logs in before attempting to show
  // a notification. This allows the user time to set up a cellular network if
  // they desire, and it also ensures we don't spam the user with an extra
  // notification just after they log into their device for the first time.
  timer_->Start(FROM_HERE, kNotificationDelay,
                base::BindOnce(&CellularSetupNotifier::OnTimerFired,
                               base::Unretained(this)));
}

void CellularSetupNotifier::OnTimerFired() {
  // TODO(crbug.com/1093185) Handle case where timer expires and this method is
  // called but user is not in a state where notification should be shown (i.e.
  // logged out).
  remote_cros_network_config_->GetDeviceStateList(base::BindOnce(
      &CellularSetupNotifier::OnGetDeviceStateList, base::Unretained(this)));
}

void CellularSetupNotifier::OnGetDeviceStateList(
    std::vector<chromeos::network_config::mojom::DeviceStatePropertiesPtr>
        devices) {
  if (!DoesCellularDeviceExist(devices)) {
    // TODO(crbug.com/1093185) If the device doesn't have cellular, save to
    // prefs not to show this notification again so we don't keep starting the
    // timer for a cellular incapable device.
    return;
  }
  remote_cros_network_config_->GetNetworkStateList(
      chromeos::network_config::mojom::NetworkFilter::New(
          chromeos::network_config::mojom::FilterType::kAll,
          chromeos::network_config::mojom::NetworkType::kCellular,
          chromeos::network_config::mojom::kNoLimit),
      base::BindOnce(&CellularSetupNotifier::OnCellularNetworksList,
                     base::Unretained(this)));
}

void CellularSetupNotifier::OnCellularNetworksList(
    std::vector<chromeos::network_config::mojom::NetworkStatePropertiesPtr>
        networks) {
  // Check if there are any activated pSIM or eSIM networks. The activation
  // state property is set to activated for all eSIM services.
  for (auto& network : networks) {
    if (network->type_state->get_cellular()->activation_state ==
        chromeos::network_config::mojom::ActivationStateType::kActivated) {
      return;
    }
  }
  ShowCellularSetupNotification();
}

void CellularSetupNotifier::ShowCellularSetupNotification() {
  std::unique_ptr<message_center::Notification> notification =
      ash::CreateSystemNotification(
          message_center::NOTIFICATION_TYPE_SIMPLE,
          kCellularSetupNotificationId,
          l10n_util::GetStringUTF16(
              IDS_ASH_NETWORK_CELLULAR_SETUP_NOTIFICATION_TITLE),
          l10n_util::GetStringUTF16(
              IDS_ASH_NETWORK_CELLULAR_SETUP_NOTIFICATION_MESSAGE),
          /*display_source=*/base::string16(), GURL(),
          message_center::NotifierId(
              message_center::NotifierType::SYSTEM_COMPONENT,
              kNotifierCellularSetup),
          message_center::RichNotificationData(),
          base::MakeRefCounted<message_center::HandleNotificationClickDelegate>(
              base::BindRepeating(&OnCellularSetupNotificationClicked)),
          /*small_image=*/gfx::VectorIcon(),
          message_center::SystemNotificationWarningLevel::NORMAL);

  // TODO(crbug.com/1093185) Set the correct image for the notification.

  message_center::MessageCenter* message_center =
      message_center::MessageCenter::Get();
  if (message_center->FindVisibleNotificationById(kCellularSetupNotificationId))
    message_center->RemoveNotification(kCellularSetupNotificationId, false);
  message_center->AddNotification(std::move(notification));
}

}  // namespace ash
