// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/network/cellular_setup_notifier.h"

#include "ash/constants/ash_pref_names.h"
#include "ash/constants/notifier_catalogs.h"
#include "ash/public/cpp/network_config_service.h"
#include "ash/public/cpp/notification_utils.h"
#include "ash/public/cpp/system_tray_client.h"
#include "ash/resources/vector_icons/vector_icons.h"
#include "ash/session/session_controller_impl.h"
#include "ash/shell.h"
#include "ash/strings/grit/ash_strings.h"
#include "ash/system/model/system_tray_model.h"
#include "base/functional/bind.h"
#include "base/timer/timer.h"
#include "components/onc/onc_constants.h"
#include "components/prefs/pref_registry_simple.h"
#include "components/vector_icons/vector_icons.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/message_center/message_center.h"
#include "ui/message_center/public/cpp/message_center_constants.h"

namespace ash {
namespace {

using chromeos::network_config::mojom::DeviceStatePropertiesPtr;
using chromeos::network_config::mojom::NetworkStatePropertiesPtr;

const char kNotifierCellularSetup[] = "ash.cellular-setup";

// Delay after OOBE until notification should be shown.
constexpr base::TimeDelta kNotificationDelay = base::Minutes(15);

void OnCellularSetupNotificationClicked() {
  Shell::Get()->system_tray_model()->client()->ShowNetworkCreate(
      ::onc::network_type::kCellular);
}

// Sets kCanCellularSetupNotificationBeShown to false for the last active user.
// Returns true if the flag was successfully set, and false if not.
bool SetCellularSetupNotificationCannotBeShown() {
  PrefService* prefs =
      Shell::Get()->session_controller()->GetLastActiveUserPrefService();
  if (!prefs) {
    return false;
  }
  prefs->SetBoolean(prefs::kCanCellularSetupNotificationBeShown, false);
  return true;
}

}  // namespace

// static
void CellularSetupNotifier::RegisterProfilePrefs(PrefRegistrySimple* registry) {
  // Default value is true as we usually want to show the notification except
  // for specific conditions (cellular-incapable device, already shown).
  registry->RegisterBooleanPref(prefs::kCanCellularSetupNotificationBeShown,
                                true);
}

// static
const char CellularSetupNotifier::kCellularSetupNotificationId[] =
    "cros_cellular_setup_notifier_ids.setup_network";

CellularSetupNotifier::CellularSetupNotifier()
    : timer_(std::make_unique<base::OneShotTimer>()) {
  GetNetworkConfigService(
      remote_cros_network_config_.BindNewPipeAndPassReceiver());
  remote_cros_network_config_->AddObserver(
      cros_network_config_observer_receiver_.BindNewPipeAndPassRemote());
  Shell::Get()->session_controller()->AddObserver(this);
}

CellularSetupNotifier::~CellularSetupNotifier() {
  Shell::Get()->session_controller()->RemoveObserver(this);
}

void CellularSetupNotifier::OnSessionStateChanged(
    session_manager::SessionState state) {
  has_active_session_ = Shell::Get()->session_controller()->GetSessionState() ==
                        session_manager::SessionState::ACTIVE;
  StartStopTimer();
}

void CellularSetupNotifier::OnDeviceStateListChanged() {
  remote_cros_network_config_->GetDeviceStateList(base::BindOnce(
      &CellularSetupNotifier::OnGetDeviceStateList, base::Unretained(this)));
}

void CellularSetupNotifier::OnNetworkStateListChanged() {
  // Return early if we have already seen an activated cellular network.
  if (has_activated_cellular_network_) {
    return;
  }
  remote_cros_network_config_->GetNetworkStateList(
      chromeos::network_config::mojom::NetworkFilter::New(
          chromeos::network_config::mojom::FilterType::kAll,
          chromeos::network_config::mojom::NetworkType::kCellular,
          chromeos::network_config::mojom::kNoLimit),
      base::BindOnce(&CellularSetupNotifier::OnGetNetworkStateList,
                     base::Unretained(this)));
}

void CellularSetupNotifier::OnNetworkStateChanged(
    NetworkStatePropertiesPtr network) {
  // Return early if we have already seen an activated cellular network.
  if (has_activated_cellular_network_) {
    return;
  }
  if (network->type !=
          chromeos::network_config::mojom::NetworkType::kCellular ||
      network->type_state->get_cellular()->activation_state !=
          chromeos::network_config::mojom::ActivationStateType::kActivated) {
    return;
  }
  has_activated_cellular_network_ = true;
  SetCellularSetupNotificationCannotBeShown();
  StopTimerOrHideNotification();
}

void CellularSetupNotifier::OnGetNetworkStateList(
    std::vector<NetworkStatePropertiesPtr> networks) {
  // Check if there are any activated pSIM or eSIM networks. If any are found
  // the notification should not be shown.
  for (auto& network : networks) {
    if (network->type_state->get_cellular()->activation_state ==
        chromeos::network_config::mojom::ActivationStateType::kActivated) {
      has_activated_cellular_network_ = true;
      SetCellularSetupNotificationCannotBeShown();
      StopTimerOrHideNotification();
      return;
    }
  }
}

void CellularSetupNotifier::OnGetDeviceStateList(
    std::vector<DeviceStatePropertiesPtr> devices) {
  has_cellular_device_ = false;
  for (auto& device : devices) {
    if (device->type ==
        chromeos::network_config::mojom::NetworkType::kCellular) {
      has_cellular_device_ = true;
    }
  }
  StartStopTimer();
}

void CellularSetupNotifier::StartStopTimer() {
  PrefService* prefs =
      Shell::Get()->session_controller()->GetLastActiveUserPrefService();
  if (!prefs ||
      !prefs->GetBoolean(prefs::kCanCellularSetupNotificationBeShown)) {
    timer_->Stop();
    return;
  }

  // Notifications can only be shown during an active user session.
  if (!has_active_session_) {
    timer_->Stop();
    return;
  }

  // The notification should only be shown if the device is cellular-capable.
  if (!has_cellular_device_) {
    timer_->Stop();
    return;
  }

  // The notification should only be shown if there isn't already an activated
  // cellular network. Unlike the cases above, finding an activated cellular
  // should result in the notification never being shown so we additionally
  // update the pref.
  if (has_activated_cellular_network_) {
    SetCellularSetupNotificationCannotBeShown();
    timer_->Stop();
    return;
  }
  if (timer_->IsRunning()) {
    return;
  }

  // Wait |kNotificationDelay| before attempting to show a notification. This
  // allows the user time to set up a cellular network if they desire, and it
  // also ensures we don't spam the user with an extra notification just after
  // they log into their device for the first time.
  timer_->Start(
      FROM_HERE, kNotificationDelay,
      base::BindOnce(&CellularSetupNotifier::ShowCellularSetupNotification,
                     base::Unretained(this)));
}

void CellularSetupNotifier::StopTimerOrHideNotification() {
  timer_->Stop();
  message_center::MessageCenter* message_center =
      message_center::MessageCenter::Get();
  message_center->RemoveNotification(kCellularSetupNotificationId, false);
}

void CellularSetupNotifier::ShowCellularSetupNotification() {
  if (!SetCellularSetupNotificationCannotBeShown()) {
    // If we didn't successfully set the flag, don't show the notification or
    // else we may show the notification multiple times.
    return;
  }

  std::unique_ptr<message_center::Notification> notification =
      ash::CreateSystemNotificationPtr(
          message_center::NOTIFICATION_TYPE_SIMPLE,
          kCellularSetupNotificationId,
          l10n_util::GetStringUTF16(
              IDS_ASH_NETWORK_CELLULAR_SETUP_NOTIFICATION_TITLE),
          l10n_util::GetStringUTF16(
              IDS_ASH_NETWORK_CELLULAR_SETUP_NOTIFICATION_MESSAGE),
          /*display_source=*/std::u16string(), GURL(),
          message_center::NotifierId(
              message_center::NotifierType::SYSTEM_COMPONENT,
              kNotifierCellularSetup, NotificationCatalogName::kCellularSetup),
          message_center::RichNotificationData(),
          base::MakeRefCounted<message_center::HandleNotificationClickDelegate>(
              base::BindRepeating(&OnCellularSetupNotificationClicked)),
          kAddCellularNetworkIcon,
          message_center::SystemNotificationWarningLevel::NORMAL);

  message_center::MessageCenter* message_center =
      message_center::MessageCenter::Get();
  DCHECK(!message_center->FindVisibleNotificationById(
      kCellularSetupNotificationId));
  message_center->AddNotification(std::move(notification));
}

}  // namespace ash
