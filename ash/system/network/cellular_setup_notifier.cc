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

const char kNotifierCellularSetup[] = "ash.cellular-setup";

// Delay after OOBE until notification should be shown.
constexpr base::TimeDelta kNotificationDelay = base::Minutes(15);

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
  Shell::Get()->system_tray_model()->client()->ShowNetworkCreate(
      ::onc::network_type::kCellular);
}

// Returns the value of kCanCellularSetupNotificationBeShown for the last active
// user. If the last active user's PrefService is null, returns false.
bool GetCanCellularSetupNotificationBeShown() {
  PrefService* prefs =
      Shell::Get()->session_controller()->GetLastActiveUserPrefService();
  if (!prefs) {
    // Return false because we don't want to show the notification if we're
    // unsure if it can be shown or not.
    return false;
  }
  return prefs->GetBoolean(prefs::kCanCellularSetupNotificationBeShown);
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
  if (Shell::Get()->session_controller()->GetSessionState() !=
      session_manager::SessionState::ACTIVE) {
    timer_->Stop();
    return;
  }

  if (!GetCanCellularSetupNotificationBeShown()) {
    // The notification has already been shown or there is some condition that
    // dictates that the notification shouldn't be shown.
    return;
  }

  // Wait |kNotificationDelay| after the user logs in before attempting to show
  // a notification. This allows the user time to set up a cellular network if
  // they desire, and it also ensures we don't spam the user with an extra
  // notification just after they log into their device for the first time.
  timer_->Start(FROM_HERE, kNotificationDelay,
                base::BindOnce(&CellularSetupNotifier::OnTimerFired,
                               base::Unretained(this)));
}

void CellularSetupNotifier::OnTimerFired() {
  timer_fired_ = true;
  MaybeShowCellularSetupNotification();
}

void CellularSetupNotifier::OnNetworkStateListChanged() {
  MaybeShowCellularSetupNotification();
}

void CellularSetupNotifier::OnNetworkStateChanged(
    chromeos::network_config::mojom::NetworkStatePropertiesPtr network) {
  if (network->type !=
          chromeos::network_config::mojom::NetworkType::kCellular ||
      network->type_state->get_cellular()->activation_state !=
          chromeos::network_config::mojom::ActivationStateType::kActivated) {
    return;
  }

  SetCellularSetupNotificationCannotBeShown();
  message_center::MessageCenter* message_center =
      message_center::MessageCenter::Get();
  message_center->RemoveNotification(kCellularSetupNotificationId, false);
}

void CellularSetupNotifier::MaybeShowCellularSetupNotification() {
  remote_cros_network_config_->GetDeviceStateList(base::BindOnce(
      &CellularSetupNotifier::OnGetDeviceStateList, base::Unretained(this)));
}

void CellularSetupNotifier::OnGetDeviceStateList(
    std::vector<chromeos::network_config::mojom::DeviceStatePropertiesPtr>
        devices) {
  if (!DoesCellularDeviceExist(devices)) {
    // Save to prefs not to show this notification again so we don't keep
    // starting the timer for a cellular-incapable device.
    SetCellularSetupNotificationCannotBeShown();
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
      // Save to prefs not to try to show this notification again so we don't
      // keep starting the timer for a user who already has an activated
      // cellular network.
      SetCellularSetupNotificationCannotBeShown();
      message_center::MessageCenter* message_center =
          message_center::MessageCenter::Get();
      message_center->RemoveNotification(kCellularSetupNotificationId, false);
      return;
    }
  }

  // Do not show notification if it has already been shown, or the timer
  // has not yet been fired.
  if (!GetCanCellularSetupNotificationBeShown() || !timer_fired_) {
    return;
  }

  ShowCellularSetupNotification();
}

// Shows the Cellular Setup notification except in cases where it is unable to
// save that it will have shown the notification.
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
  if (message_center->FindVisibleNotificationById(kCellularSetupNotificationId))
    message_center->RemoveNotification(kCellularSetupNotificationId, false);
  message_center->AddNotification(std::move(notification));
}

}  // namespace ash
