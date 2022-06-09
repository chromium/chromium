// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/network/network_list_view_controller_impl.h"

#include "ash/constants/ash_features.h"
#include "ash/public/cpp/bluetooth_config_service.h"
#include "ash/resources/vector_icons/vector_icons.h"
#include "ash/session/session_controller_impl.h"
#include "ash/shell.h"
#include "ash/strings/grit/ash_strings.h"
#include "ash/style/ash_color_provider.h"
#include "ash/system/model/system_tray_model.h"
#include "ash/system/network/network_detailed_network_view_impl.h"
#include "ash/system/network/network_list_mobile_header_view.h"
#include "ash/system/network/network_list_network_header_view.h"
#include "ash/system/network/network_list_network_item_view.h"
#include "ash/system/network/tray_network_state_model.h"
#include "ash/system/tray/tray_info_label.h"
#include "ash/system/tray/tri_view.h"
#include "chromeos/dbus/hermes/hermes_manager_client.h"
#include "chromeos/services/network_config/public/cpp/cros_network_config_util.h"
#include "chromeos/services/network_config/public/mojom/cros_network_config.mojom.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/gfx/image/image_skia_operations.h"
#include "ui/gfx/paint_vector_icon.h"
#include "ui/views/controls/image_view.h"
#include "ui/views/controls/separator.h"

namespace ash {
namespace {

using chromeos::network_config::NetworkTypeMatchesType;
using chromeos::network_config::StateIsConnected;

using chromeos::network_config::mojom::DeviceStateProperties;
using chromeos::network_config::mojom::DeviceStateType;
using chromeos::network_config::mojom::FilterType;
using chromeos::network_config::mojom::GlobalPolicy;
using chromeos::network_config::mojom::NetworkFilter;
using chromeos::network_config::mojom::NetworkStateProperties;
using chromeos::network_config::mojom::NetworkStatePropertiesPtr;
using chromeos::network_config::mojom::NetworkType;
using chromeos::network_config::mojom::ProxyMode;

using chromeos::bluetooth_config::mojom::BluetoothSystemPropertiesPtr;
using chromeos::bluetooth_config::mojom::BluetoothSystemState;

// Helper function to remove |*view| from its view hierarchy, delete the view,
// and reset the value of |*view| to be |nullptr|.
template <class T>
void RemoveAndResetViewIfExists(T** view) {
  DCHECK(view);

  if (!*view)
    return;

  views::View* parent = (*view)->parent();

  if (parent) {
    parent->RemoveChildViewT(*view);
    *view = nullptr;
  }
}

bool IsSecondaryUser() {
  SessionControllerImpl* session_controller =
      Shell::Get()->session_controller();
  return session_controller->IsActiveUserSessionStarted() &&
         !session_controller->IsUserPrimary();
}

bool IsCellularDeviceInhibited() {
  const DeviceStateProperties* cellular_device =
      Shell::Get()->system_tray_model()->network_state_model()->GetDevice(
          NetworkType::kCellular);
  if (!cellular_device)
    return false;
  return cellular_device->inhibit_reason !=
         chromeos::network_config::mojom::InhibitReason::kNotInhibited;
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
  if (chromeos::HermesManagerClient::Get()->GetAvailableEuiccs().empty())
    return false;

  for (const auto& sim_info : *cellular_device->sim_infos) {
    if (!sim_info->eid.empty())
      return true;
  }
  return false;
}

}  // namespace

NetworkListViewControllerImpl::NetworkListViewControllerImpl(
    NetworkDetailedNetworkView* network_detailed_network_view)
    : model_(Shell::Get()->system_tray_model()->network_state_model()),
      network_detailed_network_view_(network_detailed_network_view) {
  DCHECK(ash::features::IsQuickSettingsNetworkRevampEnabled());
  DCHECK(ash::features::IsBluetoothRevampEnabled());
  DCHECK(network_detailed_network_view_);
  Shell::Get()->system_tray_model()->network_state_model()->AddObserver(this);

  GetBluetoothConfigService(
      remote_cros_bluetooth_config_.BindNewPipeAndPassReceiver());
  remote_cros_bluetooth_config_->ObserveSystemProperties(
      cros_system_properties_observer_receiver_.BindNewPipeAndPassRemote());

  GetNetworkStateList();
}

NetworkListViewControllerImpl::~NetworkListViewControllerImpl() {
  Shell::Get()->system_tray_model()->network_state_model()->RemoveObserver(
      this);
}

void NetworkListViewControllerImpl::ActiveNetworkStateChanged() {
  GetNetworkStateList();
}

void NetworkListViewControllerImpl::NetworkListChanged() {
  GetNetworkStateList();
}

void NetworkListViewControllerImpl::GlobalPolicyChanged() {
  UpdateMobileSection();
};

void NetworkListViewControllerImpl::OnPropertiesUpdated(
    BluetoothSystemPropertiesPtr properties) {
  if (bluetooth_system_state_ == properties->system_state)
    return;

  bluetooth_system_state_ = properties->system_state;
  UpdateMobileSection();
}

void NetworkListViewControllerImpl::GetNetworkStateList() {
  model()->cros_network_config()->GetNetworkStateList(
      NetworkFilter::New(FilterType::kVisible, NetworkType::kAll,
                         chromeos::network_config::mojom::kNoLimit),
      base::BindOnce(&NetworkListViewControllerImpl::OnGetNetworkStateList,
                     weak_ptr_factory_.GetWeakPtr()));
}

void NetworkListViewControllerImpl::OnGetNetworkStateList(
    std::vector<NetworkStatePropertiesPtr> networks) {
  // Indicates the current position a view will be added to in
  // NetworkDetailedNetworkView scroll list.
  int index = 0;

  // Store current views in |previous_network_views|, views which have
  // a corresponding network in |networks| will be added back to
  // |network_id_to_view_map_| any remaining views in |previous_network_views|
  // would be deleted.
  NetworkIdToViewMap previous_network_views =
      std::move(network_id_to_view_map_);
  network_id_to_view_map_.clear();

  UpdateNetworkTypeExistence(networks);

  // Show a warning that the connection might be monitored if connected to a VPN
  // or if the default network has a proxy installed.
  index = ShowConnectionWarningIfVpnOrProxy(index);

  // Show Ethernet section first.
  index = CreateItemViewsIfMissingAndReorder(NetworkType::kEthernet, index,
                                             networks, &previous_network_views);

  if (ShouldMobileDataSectionBeShown()) {
    // Add separator if mobile section is not the first view child, else
    // delete unused separator.
    if (index > 0) {
      index =
          CreateSeparatorIfMissingAndReorder(index, &mobile_separator_view_);
    } else {
      RemoveAndResetViewIfExists(&mobile_separator_view_);
    }

    if (!mobile_header_view_) {
      mobile_header_view_ =
          network_detailed_network_view()->AddMobileSectionHeader();
      mobile_header_view_->SetID(static_cast<int>(
          NetworkListViewControllerViewChildId::kMobileSectionHeader));
    }

    UpdateMobileSection();

    network_detailed_network_view()->network_list()->ReorderChildView(
        mobile_header_view_, index++);

    index = CreateItemViewsIfMissingAndReorder(
        NetworkType::kMobile, index, networks, &previous_network_views);

    // Add mobile status message to NetworkDetailedNetworkView scroll list if it
    // exist.
    if (mobile_status_message_) {
      network_detailed_network_view()->network_list()->ReorderChildView(
          mobile_status_message_, index++);
    }

  } else {
    RemoveAndResetViewIfExists(&mobile_header_view_);
    RemoveAndResetViewIfExists(&mobile_separator_view_);
  }

  if (index > 0) {
    index = CreateSeparatorIfMissingAndReorder(index, &wifi_separator_view_);
  } else {
    RemoveAndResetViewIfExists(&wifi_separator_view_);
  }

  UpdateWifiSection();

  network_detailed_network_view()->network_list()->ReorderChildView(
      wifi_header_view_, index++);

  index = CreateItemViewsIfMissingAndReorder(NetworkType::kWiFi, index,
                                             networks, &previous_network_views);
  if (wifi_status_message_) {
    network_detailed_network_view()->network_list()->ReorderChildView(
        wifi_status_message_, index++);
  }

  // Remaining views in |previous_network_views| are no longer needed
  // and should be deleted.
  for (const auto& id_and_view : previous_network_views) {
    network_detailed_network_view()->network_list()->RemoveChildViewT(
        id_and_view.second);
  }

  FocusLastSelectedView();
  network_detailed_network_view()->NotifyNetworkListChanged();
}

void NetworkListViewControllerImpl::UpdateNetworkTypeExistence(
    const std::vector<NetworkStatePropertiesPtr>& networks) {
  has_mobile_networks_ = false;
  has_wifi_networks_ = false;
  is_vpn_connected_ = false;

  for (auto& network : networks) {
    if (NetworkTypeMatchesType(network->type, NetworkType::kMobile)) {
      has_mobile_networks_ = true;
    } else if (NetworkTypeMatchesType(network->type, NetworkType::kWiFi)) {
      has_wifi_networks_ = true;
    } else if (NetworkTypeMatchesType(network->type, NetworkType::kVPN) &&
               StateIsConnected(network->connection_state)) {
      is_vpn_connected_ = true;
    }
  }

  is_mobile_network_enabled_ =
      model()->GetDeviceState(NetworkType::kCellular) ==
          DeviceStateType::kEnabled ||
      model()->GetDeviceState(NetworkType::kTether) ==
          DeviceStateType::kEnabled;

  is_wifi_enabled_ =
      model()->GetDeviceState(NetworkType::kWiFi) == DeviceStateType::kEnabled;
}

int NetworkListViewControllerImpl::ShowConnectionWarningIfVpnOrProxy(
    int index) {
  const NetworkStateProperties* default_network = model()->default_network();
  bool using_proxy =
      default_network && default_network->proxy_mode != ProxyMode::kDirect;

  if (is_vpn_connected_ || using_proxy) {
    if (!connection_warning_)
      ShowConnectionWarning();

    network_detailed_network_view()->network_list()->ReorderChildView(
        connection_warning_, index++);
  } else if (!is_vpn_connected_ && !using_proxy) {
    RemoveAndResetViewIfExists(&connection_warning_);
  }

  return index;
}

bool NetworkListViewControllerImpl::ShouldMobileDataSectionBeShown() {
  // The section should always be shown if Cellular networks are available.
  if (model()->GetDeviceState(NetworkType::kCellular) !=
      DeviceStateType::kUnavailable) {
    return true;
  }

  const DeviceStateType tether_state =
      model()->GetDeviceState(NetworkType::kTether);

  // Hide the section if both Cellular and Tether are UNAVAILABLE.
  if (tether_state == DeviceStateType::kUnavailable)
    return false;

  // Hide the section if Tether is PROHIBITED.
  if (tether_state == DeviceStateType::kProhibited)
    return false;

  // Secondary users cannot enable Bluetooth, and Tether is only UNINITIALIZED
  // if Bluetooth is disabled. Hide the section in this case.
  if (tether_state == DeviceStateType::kUninitialized && IsSecondaryUser())
    return false;

  return true;
}

int NetworkListViewControllerImpl::CreateSeparatorIfMissingAndReorder(
    int index,
    views::Separator** separator_view) {
  // Separator view should never be the first view in the list.
  DCHECK(index);
  DCHECK(separator_view);

  if (*separator_view) {
    network_detailed_network_view()->network_list()->ReorderChildView(
        *separator_view, index++);
    return index;
  }

  std::unique_ptr<views::Separator> separator =
      base::WrapUnique(TrayPopupUtils::CreateListSubHeaderSeparator());

  if (separator_view == &wifi_separator_view_) {
    separator->SetID(
        static_cast<int>(NetworkListViewControllerViewChildId::kWifiSeperator));
  } else if (separator_view == &mobile_separator_view_) {
    separator->SetID(static_cast<int>(
        NetworkListViewControllerViewChildId::kMobileSeperator));
  } else {
    NOTREACHED();
  }

  *separator_view =
      network_detailed_network_view()->network_list()->AddChildViewAt(
          std::move(separator), index++);
  return index;
}

void NetworkListViewControllerImpl::UpdateMobileSection() {
  if (!mobile_header_view_)
    return;

  const bool is_add_esim_enabled =
      is_mobile_network_enabled_ && !IsCellularDeviceInhibited();

  bool is_add_esim_visible = IsESimSupported();
  const GlobalPolicy* global_policy = model()->global_policy();

  // Adding new cellular networks is disallowed when only policy cellular
  // networks are allowed by admin.
  if (ash::features::IsESimPolicyEnabled() &&
      (!global_policy || global_policy->allow_only_policy_cellular_networks)) {
    is_add_esim_visible = false;
  }

  mobile_header_view_->SetAddESimButtonState(/*enabled=*/is_add_esim_enabled,
                                             /*visible=*/is_add_esim_visible);

  UpdateMobileToggleAndSetStatusMessage();
}

void NetworkListViewControllerImpl::UpdateWifiSection() {
  if (!wifi_header_view_) {
    wifi_header_view_ = network_detailed_network_view()->AddWifiSectionHeader();
    wifi_header_view_->SetID(static_cast<int>(
        NetworkListViewControllerViewChildId::kWifiSectionHeader));
  }

  wifi_header_view_->SetJoinWifiButtonState(/*enabled=*/is_wifi_enabled_,
                                            /*visible=*/true);
  wifi_header_view_->SetToggleVisibility(/*visible=*/true);
  wifi_header_view_->SetToggleState(/*enabled=*/true,
                                    /*is_on=*/is_wifi_enabled_);

  if (!is_wifi_enabled_) {
    CreateInfoLabelIfMissingAndUpdate(IDS_ASH_STATUS_TRAY_NETWORK_WIFI_DISABLED,
                                      &wifi_status_message_);
  } else if (!has_wifi_networks_) {
    CreateInfoLabelIfMissingAndUpdate(IDS_ASH_STATUS_TRAY_NETWORK_WIFI_ENABLED,
                                      &wifi_status_message_);
  } else {
    RemoveAndResetViewIfExists(&wifi_status_message_);
  }
}

void NetworkListViewControllerImpl::UpdateMobileToggleAndSetStatusMessage() {
  if (!mobile_header_view_)
    return;

  const DeviceStateType cellular_state =
      model()->GetDeviceState(NetworkType::kCellular);
  const DeviceStateType tether_state =
      model()->GetDeviceState(NetworkType::kTether);

  const bool is_secondary_user = IsSecondaryUser();

  if (cellular_state == DeviceStateType::kUninitialized) {
    CreateInfoLabelIfMissingAndUpdate(IDS_ASH_STATUS_TRAY_INITIALIZING_CELLULAR,
                                      &mobile_status_message_);
    mobile_header_view_->SetToggleVisibility(/*visible=*/false);
    return;
  }

  if (cellular_state != DeviceStateType::kUnavailable) {
    if (IsCellularDeviceInhibited()) {
      // When a device is inhibited, it cannot process any new operations. Thus,
      // keep the toggle on to show users that the device is active, but set it
      // to be disabled to make it clear that users cannot update it until it
      // becomes uninhibited.
      mobile_header_view_->SetToggleVisibility(/*visible=*/true);
      mobile_header_view_->SetToggleState(/*enabled=*/false,
                                          /*is_on=*/true);
      RemoveAndResetViewIfExists(&mobile_status_message_);
      return;
    }

    const bool toggle_enabled =
        !is_secondary_user && (cellular_state == DeviceStateType::kEnabled ||
                               cellular_state == DeviceStateType::kDisabled);
    const bool cellular_enabled = cellular_state == DeviceStateType::kEnabled;
    mobile_header_view_->SetToggleVisibility(/*visibility=*/true);
    mobile_header_view_->SetToggleState(/*enabled=*/toggle_enabled,
                                        /*is_on=*/cellular_enabled);

    if (cellular_state == DeviceStateType::kDisabling) {
      CreateInfoLabelIfMissingAndUpdate(
          IDS_ASH_STATUS_TRAY_NETWORK_MOBILE_DISABLING,
          &mobile_status_message_);
      return;
    }

    if (cellular_enabled) {
      if (has_mobile_networks_) {
        RemoveAndResetViewIfExists(&mobile_status_message_);
        return;
      }

      CreateInfoLabelIfMissingAndUpdate(IDS_ASH_STATUS_TRAY_NO_MOBILE_NETWORKS,
                                        &mobile_status_message_);
      return;
    }

    CreateInfoLabelIfMissingAndUpdate(
        IDS_ASH_STATUS_TRAY_NETWORK_MOBILE_DISABLED, &mobile_status_message_);
    return;
  }

  // When Cellular is not available, always show the toggle.
  mobile_header_view_->SetToggleVisibility(/*visibility=*/true);

  // Otherwise, toggle state and status message reflect Tether.
  if (tether_state == DeviceStateType::kUninitialized) {
    if (bluetooth_system_state_ == BluetoothSystemState::kEnabling) {
      mobile_header_view_->SetToggleState(/*toggle_enabled=*/false,
                                          /*is_on=*/true);
      CreateInfoLabelIfMissingAndUpdate(
          IDS_ASH_STATUS_TRAY_INITIALIZING_CELLULAR, &mobile_status_message_);
      return;
    }
    mobile_header_view_->SetToggleState(
        /*toggle_enabled=*/!is_secondary_user, /*is_on=*/false);
    CreateInfoLabelIfMissingAndUpdate(
        IDS_ASH_STATUS_TRAY_ENABLING_MOBILE_ENABLES_BLUETOOTH,
        &mobile_status_message_);
    return;
  }

  const bool tether_enabled = tether_state == DeviceStateType::kEnabled;

  // Ensure that the toggle state and status message match the tether state.
  mobile_header_view_->SetToggleState(/*toggle_enabled=*/!is_secondary_user,
                                      /*is_on=*/tether_enabled);
  if (tether_enabled && !has_mobile_networks_) {
    CreateInfoLabelIfMissingAndUpdate(
        IDS_ASH_STATUS_TRAY_NO_MOBILE_DEVICES_FOUND, &mobile_status_message_);
    return;
  }

  RemoveAndResetViewIfExists(&mobile_status_message_);
}

void NetworkListViewControllerImpl::CreateInfoLabelIfMissingAndUpdate(
    int message_id,
    TrayInfoLabel** info_label_ptr) {
  DCHECK(message_id);
  DCHECK(info_label_ptr);

  TrayInfoLabel* info_label = *info_label_ptr;

  if (info_label) {
    info_label->Update(message_id);
    return;
  }

  std::unique_ptr<TrayInfoLabel> info =
      std::make_unique<TrayInfoLabel>(message_id);

  if (info_label_ptr == &mobile_status_message_) {
    info->SetID(static_cast<int>(
        NetworkListViewControllerViewChildId::kMobileStatusMessage));
  } else if (info_label_ptr == &wifi_status_message_) {
    info->SetID(static_cast<int>(
        NetworkListViewControllerViewChildId::kWifiStatusMessage));
  } else {
    NOTREACHED();
  }

  *info_label_ptr =
      network_detailed_network_view()->network_list()->AddChildView(
          std::move(info));
}

int NetworkListViewControllerImpl::CreateItemViewsIfMissingAndReorder(
    NetworkType type,
    int index,
    std::vector<NetworkStatePropertiesPtr>& networks,
    NetworkIdToViewMap* previous_views) {
  NetworkIdToViewMap id_to_view_map;
  NetworkListNetworkItemView* network_view = nullptr;

  for (const auto& network : networks) {
    if (!NetworkTypeMatchesType(network->type, type))
      continue;

    const std::string& network_id = network->guid;
    auto it = previous_views->find(network_id);

    if (it == previous_views->end()) {
      network_view = network_detailed_network_view()->AddNetworkListItem();
    } else {
      network_view = it->second;
      previous_views->erase(it);
    }
    network_id_to_view_map_.emplace(network_id, network_view);

    network_view->UpdateViewForNetwork(network);
    network_detailed_network_view()->network_list()->ReorderChildView(
        network_view, index);

    // Increment |index| since this position was taken by |network_view|.
    index++;
  }

  return index;
}

void NetworkListViewControllerImpl::ShowConnectionWarning() {
  // Set up layout and apply sticky row property.
  std::unique_ptr<TriView> connection_warning(
      TrayPopupUtils::CreateDefaultRowView());
  TrayPopupUtils::ConfigureAsStickyHeader(connection_warning.get());

  // Set 'info' icon on left side.
  std::unique_ptr<views::ImageView> image_view =
      base::WrapUnique(TrayPopupUtils::CreateMainImageView());
  image_view->SetImage(gfx::CreateVectorIcon(
      kSystemMenuInfoIcon,
      AshColorProvider::Get()->GetContentLayerColor(
          AshColorProvider::ContentLayerType::kIconColorPrimary)));
  image_view->SetBackground(views::CreateSolidBackground(SK_ColorTRANSPARENT));
  connection_warning->AddView(TriView::Container::START, image_view.release());

  // Set message label in middle of row.
  std::unique_ptr<views::Label> label =
      base::WrapUnique(TrayPopupUtils::CreateDefaultLabel());
  label->SetText(
      l10n_util::GetStringUTF16(IDS_ASH_STATUS_TRAY_NETWORK_MONITORED_WARNING));
  label->SetBackground(views::CreateSolidBackground(SK_ColorTRANSPARENT));
  label->SetEnabledColor(AshColorProvider::Get()->GetContentLayerColor(
      AshColorProvider::ContentLayerType::kTextColorPrimary));
  TrayPopupUtils::SetLabelFontList(
      label.get(), TrayPopupUtils::FontStyle::kDetailedViewLabel);
  label->SetID(static_cast<int>(
      NetworkListViewControllerViewChildId::kConnectionWarningLabel));

  connection_warning->AddView(TriView::Container::CENTER, label.release());
  connection_warning->SetContainerBorder(
      TriView::Container::CENTER, views::CreateEmptyBorder(gfx::Insets::TLBR(
                                      0, 0, 0, kTrayPopupLabelRightPadding)));

  // Nothing to the right of the text.
  connection_warning->SetContainerVisible(TriView::Container::END, false);

  connection_warning->SetID(static_cast<int>(
      NetworkListViewControllerViewChildId::kConnectionWarning));
  connection_warning_ =
      network_detailed_network_view()->network_list()->AddChildView(
          std::move(connection_warning));
}

void NetworkListViewControllerImpl::FocusLastSelectedView() {
  views::View* selected_view = nullptr;
  views::View* parent_view = network_detailed_network_view()->network_list();
  for (const auto& [network_id, view] : network_id_to_view_map_) {
    // The within_bounds check is necessary when the network list goes beyond
    // the visible area (i.e. scrolling) and the mouse is below the tray pop-up.
    // The items not in view in the tray pop-up keep going down and have
    // View::GetVisibility() == true but they are masked and not seen by the
    // user. When the mouse is below the list where the item would be if the
    // list continued downward, IsMouseHovered() is true and this will trigger
    // an incorrect programmatic scroll if we don't stop it. The bounds check
    // ensures the view is actually visible within the tray pop-up.
    bool within_bounds =
        parent_view->GetBoundsInScreen().Intersects(view->GetBoundsInScreen());
    if (within_bounds && view->IsMouseHovered()) {
      selected_view = view;
      break;
    }
  }

  parent_view->SizeToPreferredSize();
  parent_view->Layout();
  if (selected_view)
    parent_view->ScrollRectToVisible(selected_view->bounds());
}

}  // namespace ash
