// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/network/network_section_header_view.h"

#include "ash/public/cpp/bluetooth_config_service.h"
#include "ash/public/cpp/system_tray_client.h"
#include "ash/resources/vector_icons/vector_icons.h"
#include "ash/session/session_controller_impl.h"
#include "ash/shell.h"
#include "ash/strings/grit/ash_strings.h"
#include "ash/style/icon_button.h"
#include "ash/system/model/system_tray_model.h"
#include "ash/system/network/network_utils.h"
#include "ash/system/network/tray_network_state_model.h"
#include "ash/system/tray/tray_toggle_button.h"
#include "base/bind.h"
#include "base/metrics/user_metrics.h"
#include "chromeos/ash/components/dbus/hermes/hermes_manager_client.h"
#include "chromeos/services/network_config/public/cpp/cros_network_config_util.h"
#include "chromeos/services/network_config/public/mojom/cros_network_config.mojom.h"
#include "chromeos/strings/grit/chromeos_strings.h"
#include "components/onc/onc_constants.h"
#include "components/vector_icons/vector_icons.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/views/controls/image_view.h"

using chromeos::network_config::IsInhibited;

using chromeos::network_config::mojom::DeviceStateProperties;
using chromeos::network_config::mojom::DeviceStateType;
using chromeos::network_config::mojom::FilterType;
using chromeos::network_config::mojom::GlobalPolicy;
using chromeos::network_config::mojom::NetworkFilter;
using chromeos::network_config::mojom::NetworkStateProperties;
using chromeos::network_config::mojom::NetworkStatePropertiesPtr;
using chromeos::network_config::mojom::NetworkType;

namespace ash {

namespace {

const int64_t kBluetoothTimeoutDelaySeconds = 2;

bool IsCellularDeviceInhibited() {
  const DeviceStateProperties* cellular_device =
      Shell::Get()->system_tray_model()->network_state_model()->GetDevice(
          NetworkType::kCellular);
  if (!cellular_device)
    return false;
  return cellular_device->inhibit_reason !=
         chromeos::network_config::mojom::InhibitReason::kNotInhibited;
}

bool IsCellularInhibited() {
  const DeviceStateProperties* cellular_device =
      Shell::Get()->system_tray_model()->network_state_model()->GetDevice(
          NetworkType::kCellular);
  return IsInhibited(cellular_device);
}

void ShowCellularSettings() {
  Shell::Get()
      ->system_tray_model()
      ->network_state_model()
      ->cros_network_config()
      ->GetNetworkStateList(
          NetworkFilter::New(FilterType::kVisible, NetworkType::kCellular,
                             /*limit=*/1),
          base::BindOnce([](std::vector<NetworkStatePropertiesPtr> networks) {
            std::string guid;
            if (networks.size() > 0)
              guid = networks[0]->guid;
            Shell::Get()->system_tray_model()->client()->ShowNetworkSettings(
                guid);
          }));
}

bool IsSecondaryUser() {
  SessionControllerImpl* session_controller =
      Shell::Get()->session_controller();
  return session_controller->IsActiveUserSessionStarted() &&
         !session_controller->IsUserPrimary();
}

bool IsESimSupported() {
  const DeviceStateProperties* cellular_device =
      Shell::Get()->system_tray_model()->network_state_model()->GetDevice(
          NetworkType::kCellular);

  if (!cellular_device || !cellular_device->sim_infos)
    return false;

  // Check both the SIM slot infos and the number of EUICCs because the former
  // comes from Shill and the latter from Hermes, and so there may be instances
  // where one may be true while they other isn't.
  if (HermesManagerClient::Get()->GetAvailableEuiccs().empty())
    return false;

  for (const auto& sim_info : *cellular_device->sim_infos) {
    if (!sim_info->eid.empty())
      return true;
  }
  return false;
}

int GetAddESimTooltipMessageId() {
  const DeviceStateProperties* cellular_device =
      Shell::Get()->system_tray_model()->network_state_model()->GetDevice(
          NetworkType::kCellular);

  DCHECK(cellular_device);

  switch (cellular_device->inhibit_reason) {
    case chromeos::network_config::mojom::InhibitReason::kInstallingProfile:
      return IDS_ASH_STATUS_TRAY_INHIBITED_CELLULAR_INSTALLING_PROFILE;
    case chromeos::network_config::mojom::InhibitReason::kRenamingProfile:
      return IDS_ASH_STATUS_TRAY_INHIBITED_CELLULAR_RENAMING_PROFILE;
    case chromeos::network_config::mojom::InhibitReason::kRemovingProfile:
      return IDS_ASH_STATUS_TRAY_INHIBITED_CELLULAR_REMOVING_PROFILE;
    case chromeos::network_config::mojom::InhibitReason::kConnectingToProfile:
      return IDS_ASH_STATUS_TRAY_INHIBITED_CELLULAR_CONNECTING_TO_PROFILE;
    case chromeos::network_config::mojom::InhibitReason::kRefreshingProfileList:
      return IDS_ASH_STATUS_TRAY_INHIBITED_CELLULAR_REFRESHING_PROFILE_LIST;
    case chromeos::network_config::mojom::InhibitReason::kNotInhibited:
      return IDS_ASH_STATUS_TRAY_ADD_CELLULAR_LABEL;
    case chromeos::network_config::mojom::InhibitReason::kResettingEuiccMemory:
      return IDS_ASH_STATUS_TRAY_INHIBITED_CELLULAR_RESETTING_ESIM;
    case chromeos::network_config::mojom::InhibitReason::kDisablingProfile:
      return IDS_ASH_STATUS_TRAY_INHIBITED_CELLULAR_DISABLING_PROFILE;
  }
}

}  // namespace

NetworkSectionHeaderView::NetworkSectionHeaderView(int title_id)
    : title_id_(title_id),
      model_(Shell::Get()->system_tray_model()->network_state_model()) {}

void NetworkSectionHeaderView::Init(bool enabled) {
  InitializeLayout();
  AddExtraButtons(enabled);
  AddToggleButton(enabled);
}

void NetworkSectionHeaderView::AddExtraButtons(bool enabled) {}

void NetworkSectionHeaderView::SetToggleVisibility(bool visible) {
  toggle_->SetVisible(visible);
}

void NetworkSectionHeaderView::SetToggleState(bool toggle_enabled, bool is_on) {
  toggle_->SetEnabled(toggle_enabled);
  toggle_->SetAcceptsEvents(toggle_enabled);
  toggle_->AnimateIsOn(is_on);
}

const char* NetworkSectionHeaderView::GetClassName() const {
  return "NetworkSectionHeaderView";
}

int NetworkSectionHeaderView::GetHeightForWidth(int width) const {
  // Make row height fixed avoiding layout manager adjustments.
  return GetPreferredSize().height();
}

void NetworkSectionHeaderView::InitializeLayout() {
  TrayPopupUtils::ConfigureAsStickyHeader(this);
  SetLayoutManager(std::make_unique<views::FillLayout>());
  container_ = TrayPopupUtils::CreateSubHeaderRowView(true);
  container_->AddView(
      TriView::Container::START,
      TrayPopupUtils::CreateMainImageView(/*use_wide_layout=*/false));
  AddChildView(container_);

  network_row_title_view_ = new NetworkRowTitleView(title_id_);
  container_->AddView(TriView::Container::CENTER, network_row_title_view_);
}

void NetworkSectionHeaderView::AddToggleButton(bool enabled) {
  toggle_ = new TrayToggleButton(
      base::BindRepeating(&NetworkSectionHeaderView::ToggleButtonPressed,
                          base::Unretained(this)),
      title_id_);
  toggle_->SetIsOn(enabled);
  container_->AddView(TriView::Container::END, toggle_);
}

void NetworkSectionHeaderView::ToggleButtonPressed() {
  // In the event of frequent clicks, helps to prevent a toggle button state
  // from becoming inconsistent with the async operation of enabling /
  // disabling of mobile radio. The toggle will get unlocked in the next
  // call to NetworkListView::Update(). Note that we don't disable/enable
  // because that would clear focus.
  toggle_->SetAcceptsEvents(false);
  OnToggleToggled(toggle_->GetIsOn());
}

MobileSectionHeaderView::MobileSectionHeaderView()
    : NetworkSectionHeaderView(IDS_ASH_STATUS_TRAY_NETWORK_MOBILE) {
  bool initially_enabled = model()->GetDeviceState(NetworkType::kCellular) ==
                               DeviceStateType::kEnabled ||
                           model()->GetDeviceState(NetworkType::kTether) ==
                               DeviceStateType::kEnabled;
  NetworkSectionHeaderView::Init(initially_enabled);
  model()->AddObserver(this);
  GetBluetoothConfigService(
      remote_cros_bluetooth_config_.BindNewPipeAndPassReceiver());
}

MobileSectionHeaderView::~MobileSectionHeaderView() {
  model()->RemoveObserver(this);
}

const char* MobileSectionHeaderView::GetClassName() const {
  return "MobileSectionHeaderView";
}

int MobileSectionHeaderView::UpdateToggleAndGetStatusMessage(
    bool mobile_has_networks,
    bool tether_has_networks) {
  DeviceStateType cellular_state =
      model()->GetDeviceState(NetworkType::kCellular);
  DeviceStateType tether_state = model()->GetDeviceState(NetworkType::kTether);

  bool default_toggle_enabled = !IsSecondaryUser();

  // If Cellular is available, toggle state and status message reflect Cellular.
  if (cellular_state != DeviceStateType::kUnavailable) {
    if (cellular_state == DeviceStateType::kUninitialized) {
      SetToggleVisibility(false);
      return IDS_ASH_STATUS_TRAY_INITIALIZING_CELLULAR;
    }

    if (IsCellularDeviceInhibited()) {
      // When a device is inhibited, it cannot process any new operations. Thus,
      // keep the toggle on to show users that the device is active, but set it
      // to be disabled to make it clear that users cannot update it until it
      // becomes uninhibited.
      SetToggleVisibility(true);
      SetToggleState(false /* toggle_enabled */, true /* is_on */);
      return 0;
    }

    bool toggle_enabled = default_toggle_enabled &&
                          (cellular_state == DeviceStateType::kEnabled ||
                           cellular_state == DeviceStateType::kDisabled);
    bool cellular_enabled = cellular_state == DeviceStateType::kEnabled;
    SetToggleVisibility(true);
    SetToggleState(toggle_enabled, cellular_enabled);
    if (cellular_state == DeviceStateType::kDisabling) {
      return IDS_ASH_STATUS_TRAY_NETWORK_MOBILE_DISABLING;
    }

    if (cellular_enabled) {
      if (mobile_has_networks)
        return 0;
      return IDS_ASH_STATUS_TRAY_NO_MOBILE_NETWORKS;
    }
    return IDS_ASH_STATUS_TRAY_NETWORK_MOBILE_DISABLED;
  }

  // When Cellular is not available, always show the toggle.
  SetToggleVisibility(true);

  // Tether is also unavailable (edge case).
  if (tether_state == DeviceStateType::kUnavailable) {
    SetToggleState(false /* toggle_enabled */, false /* is_on */);
    return IDS_ASH_STATUS_TRAY_NETWORK_MOBILE_DISABLED;
  }

  // Otherwise, toggle state and status message reflect Tether.
  if (tether_state == DeviceStateType::kUninitialized) {
    if (waiting_for_tether_initialize_) {
      SetToggleState(false /* toggle_enabled */, true /* is_on */);
      // "Initializing...". TODO(stevenjb): Rename the string to _MOBILE.
      return IDS_ASH_STATUS_TRAY_INITIALIZING_CELLULAR;
    } else {
      SetToggleState(default_toggle_enabled, false /* is_on */);
      return IDS_ASH_STATUS_TRAY_ENABLING_MOBILE_ENABLES_BLUETOOTH;
    }
  }

  bool tether_enabled = tether_state == DeviceStateType::kEnabled;

  if (waiting_for_tether_initialize_) {
    waiting_for_tether_initialize_ = false;
    enable_bluetooth_timer_.Stop();
    if (!tether_enabled) {
      // We enabled Bluetooth so Tether is now initialized, but it was not
      // enabled so enable it.
      model()->SetNetworkTypeEnabledState(NetworkType::kTether, true);
      SetToggleState(default_toggle_enabled, true /* is_on */);
      // "Initializing...". TODO(stevenjb): Rename the string to _MOBILE.
      return IDS_ASH_STATUS_TRAY_INITIALIZING_CELLULAR;
    }
  }

  // Ensure that the toggle state and status message match the tether state.
  SetToggleState(default_toggle_enabled, tether_enabled /* is_on */);
  if (tether_enabled && !tether_has_networks)
    return IDS_ASH_STATUS_TRAY_NO_MOBILE_DEVICES_FOUND;
  return 0;
}

void MobileSectionHeaderView::OnToggleToggled(bool is_on) {
  RecordNetworkTypeToggled(NetworkType::kMobile, is_on);
  DeviceStateType cellular_state =
      model()->GetDeviceState(NetworkType::kCellular);

  // When Cellular is available, the toggle controls Cellular enabled state.
  // (Tether may be enabled by turning on Bluetooth and turning on
  // 'Get data connection' in the Settings > Mobile data subpage).
  if (cellular_state != DeviceStateType::kUnavailable) {
    if (is_on && IsCellularInhibited()) {
      ShowCellularSettings();
      return;
    }
    model()->SetNetworkTypeEnabledState(NetworkType::kCellular, is_on);
    return;
  }

  DeviceStateType tether_state = model()->GetDeviceState(NetworkType::kTether);

  // Tether is also unavailable (edge case).
  if (tether_state == DeviceStateType::kUnavailable)
    return;

  // If Tether is available but uninitialized, we expect Bluetooth to be off.
  // Enable Bluetooth so that Tether will be initialized. Ignore edge cases
  // (e.g. Bluetooth was disabled from a different UI).
  if (tether_state == DeviceStateType::kUninitialized) {
    if (is_on && !waiting_for_tether_initialize_)
      EnableBluetooth();
    return;
  }

  // Otherwise the toggle controls the Tether enabled state.
  model()->SetNetworkTypeEnabledState(NetworkType::kTether, is_on);
}

void MobileSectionHeaderView::AddExtraButtons(bool enabled) {
  // The button navigates to Settings, only add it if this can occur.
  if (!TrayPopupUtils::CanOpenWebUISettings())
    return;

  // The button opens the eSIM setup flow, and should only be added if the
  // device is eSIM-capable.
  if (!IsESimSupported()) {
    return;
  }

  can_add_esim_button_be_enabled_ = enabled;
  const gfx::VectorIcon& icon = base::i18n::IsRTL() ? kAddCellularNetworkRtlIcon
                                                    : kAddCellularNetworkIcon;
  add_esim_button_ = new IconButton(
      base::BindRepeating(&MobileSectionHeaderView::AddCellularButtonPressed,
                          base::Unretained(this)),
      IconButton::Type::kMedium, &icon, GetAddESimTooltipMessageId());
  add_esim_button_->SetEnabled(enabled && !IsCellularDeviceInhibited());
  container()->AddView(TriView::Container::END, add_esim_button_);
  UpdateAddESimButtonVisibility();
}

void MobileSectionHeaderView::DeviceStateListChanged() {
  if (!add_esim_button_)
    return;

  if (!IsESimSupported()) {
    add_esim_button_->SetVisible(/*visible=*/false);
    return;
  }

  add_esim_button_->SetEnabled(can_add_esim_button_be_enabled_ &&
                               !IsCellularDeviceInhibited());
  add_esim_button_->SetTooltipText(
      l10n_util::GetStringUTF16(GetAddESimTooltipMessageId()));
}

void MobileSectionHeaderView::GlobalPolicyChanged() {
  UpdateAddESimButtonVisibility();
}

void MobileSectionHeaderView::UpdateAddESimButtonVisibility() {
  if (!add_esim_button_) {
    return;
  }

  const GlobalPolicy* global_policy = model()->global_policy();

  // Adding new cellular networks is disallowed when only policy cellular
  // networks are allowed by admin.
  if (!global_policy || global_policy->allow_only_policy_cellular_networks) {
    add_esim_button_->SetVisible(/*visible=*/false);
    return;
  }

  add_esim_button_->SetVisible(/*visible=*/true);
}

void MobileSectionHeaderView::AddCellularButtonPressed() {
  Shell::Get()->system_tray_model()->client()->ShowNetworkCreate(
      ::onc::network_type::kCellular);
}

void MobileSectionHeaderView::EnableBluetooth() {
  DCHECK(!waiting_for_tether_initialize_);
  remote_cros_bluetooth_config_->SetBluetoothEnabledState(true);
  waiting_for_tether_initialize_ = true;
  enable_bluetooth_timer_.Start(
      FROM_HERE, base::Seconds(kBluetoothTimeoutDelaySeconds),
      base::BindOnce(&MobileSectionHeaderView::OnEnableBluetoothTimeout,
                     weak_ptr_factory_.GetWeakPtr()));
}

void MobileSectionHeaderView::OnEnableBluetoothTimeout() {
  DCHECK(waiting_for_tether_initialize_);
  waiting_for_tether_initialize_ = false;
  SetToggleState(true /* toggle_enabled */, false /* is_on */);

  LOG(ERROR) << "Error enabling Bluetooth. Cannot enable Mobile data.";
}

WifiSectionHeaderView::WifiSectionHeaderView()
    : NetworkSectionHeaderView(IDS_ASH_STATUS_TRAY_NETWORK_WIFI) {
  bool enabled =
      model()->GetDeviceState(NetworkType::kWiFi) == DeviceStateType::kEnabled;
  NetworkSectionHeaderView::Init(enabled);
  model()->AddObserver(this);
}

WifiSectionHeaderView::~WifiSectionHeaderView() {
  model()->RemoveObserver(this);
}

void WifiSectionHeaderView::DeviceStateListChanged() {
  UpdateJoinButtonVisibility();
}

void WifiSectionHeaderView::GlobalPolicyChanged() {
  UpdateJoinButtonVisibility();
}

void WifiSectionHeaderView::SetToggleState(bool toggle_enabled, bool is_on) {
  join_button_->SetEnabled(toggle_enabled && is_on);
  NetworkSectionHeaderView::SetToggleState(toggle_enabled, is_on);
}

const char* WifiSectionHeaderView::GetClassName() const {
  return "WifiSectionHeaderView";
}

void WifiSectionHeaderView::OnToggleToggled(bool is_on) {
  RecordNetworkTypeToggled(NetworkType::kWiFi, is_on);
  model()->SetNetworkTypeEnabledState(NetworkType::kWiFi, is_on);
}

void WifiSectionHeaderView::AddExtraButtons(bool enabled) {
  auto* join_button = new IconButton(
      base::BindRepeating(&WifiSectionHeaderView::JoinButtonPressed,
                          base::Unretained(this)),
      IconButton::Type::kMedium, &vector_icons::kWifiAddIcon,
      IDS_ASH_STATUS_TRAY_OTHER_WIFI);
  join_button->SetEnabled(enabled);
  container()->AddView(TriView::Container::END, join_button);
  join_button_ = join_button;
  UpdateJoinButtonVisibility();
}

void WifiSectionHeaderView::UpdateJoinButtonVisibility() {
  if (!join_button_) {
    return;
  }

  const DeviceStateProperties* wifi_device =
      model()->GetDevice(chromeos::network_config::mojom::NetworkType::kWiFi);
  if (!wifi_device) {
    join_button_->SetVisible(/*visible=*/false);
    return;
  }

  const GlobalPolicy* global_policy = model()->global_policy();

  // Adding new network config is disallowed when only policy wifi networks
  // are allowed by admin or managed networks are available and corresponding
  // settings is enabled.
  if (!global_policy ||
      global_policy->allow_only_policy_wifi_networks_to_connect ||
      (global_policy->allow_only_policy_wifi_networks_to_connect_if_available &&
       wifi_device->managed_network_available)) {
    join_button_->SetVisible(/*visible=*/false);
    return;
  }

  join_button_->SetVisible(/*visible=*/true);
}

void WifiSectionHeaderView::JoinButtonPressed() {
  base::RecordAction(base::UserMetricsAction("StatusArea_Network_JoinOther"));
  Shell::Get()->system_tray_model()->client()->ShowNetworkCreate(
      ::onc::network_type::kWiFi);
}

}  // namespace ash
