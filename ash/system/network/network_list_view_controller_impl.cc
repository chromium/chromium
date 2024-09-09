// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/network/network_list_view_controller_impl.h"

#include <memory>
#include <vector>

#include "ash/ash_element_identifiers.h"
#include "ash/constants/ash_features.h"
#include "ash/public/cpp/bluetooth_config_service.h"
#include "ash/resources/vector_icons/vector_icons.h"
#include "ash/session/session_controller_impl.h"
#include "ash/shell.h"
#include "ash/shell_delegate.h"
#include "ash/strings/grit/ash_strings.h"
#include "ash/style/ash_color_provider.h"
#include "ash/style/typography.h"
#include "ash/system/model/system_tray_model.h"
#include "ash/system/network/network_detailed_network_view_impl.h"
#include "ash/system/network/network_list_mobile_header_view.h"
#include "ash/system/network/network_list_network_header_view.h"
#include "ash/system/network/network_list_network_item_view.h"
#include "ash/system/network/network_utils.h"
#include "ash/system/network/tray_network_state_model.h"
#include "ash/system/tray/tray_info_label.h"
#include "ash/system/tray/tri_view.h"
#include "base/timer/timer.h"
#include "chromeos/ash/components/dbus/hermes/hermes_manager_client.h"
#include "chromeos/ash/services/bluetooth_config/public/cpp/cros_bluetooth_config_util.h"
#include "chromeos/ash/services/multidevice_setup/public/mojom/multidevice_setup.mojom.h"
#include "chromeos/services/network_config/public/cpp/cros_network_config_util.h"
#include "chromeos/services/network_config/public/mojom/cros_network_config.mojom.h"
#include "chromeos/services/network_config/public/mojom/network_types.mojom-shared.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/chromeos/styles/cros_tokens_color_mappings.h"
#include "ui/gfx/image/image_skia_operations.h"
#include "ui/gfx/paint_vector_icon.h"
#include "ui/views/controls/image_view.h"
#include "ui/views/view.h"
#include "ui/views/view_class_properties.h"

namespace ash {

namespace {

using bluetooth_config::IsBluetoothEnabledOrEnabling;
using bluetooth_config::mojom::BluetoothSystemPropertiesPtr;
using bluetooth_config::mojom::BluetoothSystemState;
using ::chromeos::network_config::NetworkTypeMatchesType;
using ::chromeos::network_config::StateIsConnected;
using ::chromeos::network_config::mojom::DeviceStateProperties;
using ::chromeos::network_config::mojom::DeviceStateType;
using ::chromeos::network_config::mojom::FilterType;
using ::chromeos::network_config::mojom::GlobalPolicy;
using ::chromeos::network_config::mojom::InhibitReason;
using ::chromeos::network_config::mojom::ManagedPropertiesPtr;
using ::chromeos::network_config::mojom::NetworkFilter;
using ::chromeos::network_config::mojom::NetworkStateProperties;
using ::chromeos::network_config::mojom::NetworkStatePropertiesPtr;
using ::chromeos::network_config::mojom::NetworkType;
using ::chromeos::network_config::mojom::OncSource;
using ::chromeos::network_config::mojom::ProxyMode;

// Delay between scan requests.
constexpr int kRequestScanDelaySeconds = 10;

constexpr auto kWifiGroupLabelPadding = gfx::Insets::TLBR(8, 22, 8, 4);

// Helper function to remove `*view` from its view hierarchy, delete the view,
// and reset the value of `*view` to be `nullptr`.
template <class T>
void RemoveAndResetViewIfExists(raw_ptr<T>* view) {
  DCHECK(view);

  if (!*view) {
    return;
  }

  views::View* parent = (*view)->parent();

  if (parent) {
    parent->RemoveChildViewT(view->ExtractAsDangling());
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
  if (!cellular_device) {
    return false;
  }
  return cellular_device->inhibit_reason !=
         chromeos::network_config::mojom::InhibitReason::kNotInhibited;
}

bool IsCellularDeviceFlashing() {
  const DeviceStateProperties* cellular_device =
      Shell::Get()->system_tray_model()->network_state_model()->GetDevice(
          NetworkType::kCellular);
  if (!cellular_device) {
    return false;
  }
  return cellular_device->is_flashing;
}

bool IsESimSupported() {
  const DeviceStateProperties* cellular_device =
      Shell::Get()->system_tray_model()->network_state_model()->GetDevice(
          NetworkType::kCellular);

  if (!cellular_device || !cellular_device->sim_infos) {
    return false;
  }

  // Check both the SIM slot infos and the number of EUICCs because the former
  // comes from Shill and the latter from Hermes, and so there may be instances
  // where one may be true while they other isn't.
  if (HermesManagerClient::Get() &&
      HermesManagerClient::Get()->GetAvailableEuiccs().empty()) {
    return false;
  }
  for (const auto& sim_info : *cellular_device->sim_infos) {
    if (!sim_info->eid.empty()) {
      return true;
    }
  }
  return false;
}

bool IsCellularSimLocked() {
  const DeviceStateProperties* cellular_device =
      Shell::Get()->system_tray_model()->network_state_model()->GetDevice(
          NetworkType::kCellular);
  return cellular_device &&
         !cellular_device->sim_lock_status->lock_type.empty();
}

NetworkType GetMobileSectionNetworkType() {
  if (features::IsInstantHotspotRebrandEnabled()) {
    return NetworkType::kCellular;
  }
  return NetworkType::kMobile;
}

}  // namespace

NetworkListViewControllerImpl::NetworkListViewControllerImpl(
    NetworkDetailedNetworkView* network_detailed_network_view)
    : model_(Shell::Get()->system_tray_model()->network_state_model()),
      network_detailed_network_view_(network_detailed_network_view) {
  DCHECK(network_detailed_network_view_);
  Shell::Get()->system_tray_model()->network_state_model()->AddObserver(this);

  GetBluetoothConfigService(
      remote_cros_bluetooth_config_.BindNewPipeAndPassReceiver());
  remote_cros_bluetooth_config_->ObserveSystemProperties(
      cros_system_properties_observer_receiver_.BindNewPipeAndPassRemote());

  if (features::IsInstantHotspotRebrandEnabled()) {
    Shell::Get()->shell_delegate()->BindMultiDeviceSetup(
        multidevice_setup_remote_.BindNewPipeAndPassReceiver());

    multidevice_setup_remote_->AddHostStatusObserver(
        host_status_observer_receiver_.BindNewPipeAndPassRemote());

    multidevice_setup_remote_->GetHostStatus(
        base::BindOnce(&NetworkListViewControllerImpl::OnHostStatusChanged,
                       weak_ptr_factory_.GetWeakPtr()));
  }

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

void NetworkListViewControllerImpl::DeviceStateListChanged() {
  UpdateMobileSection();
}

void NetworkListViewControllerImpl::GlobalPolicyChanged() {
  UpdateMobileSection();
  GetNetworkStateList();
}

void NetworkListViewControllerImpl::OnPropertiesUpdated(
    BluetoothSystemPropertiesPtr properties) {
  if (bluetooth_system_state_ == properties->system_state) {
    return;
  }

  bluetooth_system_state_ = properties->system_state;
  if (features::IsInstantHotspotRebrandEnabled()) {
    UpdateTetherHostsSection();
  } else {
    UpdateMobileSection();
  }
}

void NetworkListViewControllerImpl::GetNetworkStateList() {
  model()->cros_network_config()->GetNetworkStateList(
      NetworkFilter::New(FilterType::kVisible, NetworkType::kAll,
                         chromeos::network_config::mojom::kNoLimit),
      base::BindOnce(&NetworkListViewControllerImpl::OnGetNetworkStateList,
                     weak_ptr_factory_.GetWeakPtr()));
}

void NetworkListViewControllerImpl::OnHostStatusChanged(
    multidevice_setup::mojom::HostStatus host_status,
    const std::optional<multidevice::RemoteDevice>& host_device) {
  has_phone_eligible_for_setup_ =
      (host_status ==
       multidevice_setup::mojom::HostStatus::kEligibleHostExistsButNoHostSet);
  GetNetworkStateList();
}

void NetworkListViewControllerImpl::OnGetNetworkStateList(
    std::vector<NetworkStatePropertiesPtr> networks) {
  int old_position = network_detailed_network_view()->GetScrollPosition();

  // Indicates the current position a view will be added to in
  // `NetworkDetailedNetworkView` scroll list.
  size_t index = 0;

  // Store current views in `previous_network_views`, views which have
  // a corresponding network in `networks` will be added back to
  // `network_id_to_view_map_` any remaining views in `previous_network_views`
  // would be deleted.
  NetworkIdToViewMap previous_network_views =
      std::move(network_id_to_view_map_);
  network_id_to_view_map_.clear();

  UpdateNetworkTypeExistence(networks);

  network_detailed_network_view()->ReorderFirstListView(index++);

  // The warning message entry and the ethernet entry are placed in the
  // `network_detailed_network_view()`'s `first_list_view_`. Here this index is
  // used to indicate the current position a entry will be added to or reordered
  // in the `first_list_view_`.
  size_t first_list_item_index = 0;

  // Show a warning that the connection might be monitored if connected to a
  // VPN or if the default network has a proxy installed.
  first_list_item_index =
      ShowConnectionWarningIfNetworkMonitored(first_list_item_index);

  // Show Ethernet section first.
  first_list_item_index = CreateItemViewsIfMissingAndReorder(
      NetworkType::kEthernet, first_list_item_index, networks,
      &previous_network_views);

  if (ShouldMobileDataSectionBeShown()) {
    if (!mobile_header_view_) {
      mobile_header_view_ =
          network_detailed_network_view()->AddMobileSectionHeader();

      // Mobile toggle state is set here to avoid toggle animating on/off when
      // detailed view is opened for the first time. Enabled state will be
      // updated in subsequent calls.
      mobile_header_view_->SetToggleState(
          /*enabled=*/false,
          /*is_on=*/is_mobile_network_enabled_, /*animate_toggle=*/false);
    }

    UpdateMobileSection();
    network_detailed_network_view()->ReorderMobileTopContainer(index++);

    size_t mobile_item_index = 0;
    mobile_item_index = CreateItemViewsIfMissingAndReorder(
        GetMobileSectionNetworkType(), mobile_item_index, networks,
        &previous_network_views);

    // Add mobile status message to NetworkDetailedNetworkView's
    // `mobile_network_list_view_` if it exists.
    if (mobile_status_message_) {
      network_detailed_network_view()
          ->GetNetworkList(GetMobileSectionNetworkType())
          ->ReorderChildView(mobile_status_message_, mobile_item_index++);
    }

    if (ShouldAddESimEntry()) {
      mobile_item_index = CreateConfigureNetworkEntry(
          &add_esim_entry_, GetMobileSectionNetworkType(), mobile_item_index);
      add_esim_entry_->SetProperty(views::kElementIdentifierKey,
                                   kNetworkAddEsimElementId);
    } else {
      RemoveAndResetViewIfExists(&add_esim_entry_);
    }

    network_detailed_network_view()->ReorderMobileListView(index++);
  } else {
    RemoveAndResetViewIfExists(&mobile_header_view_);
  }

  if (features::IsInstantHotspotRebrandEnabled()) {
    if (ShouldTetherHostsSectionBeShown()) {
      if (!tether_hosts_header_view_) {
        tether_hosts_header_view_ =
            network_detailed_network_view()->AddTetherHostsSectionHeader(
                base::BindRepeating(
                    &NetworkListViewControllerImpl::UpdateTetherHostsSection,
                    weak_ptr_factory_.GetWeakPtr()));
      }

      UpdateTetherHostsSection();
      tether_hosts_header_view_->parent()->ReorderChildView(
          tether_hosts_header_view_, index++);

      size_t tether_item_index = 0;

      if (has_phone_eligible_for_setup_) {
        // This does not remote tether_hosts_status_message_, as
        // UpdateTetherHostsSection() ensures tether_hosts_status_message_ does
        // not exist if has_phone_eligible_for_setup_ is true
        tether_item_index = CreateConfigureNetworkEntry(
            &set_up_cross_device_suite_entry_, NetworkType::kTether,
            tether_item_index);
      } else {
        RemoveAndResetViewIfExists(&set_up_cross_device_suite_entry_);
        tether_item_index = CreateItemViewsIfMissingAndReorder(
            NetworkType::kTether, tether_item_index, networks,
            &previous_network_views);

        // Add tether hosts status message to NetworkDetailedNetworkView's
        // `tether_hosts_network_list_view_` if it exist.
        if (tether_hosts_status_message_) {
          network_detailed_network_view()
              ->GetNetworkList(NetworkType::kTether)
              ->ReorderChildView(tether_hosts_status_message_,
                                 tether_item_index);
        }
      }
      network_detailed_network_view()->ReorderTetherHostsListView(index++);
    } else {
      RemoveAndResetViewIfExists(&tether_hosts_header_view_);
    }
  }

  UpdateWifiSection();
  network_detailed_network_view()->ReorderNetworkTopContainer(index++);

  size_t network_item_index = 0;
  // The wifi networks are grouped into known and unknown groups.
  std::vector<NetworkStatePropertiesPtr> known_networks;
  std::vector<NetworkStatePropertiesPtr> unknown_networks;
  for (NetworkStatePropertiesPtr& network : networks) {
    if (network->source != OncSource::kNone &&
        NetworkTypeMatchesType(network->type, NetworkType::kWiFi)) {
      known_networks.push_back(std::move(network));
    } else if (NetworkTypeMatchesType(network->type, NetworkType::kWiFi)) {
      unknown_networks.push_back(std::move(network));
    }
  }
  if (!known_networks.empty()) {
    network_item_index =
        CreateWifiGroupHeader(network_item_index, /*is_known=*/true);
    network_item_index = CreateItemViewsIfMissingAndReorder(
        NetworkType::kWiFi, network_item_index, known_networks,
        &previous_network_views);
  } else {
    RemoveAndResetViewIfExists(&known_header_);
  }
  if (!unknown_networks.empty()) {
    network_item_index =
        CreateWifiGroupHeader(network_item_index, /*is_known=*/false);
    network_item_index = CreateItemViewsIfMissingAndReorder(
        NetworkType::kWiFi, network_item_index, unknown_networks,
        &previous_network_views);
  } else {
    RemoveAndResetViewIfExists(&unknown_header_);
  }
  if (is_wifi_enabled_) {
    network_item_index = CreateConfigureNetworkEntry(
        &join_wifi_entry_, NetworkType::kWiFi, network_item_index);
  } else {
    RemoveAndResetViewIfExists(&join_wifi_entry_);
  }
  network_detailed_network_view()->ReorderNetworkListView(index++);

  if (wifi_status_message_) {
    network_detailed_network_view()
        ->GetNetworkList(NetworkType::kWiFi)
        ->ReorderChildView(wifi_status_message_, network_item_index++);
  }

  UpdateScanningBarAndTimer();

  // Remaining views in `previous_network_views` are no longer needed
  // and should be deleted.
  for (const auto& id_and_view : previous_network_views) {
    auto* parent = id_and_view.second->parent();
    parent->RemoveChildViewT(id_and_view.second);
  }

  FocusLastSelectedView();
  network_detailed_network_view()->NotifyNetworkListChanged();

  // Resets the scrolling position to the old position to avoid and position
  // change during reordering the child views in the list.
  network_detailed_network_view()->ScrollToPosition(old_position);
}

void NetworkListViewControllerImpl::UpdateNetworkTypeExistence(
    const std::vector<NetworkStatePropertiesPtr>& networks) {
  has_cellular_networks_ = false;
  has_wifi_networks_ = false;
  has_tether_networks_ = false;
  connected_vpn_guid_ = std::string();

  for (auto& network : networks) {
    if (NetworkTypeMatchesType(network->type, NetworkType::kCellular)) {
      has_cellular_networks_ = true;
    } else if (NetworkTypeMatchesType(network->type, NetworkType::kWiFi)) {
      has_wifi_networks_ = true;
    } else if (NetworkTypeMatchesType(network->type, NetworkType::kTether)) {
      has_tether_networks_ = true;
    } else if (NetworkTypeMatchesType(network->type, NetworkType::kVPN) &&
               StateIsConnected(network->connection_state)) {
      connected_vpn_guid_ = network->guid;
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

size_t NetworkListViewControllerImpl::ShowConnectionWarningIfNetworkMonitored(
    size_t index) {
  const NetworkStateProperties* default_network = model()->default_network();
  const GlobalPolicy* global_policy = model()->global_policy();
  bool using_proxy =
      default_network && default_network->proxy_mode != ProxyMode::kDirect;
  bool enterprise_monitored_web_requests =
      global_policy && (global_policy->dns_queries_monitored ||
                        global_policy->report_xdr_events_enabled);

  if (!connected_vpn_guid_.empty() || using_proxy ||
      enterprise_monitored_web_requests) {
    if (!connection_warning_) {
      ShowConnectionWarning(
          /*show_managed_icon=*/enterprise_monitored_web_requests);
    }
    // If the warning is already shown check if the label needs to be updated
    // with a different message.
    else if (connection_warning_label_->GetText() != GenerateLabelText()) {
      connection_warning_label_->SetText(GenerateLabelText());
    }

    // The initial request to add a connection warning icon is missing
    // information about the management status of VPN and proxy configurations.
    // This call will request the managed network properties of the default
    // network and the VPN network (if one is active).
    if (!enterprise_monitored_web_requests) {
      MaybeShowConnectionWarningManagedIcon(using_proxy);
    }

    // The warning messages are shown in the ethernet section.
    network_detailed_network_view()
        ->GetNetworkList(NetworkType::kEthernet)
        ->ReorderChildView(connection_warning_, index++);
  } else if (connected_vpn_guid_.empty() && !using_proxy) {
    HideConnectionWarning();
    network_detailed_network_view()->MaybeRemoveFirstListView();
  }

  return index;
}

void NetworkListViewControllerImpl::MaybeShowConnectionWarningManagedIcon(
    bool using_proxy) {
  // If the proxy is set, check if it's a managed setting.
  const NetworkStateProperties* default_network = model()->default_network();
  if (using_proxy && default_network) {
    model()->cros_network_config()->GetManagedProperties(
        default_network->guid,
        base::BindOnce(
            &NetworkListViewControllerImpl::OnGetManagedPropertiesResult,
            weak_ptr_factory_.GetWeakPtr(), default_network->guid));
  } else {
    is_proxy_managed_ = false;
  }

  // If the vpn is set, check if it's a managed setting.
  if (!connected_vpn_guid_.empty()) {
    model()->cros_network_config()->GetManagedProperties(
        connected_vpn_guid_,
        base::BindOnce(
            &NetworkListViewControllerImpl::OnGetManagedPropertiesResult,
            weak_ptr_factory_.GetWeakPtr(), connected_vpn_guid_));
  } else {
    is_vpn_managed_ = false;
  }
}

bool NetworkListViewControllerImpl::ShouldAddESimEntry() const {
  const bool is_add_esim_enabled = is_mobile_network_enabled_ &&
                                   !IsCellularDeviceInhibited() &&
                                   !IsCellularDeviceFlashing();

  bool is_add_esim_visible = IsESimSupported();
  const GlobalPolicy* global_policy = model()->global_policy();

  // Adding new cellular networks is disallowed when only policy cellular
  // networks are allowed by admin.
  if (!global_policy || global_policy->allow_only_policy_cellular_networks) {
    is_add_esim_visible = false;
  }
  // The entry navigates to Settings, only add it if this can occur.
  return is_add_esim_enabled && is_add_esim_visible &&
         TrayPopupUtils::CanOpenWebUISettings();
}

void NetworkListViewControllerImpl::OnGetManagedPropertiesResult(
    const std::string& guid,
    ManagedPropertiesPtr properties) {
  // Bail out early if no connection warning is being shown.
  // This could happen if the connection warning is hidden while the async
  // GetManagedProperties step is in progress.
  if (!connection_warning_) {
    return;
  }

  // Check if the proxy is managed.
  const NetworkStateProperties* default_network = model()->default_network();
  if (default_network && default_network->guid == guid) {
    is_proxy_managed_ =
        properties && properties->proxy_settings &&
        properties->proxy_settings->type->policy_source !=
            chromeos::network_config::mojom::PolicySource::kNone;
  }
  // Check if the VPN is managed.
  if (guid == connected_vpn_guid_) {
    // TODO(b/261009968): Add check for managed WireGuard settings.
    is_vpn_managed_ =
        properties && properties->type_properties->is_vpn() &&
        properties->type_properties->get_vpn()->host &&
        properties->type_properties->get_vpn()->host->policy_source !=
            chromeos::network_config::mojom::PolicySource::kNone;
  }

  if (is_proxy_managed_ || is_vpn_managed_) {
    SetConnectionWarningIcon(connection_warning_, /*use_managed_icon=*/true);
  }
}

void NetworkListViewControllerImpl::SetConnectionWarningIcon(
    TriView* parent,
    bool use_managed_icon) {
  DCHECK(parent) << "The connection warning parent view should not be null";
  int newIconId = static_cast<int>(
      use_managed_icon
          ? NetworkListViewControllerViewChildId::kConnectionWarningManagedIcon
          : NetworkListViewControllerViewChildId::kConnectionWarningSystemIcon);

  if (connection_warning_icon_ &&
      connection_warning_icon_->GetID() == newIconId) {
    // The view is already showing the correct icon.
    return;
  }

  // Remove the previous icon if set.
  RemoveAndResetViewIfExists(&connection_warning_icon_);

  // Set 'info' icon on left side.
  std::unique_ptr<views::ImageView> image_view = base::WrapUnique(
      TrayPopupUtils::CreateMainImageView(/*use_wide_layout=*/false));
  image_view->SetImage(gfx::CreateVectorIcon(
      use_managed_icon ? kSystemTrayManagedIcon : kSystemMenuInfoIcon,
      AshColorProvider::Get()->GetContentLayerColor(
          AshColorProvider::ContentLayerType::kIconColorPrimary)));
  image_view->SetBackground(views::CreateSolidBackground(SK_ColorTRANSPARENT));
  image_view->SetID(newIconId);
  connection_warning_icon_ = image_view.get();
  parent->AddView(TriView::Container::START, image_view.release());
}

bool NetworkListViewControllerImpl::ShouldMobileDataSectionBeShown() {
  // The section should always be shown if Cellular networks are available.
  if (model()->GetDeviceState(NetworkType::kCellular) !=
      DeviceStateType::kUnavailable) {
    return true;
  }

  if (features::IsInstantHotspotRebrandEnabled()) {
    return false;
  }

  const DeviceStateType tether_state =
      model()->GetDeviceState(NetworkType::kTether);

  // Hide the section if both Cellular and Tether are UNAVAILABLE.
  if (tether_state == DeviceStateType::kUnavailable) {
    return false;
  }

  // Hide the section if Tether is PROHIBITED.
  if (tether_state == DeviceStateType::kProhibited) {
    return false;
  }

  // Secondary users cannot enable Bluetooth, and Tether is only UNINITIALIZED
  // if Bluetooth is disabled. Hide the section in this case.
  if (tether_state == DeviceStateType::kUninitialized && IsSecondaryUser()) {
    return false;
  }

  return true;
}

bool NetworkListViewControllerImpl::ShouldTetherHostsSectionBeShown() {
  // The section should never be shown if the feature flag is disabled.
  if (!features::IsInstantHotspotRebrandEnabled()) {
    return false;
  }

  // The Tether Hosts section should always be shown if there is an eligible
  // device which has not been set up yet.
  if (has_phone_eligible_for_setup_) {
    return true;
  }

  const DeviceStateType tether_state =
      model()->GetDeviceState(NetworkType::kTether);

  // Hide the section if Tether is Unavailable, Prohibited, or Disabled.
  if (tether_state == DeviceStateType::kUnavailable ||
      tether_state == DeviceStateType::kProhibited ||
      tether_state == DeviceStateType::kDisabled) {
    return false;
  }

  // Secondary users cannot enable Bluetooth, and Tether is only UNINITIALIZED
  // if Bluetooth is disabled. Hide the section in this case.
  if (tether_state == DeviceStateType::kUninitialized && IsSecondaryUser()) {
    return false;
  }

  return true;
}

size_t NetworkListViewControllerImpl::CreateWifiGroupHeader(
    size_t index,
    const bool is_known) {
  // If the headers are already created, reorder the child views and return.
  if (is_known && known_header_) {
    network_detailed_network_view()
        ->GetNetworkList(NetworkType::kWiFi)
        ->ReorderChildView(known_header_, index++);
    return index;
  }
  if (!is_known && unknown_header_) {
    network_detailed_network_view()
        ->GetNetworkList(NetworkType::kWiFi)
        ->ReorderChildView(unknown_header_, index++);
    return index;
  }

  auto header = std::make_unique<views::Label>();
  header->SetText(l10n_util::GetStringUTF16(
      is_known ? IDS_ASH_QUICK_SETTINGS_KNOWN_NETWORKS
               : IDS_ASH_QUICK_SETTINGS_UNKNOWN_NETWORKS));
  header->SetHorizontalAlignment(gfx::HorizontalAlignment::ALIGN_TO_HEAD);
  header->SetBorder(views::CreateEmptyBorder(kWifiGroupLabelPadding));
  header->SetEnabledColorId(cros_tokens::kCrosSysOnSurfaceVariant);
  TypographyProvider::Get()->StyleLabel(TypographyToken::kCrosBody2, *header);

  if (is_known) {
    known_header_ = network_detailed_network_view()
                        ->GetNetworkList(NetworkType::kWiFi)
                        ->AddChildViewAt(std::move(header), index++);
    return index;
  }

  unknown_header_ = network_detailed_network_view()
                        ->GetNetworkList(NetworkType::kWiFi)
                        ->AddChildViewAt(std::move(header), index++);
  return index;
}

size_t NetworkListViewControllerImpl::CreateConfigureNetworkEntry(
    raw_ptr<HoverHighlightView>* configure_network_entry_ptr,
    NetworkType type,
    size_t index) {
  if (*configure_network_entry_ptr) {
    network_detailed_network_view()->GetNetworkList(type)->ReorderChildView(
        *configure_network_entry_ptr, index++);
    return index;
  }

  *configure_network_entry_ptr =
      network_detailed_network_view()->AddConfigureNetworkEntry(type);
  return index++;
}

void NetworkListViewControllerImpl::UpdateMobileSection() {
  if (!mobile_header_view_) {
    return;
  }
  UpdateMobileToggleAndSetStatusMessage();
}

void NetworkListViewControllerImpl::UpdateTetherHostsSection() {
  CHECK(features::IsInstantHotspotRebrandEnabled());
  if (!tether_hosts_header_view_) {
    return;
  }

  network_detailed_network_view()->UpdateTetherHostsStatus(
      tether_hosts_header_view_->is_expanded());

  if (!IsBluetoothEnabledOrEnabling(bluetooth_system_state_) &&
      !has_phone_eligible_for_setup_) {
    CreateInfoLabelIfMissingAndUpdate(
        IDS_ASH_STATUS_TRAY_NETWORK_TETHER_NO_BLUETOOTH,
        &tether_hosts_status_message_);
    return;
  }

  if (!has_tether_networks_ && !has_phone_eligible_for_setup_) {
    CreateInfoLabelIfMissingAndUpdate(
        IDS_ASH_STATUS_TRAY_NETWORK_NO_TETHER_DEVICES_FOUND,
        &tether_hosts_status_message_);
    return;
  }

  RemoveAndResetViewIfExists(&tether_hosts_status_message_);
}

void NetworkListViewControllerImpl::UpdateWifiSection() {
  if (!wifi_header_view_) {
    wifi_header_view_ = network_detailed_network_view()->AddWifiSectionHeader();

    // WiFi toggle state is set here to avoid toggle animating on/off when
    // detailed view is opened for the first time. Enabled state will be
    // updated in subsequent calls.
    wifi_header_view_->SetToggleState(/*enabled=*/false,
                                      /*is_on=*/is_wifi_enabled_,
                                      /*animate_toggle=*/false);
  }

  wifi_header_view_->SetToggleVisibility(/*visible=*/true);
  wifi_header_view_->SetToggleState(/*enabled=*/true,
                                    /*is_on=*/is_wifi_enabled_,
                                    /*animate_toggle=*/true);

  network_detailed_network_view()->UpdateWifiStatus(is_wifi_enabled_);

  if (!is_wifi_enabled_) {
    return;
  }

  if (!has_wifi_networks_) {
    CreateInfoLabelIfMissingAndUpdate(IDS_ASH_STATUS_TRAY_NETWORK_WIFI_ENABLED,
                                      &wifi_status_message_);
  } else {
    RemoveAndResetViewIfExists(&wifi_status_message_);
  }
}

void NetworkListViewControllerImpl::UpdateMobileToggleAndSetStatusMessage() {
  if (!mobile_header_view_) {
    return;
  }

  const DeviceStateType cellular_state =
      model()->GetDeviceState(NetworkType::kCellular);
  const DeviceStateType tether_state =
      model()->GetDeviceState(NetworkType::kTether);

  const bool is_secondary_user = IsSecondaryUser();

  if (cellular_state == DeviceStateType::kUninitialized) {
    CreateInfoLabelIfMissingAndUpdate(IDS_ASH_STATUS_TRAY_INITIALIZING_CELLULAR,
                                      &mobile_status_message_);
    mobile_header_view_->SetToggleState(/*enabled=*/false,
                                        /*is_on=*/false,
                                        /*animate_toggle=*/true);
    // Updates the Mobile status to `true` so that the info label will be
    // visible, although the toggle is off.
    network_detailed_network_view()->UpdateMobileStatus(true);
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
                                          /*is_on=*/true,
                                          /*animate_toggle=*/true);
      network_detailed_network_view()->UpdateMobileStatus(true);

      if (has_cellular_networks_ || has_tether_networks_) {
        RemoveAndResetViewIfExists(&mobile_status_message_);
        return;
      }
      const InhibitReason inhibit_reason = GetCellularInhibitReason();
      if (inhibit_reason == InhibitReason::kInstallingProfile ||
          inhibit_reason == InhibitReason::kRefreshingProfileList ||
          inhibit_reason == InhibitReason::kRequestingAvailableProfiles) {
        CreateInfoLabelIfMissingAndUpdate(
            GetCellularInhibitReasonMessageId(inhibit_reason),
            &mobile_status_message_);
        return;
      }
      RemoveAndResetViewIfExists(&mobile_status_message_);
      return;
    }

    if (IsCellularDeviceFlashing()) {
      // When a device is flashing, it cannot process any new operations. Thus,
      // keep the toggle on to show users that the device is active, but set it
      // to be disabled to make it clear that users cannot update it until
      // operation completes.
      CreateInfoLabelIfMissingAndUpdate(IDS_ASH_STATUS_TRAY_UPDATING,
                                        &mobile_status_message_);

      mobile_header_view_->SetToggleVisibility(/*visible=*/true);
      mobile_header_view_->SetToggleState(/*enabled=*/false,
                                          /*is_on=*/true,
                                          /*animate_toggle=*/true);
      network_detailed_network_view()->UpdateMobileStatus(true);

      return;
    }

    const bool cellular_enabled = cellular_state == DeviceStateType::kEnabled;

    // The toggle will never be enabled for secondary users.
    bool toggle_enabled = !is_secondary_user;

    // The toggle will never be enabled during cellular state transitions.
    toggle_enabled &=
        cellular_enabled || cellular_state == DeviceStateType::kDisabled;

    // The toggle will never be enabled if the device is SIM locked and we
    // cannot open the Settings UI.
    toggle_enabled &=
        cellular_enabled ||
        Shell::Get()->session_controller()->ShouldEnableSettings() ||
        !IsCellularSimLocked();

    mobile_header_view_->SetToggleVisibility(/*visibility=*/true);
    mobile_header_view_->SetToggleState(/*enabled=*/toggle_enabled,
                                        /*is_on=*/cellular_enabled,
                                        /*animate_toggle=*/true);

    if (cellular_state == DeviceStateType::kDisabling) {
      network_detailed_network_view()->UpdateMobileStatus(true);
      CreateInfoLabelIfMissingAndUpdate(
          IDS_ASH_STATUS_TRAY_NETWORK_MOBILE_DISABLING,
          &mobile_status_message_);
      return;
    }

    network_detailed_network_view()->UpdateMobileStatus(cellular_enabled);

    if (cellular_enabled) {
      if (has_cellular_networks_ ||
          (has_tether_networks_ &&
           !features::IsInstantHotspotRebrandEnabled())) {
        RemoveAndResetViewIfExists(&mobile_status_message_);
        return;
      }

      CreateInfoLabelIfMissingAndUpdate(IDS_ASH_STATUS_TRAY_NO_MOBILE_NETWORKS,
                                        &mobile_status_message_);
      return;
    }
    return;
  }

  // When Cellular is not available, always show the toggle.
  mobile_header_view_->SetToggleVisibility(/*visibility=*/true);

  // Otherwise, toggle state and status message reflect Tether.
  if (tether_state == DeviceStateType::kUninitialized) {
    if (bluetooth_system_state_ == BluetoothSystemState::kEnabling) {
      mobile_header_view_->SetToggleState(/*enabled=*/false,
                                          /*is_on=*/true,
                                          /*animate_toggle=*/true);
      CreateInfoLabelIfMissingAndUpdate(
          IDS_ASH_STATUS_TRAY_INITIALIZING_CELLULAR, &mobile_status_message_);
      network_detailed_network_view()->UpdateMobileStatus(true);
      return;
    }
    mobile_header_view_->SetToggleState(
        /*enabled=*/!is_secondary_user, /*is_on=*/false,
        /*animate_toggle=*/true);
    CreateInfoLabelIfMissingAndUpdate(
        IDS_ASH_STATUS_TRAY_ENABLING_MOBILE_ENABLES_BLUETOOTH,
        &mobile_status_message_);
    // Updates the Mobile status to `true` so that the info label will be
    // visible, although the toggle is off.
    network_detailed_network_view()->UpdateMobileStatus(true);
    return;
  }

  const bool tether_enabled = tether_state == DeviceStateType::kEnabled;

  // Ensure that the toggle state and status message match the tether state.
  mobile_header_view_->SetToggleState(/*enabled=*/!is_secondary_user,
                                      /*is_on=*/tether_enabled,
                                      /*animate_toggle=*/true);
  network_detailed_network_view()->UpdateMobileStatus(tether_enabled);

  if (tether_enabled && !has_tether_networks_ && !has_cellular_networks_) {
    CreateInfoLabelIfMissingAndUpdate(
        IDS_ASH_STATUS_TRAY_NO_MOBILE_DEVICES_FOUND, &mobile_status_message_);
    return;
  }

  RemoveAndResetViewIfExists(&mobile_status_message_);
}

void NetworkListViewControllerImpl::CreateInfoLabelIfMissingAndUpdate(
    int message_id,
    raw_ptr<TrayInfoLabel>* info_label_ptr) {
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
    *info_label_ptr = network_detailed_network_view()
                          ->GetNetworkList(GetMobileSectionNetworkType())
                          ->AddChildView(std::move(info));
  } else if (info_label_ptr == &wifi_status_message_) {
    info->SetID(static_cast<int>(
        NetworkListViewControllerViewChildId::kWifiStatusMessage));
    *info_label_ptr = network_detailed_network_view()
                          ->GetNetworkList(NetworkType::kWiFi)
                          ->AddChildView(std::move(info));
  } else if (info_label_ptr == &tether_hosts_status_message_) {
    info->SetID(static_cast<int>(
        NetworkListViewControllerViewChildId::kTetherHostsStatusMessage));
    *info_label_ptr = network_detailed_network_view()
                          ->GetNetworkList(NetworkType::kTether)
                          ->AddChildView(std::move(info));
  } else {
    NOTREACHED();
  }
}

size_t NetworkListViewControllerImpl::CreateItemViewsIfMissingAndReorder(
    NetworkType type,
    size_t index,
    std::vector<NetworkStatePropertiesPtr>& networks,
    NetworkIdToViewMap* previous_views) {
  NetworkIdToViewMap id_to_view_map;
  NetworkListNetworkItemView* network_view = nullptr;

  for (const auto& network : networks) {
    if (!NetworkTypeMatchesType(network->type, type)) {
      continue;
    }

    const std::string& network_id = network->guid;
    auto it = previous_views->find(network_id);
    if (it == previous_views->end()) {
      network_view = network_detailed_network_view()->AddNetworkListItem(type);
    } else {
      network_view = it->second;
      previous_views->erase(it);
    }
    network_id_to_view_map_.emplace(network_id, network_view);

    network_view->UpdateViewForNetwork(network);
    network_detailed_network_view()->GetNetworkList(type)->ReorderChildView(
        network_view, index);
    network_view->SetEnabled(!IsNetworkDisabled(network));

    // Increment `index` since this position was taken by `network_view`.
    index++;
  }

  return index;
}

std::u16string NetworkListViewControllerImpl::GenerateLabelText() {
  const bool using_proxy =
      model()->default_network() &&
      model()->default_network()->proxy_mode != ProxyMode::kDirect;

  // If the device is not managed then the high risk disclosure should be shown.
  // VPN connections always show a high risk disclosure, regardless of the
  // management state.
  if ((using_proxy && !is_proxy_managed_) || !connected_vpn_guid_.empty()) {
    return l10n_util::GetStringUTF16(
        IDS_ASH_STATUS_TRAY_NETWORK_MONITORED_WARNING);
  }

  // If XDR reporting is enabled the high risk disclosure should be shown.
  if (model()->global_policy()->report_xdr_events_enabled) {
    return l10n_util::GetStringUTF16(
        IDS_ASH_STATUS_TRAY_NETWORK_MONITORED_WARNING);
  }

  return l10n_util::GetStringUTF16(IDS_ASH_STATUS_TRAY_NETWORK_MANAGED_WARNING);
}

void NetworkListViewControllerImpl::ShowConnectionWarning(
    bool show_managed_icon) {
  // Set up layout and apply sticky row property.
  std::unique_ptr<TriView> connection_warning(
      TrayPopupUtils::CreateDefaultRowView(/*use_wide_layout=*/false));
  TrayPopupUtils::ConfigureHeader(connection_warning.get());

  SetConnectionWarningIcon(connection_warning.get(),
                           /*use_managed_icon=*/show_managed_icon);

  // Set message label in middle of row.
  std::unique_ptr<views::Label> label =
      base::WrapUnique(TrayPopupUtils::CreateDefaultLabel());
  label->SetText(GenerateLabelText());
  label->SetBackground(views::CreateSolidBackground(SK_ColorTRANSPARENT));
  label->SetEnabledColor(AshColorProvider::Get()->GetContentLayerColor(
      AshColorProvider::ContentLayerType::kTextColorPrimary));
  label->SetAutoColorReadabilityEnabled(false);
  TypographyProvider::Get()->StyleLabel(TypographyToken::kCrosBody2, *label);
  label->SetID(static_cast<int>(
      NetworkListViewControllerViewChildId::kConnectionWarningLabel));
  connection_warning_label_ = label.get();

  connection_warning->AddView(TriView::Container::CENTER, std::move(label));
  connection_warning->SetContainerBorder(
      TriView::Container::CENTER, views::CreateEmptyBorder(gfx::Insets::TLBR(
                                      0, 0, 0, kTrayPopupLabelRightPadding)));

  // Nothing to the right of the text.
  connection_warning->SetContainerVisible(TriView::Container::END, false);
  connection_warning->SetID(static_cast<int>(
      NetworkListViewControllerViewChildId::kConnectionWarning));
  // The warning messages are shown in the ethernet section.
  connection_warning_ = network_detailed_network_view()
                            ->GetNetworkList(NetworkType::kEthernet)
                            ->AddChildView(std::move(connection_warning));
}

void NetworkListViewControllerImpl::HideConnectionWarning() {
  // If `connection_warning_icon_` or `connection_warning_label_` existed, they
  // must be cleared first because `connection_warning_` owns them.
  RemoveAndResetViewIfExists(&connection_warning_icon_);
  RemoveAndResetViewIfExists(&connection_warning_label_);
  RemoveAndResetViewIfExists(&connection_warning_);
}

void NetworkListViewControllerImpl::UpdateScanningBarAndTimer() {
  if (is_wifi_enabled_ && !network_scan_repeating_timer_.IsRunning()) {
    ScanAndStartTimer();
  }

  if (!is_wifi_enabled_ && network_scan_repeating_timer_.IsRunning()) {
    network_scan_repeating_timer_.Stop();
  }

  bool is_scanning_bar_visible = false;
  if (is_wifi_enabled_) {
    const DeviceStateProperties* wifi = model_->GetDevice(NetworkType::kWiFi);
    const DeviceStateProperties* tether =
        model_->GetDevice(NetworkType::kTether);

    is_scanning_bar_visible =
        (wifi && wifi->scanning) || (tether && tether->scanning);
  }

  network_detailed_network_view()->UpdateScanningBarVisibility(
      /*visible=*/is_scanning_bar_visible);
}

void NetworkListViewControllerImpl::ScanAndStartTimer() {
  RequestScan();
  network_scan_repeating_timer_.Start(
      FROM_HERE, base::Seconds(kRequestScanDelaySeconds), this,
      &NetworkListViewControllerImpl::RequestScan);
}

void NetworkListViewControllerImpl::RequestScan() {
  VLOG(1) << "Requesting Network Scan.";
  model_->cros_network_config()->RequestNetworkScan(NetworkType::kWiFi);
  model_->cros_network_config()->RequestNetworkScan(NetworkType::kTether);
}

void NetworkListViewControllerImpl::FocusLastSelectedView() {
  views::View* selected_view = nullptr;
  views::View* parent_view =
      network_detailed_network_view()->GetNetworkList(NetworkType::kAll);
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
  parent_view->DeprecatedLayoutImmediately();
  if (selected_view) {
    parent_view->ScrollRectToVisible(selected_view->bounds());
  }
}

}  // namespace ash
