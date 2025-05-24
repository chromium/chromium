// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_SYSTEM_NETWORK_NETWORK_LIST_VIEW_CONTROLLER_IMPL_H_
#define ASH_SYSTEM_NETWORK_NETWORK_LIST_VIEW_CONTROLLER_IMPL_H_

#include <optional>
#include <string>

#include "ash/ash_export.h"
#include "ash/system/network/network_detailed_network_view_impl.h"
#include "ash/system/network/network_list_mobile_header_view.h"
#include "ash/system/network/network_list_network_header_view.h"
#include "ash/system/network/network_list_network_item_view.h"
#include "ash/system/network/network_list_tether_hosts_header_view.h"
#include "ash/system/network/network_list_view_controller.h"
#include "ash/system/network/network_list_wifi_header_view.h"
#include "ash/system/network/tray_network_state_observer.h"
#include "ash/system/tray/tray_info_label.h"
#include "ash/system/tray/tray_popup_utils.h"
#include "ash/system/tray/tray_utils.h"
#include "ash/system/tray/tri_view.h"
#include "base/memory/raw_ptr.h"
#include "base/timer/timer.h"
#include "chromeos/ash/services/bluetooth_config/public/mojom/cros_bluetooth_config.mojom.h"
#include "chromeos/ash/services/multidevice_setup/public/mojom/multidevice_setup.mojom.h"
#include "chromeos/services/network_config/public/mojom/cros_network_config.mojom.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "mojo/public/cpp/bindings/remote.h"

namespace views {
class ImageView;
class Label;
}  // namespace views

namespace ash {

class HoverHighlightView;
class NetworkDetailedNetworkView;

// Implementation of NetworkListViewController.
class ASH_EXPORT NetworkListViewControllerImpl
    : public TrayNetworkStateObserver,
      public NetworkListViewController,
      public multidevice_setup::mojom::HostStatusObserver,
      public bluetooth_config::mojom::SystemPropertiesObserver {
 public:
  NetworkListViewControllerImpl(
      NetworkDetailedNetworkView* network_detailed_network_view);
  NetworkListViewControllerImpl(const NetworkListViewController&) = delete;
  NetworkListViewControllerImpl& operator=(
      const NetworkListViewControllerImpl&) = delete;
  ~NetworkListViewControllerImpl() override;

 protected:
  TrayNetworkStateModel* model() const { return model_; }

  NetworkDetailedNetworkView* network_detailed_network_view() {
    return network_detailed_network_view_;
  }

 private:
  friend class NetworkListViewControllerTest;
  friend class FakeNetworkDetailedNetworkView;

  // Used for testing. Starts at 11 to avoid collision with header view
  // child elements.
  enum class NetworkListViewControllerViewChildId {
    kConnectionWarning = 11,
    kConnectionWarningLabel = 12,
    kMobileStatusMessage = 13,
    kMobileSectionHeader = 14,
    kWifiSectionHeader = 15,
    kWifiStatusMessage = 16,
    kConnectionWarningSystemIcon = 17,
    kConnectionWarningManagedIcon = 18,
    kTetherHostsSectionHeader = 19,
    kTetherHostsStatusMessage = 20
  };

  // Map of network guids and their corresponding list item views.
  using NetworkIdToViewMap =
      base::flat_map<std::string,
                     raw_ptr<NetworkListNetworkItemView, CtnExperimental>>;

  // multidevice_setup::mojom::HostStatusObserver:
  void OnHostStatusChanged(
      multidevice_setup::mojom::HostStatus host_status,
      const std::optional<multidevice::RemoteDevice>& host_device) override;

  // TrayNetworkStateObserver:
  void ActiveNetworkStateChanged() override;
  void NetworkListChanged() override;
  void DeviceStateListChanged() override;
  void GlobalPolicyChanged() override;

  // bluetooth_config::mojom::SystemPropertiesObserver:
  void OnPropertiesUpdated(bluetooth_config::mojom::BluetoothSystemPropertiesPtr
                               properties) override;

  // Called to initialize views and when network list is recently updated.
  void GetNetworkStateList();
  void OnGetNetworkStateList(
      std::vector<chromeos::network_config::mojom::NetworkStatePropertiesPtr>
          networks);

  // Checks `networks` and caches whether Mobile network, WiFi networks and vpn
  // networks exist in the list of `networks`. Also caches if a Mobile and
  // WiFi networks are enabled.
  void UpdateNetworkTypeExistence(
      const std::vector<
          chromeos::network_config::mojom::NetworkStatePropertiesPtr>&
          networks);

  // Adds a warning indicator if connected to a VPN, if the default network
  // has a proxy installed, if the secure DNS template URIs contain user/device
  // identifiers or if DeviceReportXDREvents is enabled.
  size_t ShowConnectionWarningIfNetworkMonitored(size_t index);

  // Returns true if mobile data section should be added to view.
  bool ShouldMobileDataSectionBeShown();

  // Returns true if tether hosts section should be added to view.
  bool ShouldTetherHostsSectionBeShown();

  // Creates the wifi group header for wifi networks. If `is_known` is `true`,
  // it creates the "Known networks" header, which is the `known_header_`. If
  // `is_known` is false, it creates "Unknown networks" header, which is the
  // `unknown_header_`.
  size_t CreateWifiGroupHeader(size_t index, const bool is_known);

  // Creates and adds the "+ <network>" entry at the bottom of the wifi
  // networks (for `NetworkType::kWiFi`) or mobile networks (for
  // `NetworkType::kMobile`) based on the value of `type`.
  // `plus_network_entry_ptr` is the pointer to the "+ <network>" entry, and
  // `index` is increased by 1 to indicate the order of this view so that this
  // view can be reordered later if necessary.
  size_t CreateConfigureNetworkEntry(
      raw_ptr<HoverHighlightView>* plus_network_entry_ptr,
      NetworkType type,
      size_t index);

  // Updates Mobile data section, updates add eSIM button states and
  // calls UpdateMobileToggleAndSetStatusMessage().
  void UpdateMobileSection();

  // Updates the WiFi data section. This method creates a new header if one does
  // not exist, and will update both the WiFi toggle and "add network" button.
  // If there are no WiFi networks or WiFi is disabled, this method will also
  // add an info message.
  void UpdateWifiSection();

  // Updates the Tether Hosts section. This method creates a new header if one
  // does not exist. If Bluetooth is disabled or Instant Hotspot is enabled with
  // no nearby hosts, this method will display an error message.
  void UpdateTetherHostsSection();

  // Updated mobile data toggle states and sets mobile data status message.
  void UpdateMobileToggleAndSetStatusMessage();

  // Creates an info label if missing and updates info label message.
  void CreateInfoLabelIfMissingAndUpdate(
      int message_id,
      raw_ptr<TrayInfoLabel>* info_label_ptr);

  // Creates a NetworkListNetworkItem if it does not exist else uses the
  // existing view, also reorders it in NetworkDetailedNetworkView scroll list.
  size_t CreateItemViewsIfMissingAndReorder(
      chromeos::network_config::mojom::NetworkType type,
      size_t index,
      std::vector<chromeos::network_config::mojom::NetworkStatePropertiesPtr>&
          networks,
      NetworkIdToViewMap* previous_views);

  // Generates the correct warning to display based on the management status of
  // the network configurations and how privacy intrusive the network
  // configurations are.
  std::u16string GenerateLabelText();

  // Creates a view that indicates connections might be monitored if
  // connected to a VPN, if the default network has a proxy installed, if the
  // secure DNS template URIs contain identifiers or if DeviceReportXDREvents is
  // enabled.
  void ShowConnectionWarning(bool show_managed_icon);

  // Hides a connection warning, if visible.
  void HideConnectionWarning();

  // Determines whether a scan for WiFi and Tether networks should be requested
  // and updates the scanning bar accordingly.
  void UpdateScanningBarAndTimer();

  // Calls RequestScan() and starts a timer that will repeatedly call
  // RequestScan() after a delay.
  void ScanAndStartTimer();

  // Immediately request a WiFi and Tether network scan.
  void RequestScan();

  // Focuses on last selected view in NetworkDetailedNetworkView scroll list.
  void FocusLastSelectedView();

  // Sets an icon next to the connection warning text; if `use_managed_icon` is
  // true, the managed icon is shown, otherwise the system info icon. If an icon
  // already exists, it will be replaced.
  void SetConnectionWarningIcon(TriView* parent, bool use_managed_icon);

  // Called when the managed properties for the network identified by `guid` are
  // fetched.
  void OnGetManagedPropertiesResult(
      const std::string& guid,
      chromeos::network_config::mojom::ManagedPropertiesPtr properties);

  // Checks if the network is managed and, if true, replaces the system icon
  // shown next to the privacy warning message with a managed icon. Only called
  // if the default network has a proxy configured or if a VPN is active.
  void MaybeShowConnectionWarningManagedIcon(bool using_proxy);

  // Whether to add eSim entry or not.
  bool ShouldAddESimEntry() const;

  raw_ptr<TrayNetworkStateModel> model_;

  mojo::Remote<bluetooth_config::mojom::CrosBluetoothConfig>
      remote_cros_bluetooth_config_;
  mojo::Receiver<bluetooth_config::mojom::SystemPropertiesObserver>
      cros_system_properties_observer_receiver_{this};
  mojo::Remote<multidevice_setup::mojom::MultiDeviceSetup>
      multidevice_setup_remote_;
  mojo::Receiver<multidevice_setup::mojom::HostStatusObserver>
      host_status_observer_receiver_{this};

  bluetooth_config::mojom::BluetoothSystemState bluetooth_system_state_ =
      bluetooth_config::mojom::BluetoothSystemState::kUnavailable;

  raw_ptr<TrayInfoLabel> mobile_status_message_ = nullptr;
  raw_ptr<NetworkListMobileHeaderView> mobile_header_view_ = nullptr;
  raw_ptr<TriView> connection_warning_ = nullptr;

  // Pointer to the icon displayed next to the connection warning message when
  // a proxy or a VPN is active. Owned by `connection_warning_`. If the network
  // is monitored by the admin, via policy, it displays the managed icon,
  // otherwise the system icon.
  raw_ptr<views::ImageView> connection_warning_icon_ = nullptr;
  // Owned by `connection_warning_`.
  raw_ptr<views::Label> connection_warning_label_ = nullptr;

  raw_ptr<NetworkListWifiHeaderView> wifi_header_view_ = nullptr;
  raw_ptr<TrayInfoLabel> wifi_status_message_ = nullptr;

  raw_ptr<TrayInfoLabel> tether_hosts_status_message_ = nullptr;
  raw_ptr<NetworkListTetherHostsHeaderView> tether_hosts_header_view_ = nullptr;

  // Owned by views hierarchy.
  raw_ptr<views::Label> known_header_ = nullptr;
  raw_ptr<views::Label> unknown_header_ = nullptr;
  raw_ptr<HoverHighlightView> join_wifi_entry_ = nullptr;
  raw_ptr<HoverHighlightView> add_esim_entry_ = nullptr;
  raw_ptr<HoverHighlightView> set_up_cross_device_suite_entry_ = nullptr;

  bool has_cellular_networks_;
  bool has_wifi_networks_;
  bool has_tether_networks_;
  bool is_mobile_network_enabled_;
  bool is_wifi_enabled_;
  bool is_tether_enabled_;
  std::string connected_vpn_guid_;

  // Indicates whether the proxy associated with the default network is
  // managed.
  bool is_proxy_managed_ = false;
  // Indicates whether the proxy associated with `connected_vpn_guid_` is
  // managed.
  bool is_vpn_managed_ = false;

  // Indicates whether the user has a phone which could be set up via the
  // cross-device suite of features.
  bool has_phone_eligible_for_setup_ = false;

  raw_ptr<NetworkDetailedNetworkView> network_detailed_network_view_;
  NetworkIdToViewMap network_id_to_view_map_;

  // Timer for repeatedly requesting network scans with a delay between
  // requests.
  base::RepeatingTimer network_scan_repeating_timer_;

  base::WeakPtrFactory<NetworkListViewControllerImpl> weak_ptr_factory_{this};
};

}  // namespace ash

#endif  // ASH_SYSTEM_NETWORK_NETWORK_LIST_VIEW_CONTROLLER_IMPL_H_
