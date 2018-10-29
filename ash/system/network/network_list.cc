// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/network/network_list.h"

#include <memory>
#include <utility>

#include "ash/metrics/user_metrics_recorder.h"
#include "ash/public/cpp/ash_view_ids.h"
#include "ash/resources/vector_icons/vector_icons.h"
#include "ash/session/session_controller.h"
#include "ash/shell.h"
#include "ash/shell_delegate.h"
#include "ash/strings/grit/ash_strings.h"
#include "ash/system/bluetooth/bluetooth_power_controller.h"
#include "ash/system/model/system_tray_model.h"
#include "ash/system/network/network_icon.h"
#include "ash/system/network/network_icon_animation.h"
#include "ash/system/network/network_info.h"
#include "ash/system/network/network_row_title_view.h"
#include "ash/system/network/network_state_list_detailed_view.h"
#include "ash/system/power/power_status.h"
#include "ash/system/tray/hover_highlight_view.h"
#include "ash/system/tray/system_menu_button.h"
#include "ash/system/tray/tray_constants.h"
#include "ash/system/tray/tray_info_label.h"
#include "ash/system/tray/tray_popup_item_style.h"
#include "ash/system/tray/tray_popup_utils.h"
#include "ash/system/tray/tri_view.h"
#include "ash/system/unified/top_shortcut_button.h"
#include "base/i18n/number_formatting.h"
#include "base/logging.h"
#include "base/memory/weak_ptr.h"
#include "base/strings/string16.h"
#include "base/strings/utf_string_conversions.h"
#include "base/timer/timer.h"
#include "chromeos/dbus/dbus_thread_manager.h"
#include "chromeos/network/device_state.h"
#include "chromeos/network/managed_network_configuration_handler.h"
#include "chromeos/network/network_state.h"
#include "chromeos/network/network_state_handler.h"
#include "chromeos/network/network_state_handler_observer.h"
#include "chromeos/network/proxy/ui_proxy_config_service.h"
#include "chromeos/strings/grit/chromeos_strings.h"
#include "components/device_event_log/device_event_log.h"
#include "components/onc/onc_constants.h"
#include "components/vector_icons/vector_icons.h"
#include "device/bluetooth/bluetooth_adapter_factory.h"
#include "third_party/cros_system_api/dbus/service_constants.h"
#include "third_party/skia/include/core/SkColor.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/ui_base_features.h"
#include "ui/gfx/canvas.h"
#include "ui/gfx/font.h"
#include "ui/gfx/image/image_skia.h"
#include "ui/gfx/image/image_skia_operations.h"
#include "ui/gfx/paint_vector_icon.h"
#include "ui/gfx/text_constants.h"
#include "ui/views/background.h"
#include "ui/views/border.h"
#include "ui/views/controls/button/toggle_button.h"
#include "ui/views/controls/image_view.h"
#include "ui/views/controls/label.h"
#include "ui/views/controls/scroll_view.h"
#include "ui/views/controls/separator.h"
#include "ui/views/layout/fill_layout.h"
#include "ui/views/view.h"

using chromeos::ManagedNetworkConfigurationHandler;
using chromeos::NetworkHandler;
using chromeos::NetworkStateHandler;
using chromeos::NetworkTypePattern;

namespace ash {
namespace tray {
namespace {

const int64_t kBluetoothTimeoutDelaySeconds = 2;
const int kMobileNetworkBatteryIconSize = 14;
const int kPowerStatusPaddingRight = 10;

bool IsCellularSimLocked() {
  const chromeos::DeviceState* cellular_device =
      NetworkHandler::Get()->network_state_handler()->GetDeviceStateByType(
          NetworkTypePattern::Cellular());
  return cellular_device && cellular_device->IsSimLocked();
}

void ShowCellularSettings() {
  const chromeos::NetworkState* cellular_network =
      NetworkHandler::Get()->network_state_handler()->FirstNetworkByType(
          NetworkTypePattern::Cellular());
  Shell::Get()->system_tray_model()->client_ptr()->ShowNetworkSettings(
      cellular_network ? cellular_network->guid() : std::string());
}

bool IsSecondaryUser() {
  SessionController* session_controller = Shell::Get()->session_controller();
  return session_controller->IsActiveUserSessionStarted() &&
         !session_controller->IsUserPrimary();
}

}  // namespace

// A header row for sections in network detailed view which contains a title and
// a toggle button to turn on/off the section. Subclasses are given the
// opportunity to add extra buttons before the toggle button is added.
class NetworkListView::SectionHeaderRowView : public views::View,
                                              public views::ButtonListener {
 public:
  explicit SectionHeaderRowView(int title_id)
      : title_id_(title_id),
        container_(nullptr),
        network_row_title_view_(nullptr),
        toggle_(nullptr) {}

  ~SectionHeaderRowView() override = default;

  virtual void Init(bool enabled) {
    InitializeLayout();
    AddExtraButtons(enabled);
    AddToggleButton(enabled);
  }

  void SetSubtitle(int subtitle_id) {
    network_row_title_view_->SetSubtitle(subtitle_id);
  }

  virtual void SetToggleState(bool toggle_enabled, bool is_on) {
    toggle_->SetEnabled(toggle_enabled);
    toggle_->set_accepts_events(toggle_enabled);
    toggle_->SetIsOn(is_on, true /* animate */);
  }

 protected:
  // This is called before the toggle button is added to give subclasses an
  // opportunity to add more buttons before the toggle button. Subclasses can
  // add buttons to container() using AddChildView().
  virtual void AddExtraButtons(bool enabled) {}

  // Called when |toggle_| is clicked and toggled. Subclasses can override to
  // enabled/disable their respective technology, for example.
  virtual void OnToggleToggled(bool is_on) = 0;

  TriView* container() const { return container_; }

  int GetHeightForWidth(int w) const override {
    // Make row height fixed avoiding layout manager adjustments.
    return GetPreferredSize().height();
  }

  // views::ButtonListener:
  void ButtonPressed(views::Button* sender, const ui::Event& event) override {
    DCHECK_EQ(toggle_, sender);
    // In the event of frequent clicks, helps to prevent a toggle button state
    // from becoming inconsistent with the async operation of enabling /
    // disabling of mobile radio. The toggle will get unlocked in the next
    // call to NetworkListView::Update(). Note that we don't disable/enable
    // because that would clear focus.
    toggle_->set_accepts_events(false);
    OnToggleToggled(toggle_->is_on());
  }

 private:
  void InitializeLayout() {
    TrayPopupUtils::ConfigureAsStickyHeader(this);
    SetLayoutManager(std::make_unique<views::FillLayout>());
    container_ = TrayPopupUtils::CreateSubHeaderRowView(true);
    container_->AddView(TriView::Container::START,
                        TrayPopupUtils::CreateMainImageView());
    AddChildView(container_);

    network_row_title_view_ = new NetworkRowTitleView(title_id_);
    container_->AddView(TriView::Container::CENTER, network_row_title_view_);
  }

  void AddToggleButton(bool enabled) {
    toggle_ = TrayPopupUtils::CreateToggleButton(this, title_id_);
    toggle_->SetIsOn(enabled, false);
    container_->AddView(TriView::Container::END, toggle_);
  }

  // Resource ID for the string to use as the title of the section and for the
  // accessible text on the section header toggle button.
  const int title_id_;

  // View containing header row views, including title, toggle, and extra
  // buttons.
  TriView* container_;

  // View containing the header row view. Is a child of the CENTER of
  // |container_|.
  NetworkRowTitleView* network_row_title_view_;

  // ToggleButton to toggle section on or off.
  views::ToggleButton* toggle_;

  DISALLOW_COPY_AND_ASSIGN(SectionHeaderRowView);
};

namespace {

// "Mobile Data" header row. Mobile Data reflects both Cellular state and
// Tether state. When both technologies are available, Cellular state takes
// precedence over Tether (but in some cases Tether state may be shown).
class MobileHeaderRowView : public NetworkListView::SectionHeaderRowView,
                            public chromeos::NetworkStateHandlerObserver {
 public:
  explicit MobileHeaderRowView(NetworkStateHandler* network_state_handler)
      : SectionHeaderRowView(IDS_ASH_STATUS_TRAY_NETWORK_MOBILE),
        network_state_handler_(network_state_handler),
        weak_ptr_factory_(this) {
    network_state_handler_->AddObserver(this, FROM_HERE);
  }

  ~MobileHeaderRowView() override {
    network_state_handler_->RemoveObserver(this, FROM_HERE);
  }

  void Init(bool enabled) override {
    SectionHeaderRowView::Init(enabled);
    UpdateState();
  }

  const char* GetClassName() const override { return "MobileHeaderRowView"; }

 protected:
  // NetworkListView::SectionHeaderRowView:
  void OnToggleToggled(bool is_on) override {
    NetworkStateHandler::TechnologyState cellular_state =
        network_state_handler_->GetTechnologyState(
            NetworkTypePattern::Cellular());

    // When Cellular is available, the toggle controls Cellular enabled state.
    // (Tether may be enabled by turning on Bluetooth and turning on
    // 'Get data connection' in the Settings > Mobile data subpage).
    if (cellular_state != NetworkStateHandler::TECHNOLOGY_UNAVAILABLE) {
      if (is_on && IsCellularSimLocked()) {
        ShowCellularSettings();
        return;
      }
      network_state_handler_->SetTechnologyEnabled(
          NetworkTypePattern::Cellular(), is_on,
          chromeos::network_handler::ErrorCallback());
      return;
    }

    NetworkStateHandler::TechnologyState tether_state =
        network_state_handler_->GetTechnologyState(
            NetworkTypePattern::Tether());

    // Tether is also unavailable (edge case).
    if (tether_state == NetworkStateHandler::TECHNOLOGY_UNAVAILABLE)
      return;

    // If Tether is available but uninitialized, we expect Bluetooth to be off.
    // Enable Bluetooth so that Tether will be initialized. Ignore edge cases
    // (e.g. Bluetooth was disabled from a different UI).
    if (tether_state == NetworkStateHandler::TECHNOLOGY_UNINITIALIZED) {
      if (is_on && !waiting_for_tether_initialize_)
        EnableBluetooth();
      return;
    }

    // Otherwise the toggle controls the Tether enabled state.
    network_state_handler_->SetTechnologyEnabled(
        NetworkTypePattern::Tether(), is_on,
        chromeos::network_handler::ErrorCallback());
  }

  // chromeos::NetworkStateHandlerObserver

  // Called when the available devices changes.
  void DeviceListChanged() override { UpdateState(); }

  // Called when the state of a device changes (e.g. the enabled state).
  void DevicePropertiesUpdated(const chromeos::DeviceState* device) override {
    UpdateState();
  }

  void NetworkListChanged() override { UpdateState(); }

 private:
  void UpdateState() {
    NetworkStateHandler::TechnologyState cellular_state =
        network_state_handler_->GetTechnologyState(
            NetworkTypePattern::Cellular());
    NetworkStateHandler::TechnologyState tether_state =
        network_state_handler_->GetTechnologyState(
            NetworkTypePattern::Tether());

    bool default_toggle_enabled = !IsSecondaryUser();

    // If Cellular is available, toggle state and subtitle reflect Cellular.
    if (cellular_state != NetworkStateHandler::TECHNOLOGY_UNAVAILABLE) {
      const chromeos::DeviceState* cellular_device =
          network_state_handler_->GetDeviceStateByType(
              NetworkTypePattern::Cellular());
      bool cellular_enabled =
          cellular_state == NetworkStateHandler::TECHNOLOGY_ENABLED;
      SetToggleState(default_toggle_enabled, cellular_enabled);

      int subtitle = 0;
      if (!cellular_device ||
          cellular_state == NetworkStateHandler::TECHNOLOGY_UNINITIALIZED) {
        subtitle = IDS_ASH_STATUS_TRAY_INITIALIZING_CELLULAR;
      } else if (cellular_device->scanning()) {
        subtitle = IDS_ASH_STATUS_TRAY_MOBILE_SCANNING;
      } else if (cellular_device->IsSimAbsent()) {
        subtitle = IDS_ASH_STATUS_TRAY_SIM_CARD_MISSING;
      } else if (cellular_device->IsSimLocked()) {
        subtitle = IDS_ASH_STATUS_TRAY_SIM_CARD_LOCKED;
      } else {
        const chromeos::NetworkState* mobile_network =
            network_state_handler_->FirstNetworkByType(
                NetworkTypePattern::Mobile());
        if (cellular_enabled &&
            (!mobile_network || mobile_network->IsDefaultCellular())) {
          // If no connectable Cellular network is available (see
          // network_state_handler.h re: IsDefaultCellular), show 'turn on
          // Bluetooth' if Tether is available but not initialized, otherwise
          // show 'no networks'.
          if (tether_state == NetworkStateHandler::TECHNOLOGY_UNINITIALIZED)
            subtitle = IDS_ENABLE_BLUETOOTH;
          else
            subtitle = IDS_ASH_STATUS_TRAY_NO_MOBILE_NETWORKS;
        }
      }
      SetSubtitle(subtitle);
      return;
    }

    // Tether is also unavailable (edge case).
    if (tether_state == NetworkStateHandler::TECHNOLOGY_UNAVAILABLE) {
      SetToggleState(false /* toggle_enabled */, false /* is_on */);
      SetSubtitle(IDS_ASH_STATUS_TRAY_NETWORK_MOBILE_DISABLED);
      return;
    }

    // Otherwise, toggle state and subtitle reflect Tether.

    if (tether_state == NetworkStateHandler::TECHNOLOGY_UNINITIALIZED) {
      if (waiting_for_tether_initialize_) {
        SetToggleState(false /* toggle_enabled */, true /* is_on */);
        // "Initializing...". TODO(stevenjb): Rename the string to _MOBILE.
        SetSubtitle(IDS_ASH_STATUS_TRAY_INITIALIZING_CELLULAR);
      } else {
        SetToggleState(default_toggle_enabled, false /* is_on */);
        SetSubtitle(IDS_ASH_STATUS_TRAY_ENABLING_MOBILE_ENABLES_BLUETOOTH);
      }
      return;
    }

    bool tether_enabled =
        tether_state == NetworkStateHandler::TECHNOLOGY_ENABLED;

    if (waiting_for_tether_initialize_) {
      waiting_for_tether_initialize_ = false;
      enable_bluetooth_timer_.Stop();
      if (!tether_enabled) {
        // We enabled Bluetooth so Tether is now initialized, but it was not
        // enabled so enable it.
        network_state_handler_->SetTechnologyEnabled(
            NetworkTypePattern::Tether(), true /* enabled */,
            chromeos::network_handler::ErrorCallback());
        SetToggleState(default_toggle_enabled, true /* is_on */);
        // "Initializing...". TODO(stevenjb): Rename the string to _MOBILE.
        SetSubtitle(IDS_ASH_STATUS_TRAY_INITIALIZING_CELLULAR);
        return;
      }
    }

    // Ensure that the toggle state and subtitle match the tether state.
    int subtitle = 0;
    if (tether_enabled && !network_state_handler_->FirstNetworkByType(
                              NetworkTypePattern::Tether())) {
      subtitle = IDS_ASH_STATUS_TRAY_NO_MOBILE_DEVICES_FOUND;
    }
    SetToggleState(default_toggle_enabled, tether_enabled /* is_on */);
    SetSubtitle(subtitle);
  }

  // When Tether is disabled because Bluetooth is off, then enabling Bluetooth
  // will enable Tether. If enabling Bluetooth takes longer than some timeout
  // period, it is assumed that there was an error. In that case, Tether will
  // remain uninitialized and Mobile Data will remain toggled off.
  void EnableBluetooth() {
    DCHECK(!waiting_for_tether_initialize_);

    Shell::Get()
        ->bluetooth_power_controller()
        ->SetPrimaryUserBluetoothPowerSetting(true /* enabled */);
    waiting_for_tether_initialize_ = true;
    enable_bluetooth_timer_.Start(
        FROM_HERE, base::TimeDelta::FromSeconds(kBluetoothTimeoutDelaySeconds),
        base::Bind(&MobileHeaderRowView::OnEnableBluetoothTimeout,
                   weak_ptr_factory_.GetWeakPtr()));
  }

  void OnEnableBluetoothTimeout() {
    DCHECK(waiting_for_tether_initialize_);
    waiting_for_tether_initialize_ = false;
    SetToggleState(true /* toggle_enabled */, false /* is_on */);

    LOG(ERROR) << "Error enabling Bluetooth. Cannot enable Mobile data.";
  }

  NetworkStateHandler* network_state_handler_;

  bool waiting_for_tether_initialize_ = false;
  base::OneShotTimer enable_bluetooth_timer_;

  base::WeakPtrFactory<MobileHeaderRowView> weak_ptr_factory_;

  DISALLOW_COPY_AND_ASSIGN(MobileHeaderRowView);
};

class WifiHeaderRowView : public NetworkListView::SectionHeaderRowView {
 public:
  WifiHeaderRowView()
      : SectionHeaderRowView(IDS_ASH_STATUS_TRAY_NETWORK_WIFI),
        join_button_(nullptr) {}

  ~WifiHeaderRowView() override = default;

  void SetToggleState(bool toggle_enabled, bool is_on) override {
    join_button_->SetEnabled(toggle_enabled && is_on);
    SectionHeaderRowView::SetToggleState(toggle_enabled, is_on);
  }

  const char* GetClassName() const override { return "WifiHeaderRowView"; }

 protected:
  // SectionHeaderRowView:
  void OnToggleToggled(bool is_on) override {
    NetworkStateHandler* handler =
        NetworkHandler::Get()->network_state_handler();
    handler->SetTechnologyEnabled(NetworkTypePattern::WiFi(), is_on,
                                  chromeos::network_handler::ErrorCallback());
  }

  void AddExtraButtons(bool enabled) override {
    const SkColor prominent_color = GetNativeTheme()->GetSystemColor(
        ui::NativeTheme::kColorId_ProminentButtonColor);
    gfx::ImageSkia normal_image = network_icon::GetImageForNewWifiNetwork(
        SkColorSetA(prominent_color, kJoinIconAlpha),
        SkColorSetA(prominent_color, kJoinBadgeAlpha));
    gfx::ImageSkia disabled_image = network_icon::GetImageForNewWifiNetwork(
        SkColorSetA(prominent_color, kDisabledJoinIconAlpha),
        SkColorSetA(prominent_color, kDisabledJoinBadgeAlpha));
    auto* join_button = new TopShortcutButton(this, vector_icons::kWifiAddIcon,
                                              IDS_ASH_STATUS_TRAY_OTHER_WIFI);
    join_button->SetEnabled(enabled);
    container()->AddView(TriView::Container::END, join_button);
    join_button_ = join_button;
  }

  void ButtonPressed(views::Button* sender, const ui::Event& event) override {
    if (sender == join_button_) {
      Shell::Get()->metrics()->RecordUserMetricsAction(
          UMA_STATUS_AREA_NETWORK_JOIN_OTHER_CLICKED);
      Shell::Get()->system_tray_model()->client_ptr()->ShowNetworkCreate(
          ::onc::network_type::kWiFi);
      return;
    }
    SectionHeaderRowView::ButtonPressed(sender, event);
  }

 private:
  // Full opacity for badge.
  static constexpr int kJoinBadgeAlpha = 0xFF;

  // .30 opacity for icon.
  static constexpr int kJoinIconAlpha = 0x4D;

  // .38 opacity for disabled badge.
  static constexpr int kDisabledJoinBadgeAlpha = 0x61;

  // .30 * .38 opacity for disabled icon.
  static constexpr int kDisabledJoinIconAlpha = 0x1D;

  // A button to invoke "Join Wi-Fi network" dialog.
  views::Button* join_button_;

  DISALLOW_COPY_AND_ASSIGN(WifiHeaderRowView);
};

}  // namespace

// NetworkListView:

NetworkListView::NetworkListView(DetailedViewDelegate* delegate,
                                 LoginStatus login)
    : NetworkStateListDetailedView(delegate, LIST_TYPE_NETWORK, login),
      needs_relayout_(false),
      no_wifi_networks_view_(nullptr),
      mobile_header_view_(nullptr),
      wifi_header_view_(nullptr),
      mobile_separator_view_(nullptr),
      wifi_separator_view_(nullptr),
      connection_warning_(nullptr) {}

NetworkListView::~NetworkListView() {
  network_icon::NetworkIconAnimation::GetInstance()->RemoveObserver(this);
}

void NetworkListView::UpdateNetworkList() {
  CHECK(scroll_content());

  NetworkStateHandler* handler = NetworkHandler::Get()->network_state_handler();

  NetworkStateHandler::NetworkStateList network_list;
  handler->GetVisibleNetworkList(&network_list);
  UpdateNetworks(network_list);
  UpdateNetworkIcons();
  UpdateNetworkListInternal();
}

bool NetworkListView::IsNetworkEntry(views::View* view,
                                     std::string* guid) const {
  std::map<views::View*, std::string>::const_iterator found =
      network_map_.find(view);
  if (found == network_map_.end())
    return false;
  *guid = found->second;
  return true;
}

void NetworkListView::UpdateNetworks(
    const NetworkStateHandler::NetworkStateList& networks) {
  SCOPED_NET_LOG_IF_SLOW();
  // |network_list_| contains all the info and is going to be cleared and
  // recreated. Save them to |last_network_info_map_|.
  last_network_info_map_.clear();
  for (auto& info : network_list_)
    last_network_info_map_[info->guid] = std::move(info);

  bool cellular_enabled =
      NetworkHandler::Get()->network_state_handler()->IsTechnologyEnabled(
          NetworkTypePattern::Cellular());
  network_list_.clear();
  for (const auto* network : networks) {
    if (!NetworkTypePattern::NonVirtual().MatchesType(network->type()))
      continue;
    // If cellular is disabled, skip the default cellular service.
    if (network->IsDefaultCellular() && !cellular_enabled)
      continue;
    network_list_.push_back(std::make_unique<NetworkInfo>(network->guid()));
  }
}

void NetworkListView::UpdateNetworkIcons() {
  SCOPED_NET_LOG_IF_SLOW();
  NetworkStateHandler* handler = NetworkHandler::Get()->network_state_handler();

  // First, update state for all networks.
  bool animating = false;

  for (auto& info : network_list_) {
    const chromeos::NetworkState* network =
        handler->GetNetworkStateFromGuid(info->guid);
    if (!network)
      continue;
    bool prohibited_by_policy = network->blocked_by_policy();
    info->label = network_icon::GetLabelForNetwork(
        network, network_icon::ICON_TYPE_MENU_LIST);
    info->image =
        network_icon::GetImageForNetwork(network, network_icon::ICON_TYPE_LIST);
    info->disable =
        (network->activation_state() == shill::kActivationStateActivating) ||
        prohibited_by_policy;
    info->connected = network->IsConnectedState();
    info->connecting = network->IsConnectingState();
    if (network->Matches(NetworkTypePattern::WiFi()))
      info->type = NetworkInfo::Type::WIFI;
    else if (network->Matches(NetworkTypePattern::Mobile()))
      info->type = NetworkInfo::Type::MOBILE;
    if (prohibited_by_policy) {
      info->tooltip =
          l10n_util::GetStringUTF16(IDS_ASH_STATUS_TRAY_NETWORK_PROHIBITED);
    }
    if (!animating && network->IsConnectingState())
      animating = true;
  }
  if (animating)
    network_icon::NetworkIconAnimation::GetInstance()->AddObserver(this);
  else
    network_icon::NetworkIconAnimation::GetInstance()->RemoveObserver(this);
}

void NetworkListView::UpdateNetworkListInternal() {
  SCOPED_NET_LOG_IF_SLOW();
  // Get the updated list entries.
  needs_relayout_ = false;
  network_map_.clear();
  std::unique_ptr<std::set<std::string>> new_guids = UpdateNetworkListEntries();

  // Remove old children.
  std::set<std::string> remove_guids;
  for (const auto& iter : network_guid_map_) {
    if (new_guids->find(iter.first) == new_guids->end()) {
      remove_guids.insert(iter.first);
      network_map_.erase(iter.second);
      delete iter.second;
      needs_relayout_ = true;
    }
  }

  for (const auto& remove_iter : remove_guids)
    network_guid_map_.erase(remove_iter);

  if (!needs_relayout_)
    return;

  views::View* selected_view = nullptr;
  for (const auto& iter : network_guid_map_) {
    if (iter.second->IsMouseHovered()) {
      selected_view = iter.second;
      break;
    }
  }
  scroll_content()->SizeToPreferredSize();
  scroller()->Layout();
  if (selected_view)
    scroll_content()->ScrollRectToVisible(selected_view->bounds());
}

std::unique_ptr<std::set<std::string>>
NetworkListView::UpdateNetworkListEntries() {
  NetworkStateHandler* handler = NetworkHandler::Get()->network_state_handler();

  // Keep an index where the next child should be inserted.
  int index = 0;

  // Show a warning that the connection might be monitored if connected to a VPN
  // or if the default network has a proxy installed.
  const bool using_vpn =
      !!NetworkHandler::Get()->network_state_handler()->ConnectedNetworkByType(
          NetworkTypePattern::VPN());
  bool using_proxy = false;
  // TODO(jamescook): Create UIProxyConfigService under mash. This will require
  // the mojo pref service to work with prefs in Local State.
  // http://crbug.com/718072
  if (!::features::IsMultiProcessMash()) {
    using_proxy = NetworkHandler::Get()
                      ->ui_proxy_config_service()
                      ->HasDefaultNetworkProxyConfigured();
  }
  if (using_vpn || using_proxy) {
    if (!connection_warning_)
      connection_warning_ = CreateConnectionWarning();
    PlaceViewAtIndex(connection_warning_, index++);
  }

  // First add high-priority networks (neither Wi-Fi nor Mobile).
  std::unique_ptr<std::set<std::string>> new_guids =
      UpdateNetworkChildren(NetworkInfo::Type::UNKNOWN, index);
  index += new_guids->size();

  if (ShouldMobileDataSectionBeShown()) {
    // Note: Mobile will update its own enabled state.
    index = UpdateSectionHeaderRow(
        NetworkTypePattern::Mobile(), false /* enabled */, index,
        &mobile_header_view_, &mobile_separator_view_);

    std::unique_ptr<std::set<std::string>> new_cellular_guids =
        UpdateNetworkChildren(NetworkInfo::Type::MOBILE, index);
    index += new_cellular_guids->size();
    new_guids->insert(new_cellular_guids->begin(), new_cellular_guids->end());
  }

  index = UpdateSectionHeaderRow(
      NetworkTypePattern::WiFi(),
      handler->IsTechnologyEnabled(NetworkTypePattern::WiFi()), index,
      &wifi_header_view_, &wifi_separator_view_);

  // "Wifi Enabled / Disabled".
  if (!handler->IsTechnologyEnabled(NetworkTypePattern::WiFi())) {
    UpdateInfoLabel(IDS_ASH_STATUS_TRAY_NETWORK_WIFI_DISABLED, index,
                    &no_wifi_networks_view_);
    return new_guids;
  }

  bool should_clear_info_label = true;
  if (!handler->FirstNetworkByType(NetworkTypePattern::WiFi())) {
    UpdateInfoLabel(IDS_ASH_STATUS_TRAY_NETWORK_WIFI_ENABLED, index,
                    &no_wifi_networks_view_);
    ++index;
    should_clear_info_label = false;
  }

  // Add Wi-Fi networks.
  std::unique_ptr<std::set<std::string>> new_wifi_guids =
      UpdateNetworkChildren(NetworkInfo::Type::WIFI, index);
  index += new_wifi_guids->size();
  new_guids->insert(new_wifi_guids->begin(), new_wifi_guids->end());

  // No networks or other messages (fallback).
  if (index == 0) {
    UpdateInfoLabel(IDS_ASH_STATUS_TRAY_NO_NETWORKS, index,
                    &no_wifi_networks_view_);
  } else if (should_clear_info_label) {
    // Update the label to show nothing.
    UpdateInfoLabel(0, index, &no_wifi_networks_view_);
  }

  return new_guids;
}

bool NetworkListView::ShouldMobileDataSectionBeShown() {
  NetworkStateHandler* handler = NetworkHandler::Get()->network_state_handler();

  // The section should always be shown if Cellular networks are available.
  if (handler->IsTechnologyAvailable(NetworkTypePattern::Cellular()))
    return true;

  // Hide the section if both Cellular and Tether are UNAVAILABLE.
  if (!handler->IsTechnologyAvailable(NetworkTypePattern::Tether()))
    return false;

  // Hide the section if Tether is PROHIBITED.
  if (handler->IsTechnologyProhibited(NetworkTypePattern::Tether()))
    return false;

  // Secondary users cannot enable Bluetooth, and Tether is only UNINITIALIZED
  // if Bluetooth is disabled. Hide the section in this case.
  if (handler->IsTechnologyUninitialized(NetworkTypePattern::Tether()) &&
      IsSecondaryUser()) {
    return false;
  }

  return true;
}

void NetworkListView::UpdateViewForNetwork(HoverHighlightView* view,
                                           const NetworkInfo& info) {
  view->Reset();
  gfx::ImageSkia network_image;
  if (info.type == NetworkInfo::Type::MOBILE &&
      (!info.connected && !info.connecting)) {
    // Mobile icons which are not connecting or connected should display a small
    // "X" icon superimposed so that it is clear that they are disconnected.
    network_image = gfx::ImageSkiaOperations::CreateSuperimposedImage(
        info.image, gfx::CreateVectorIcon(kNetworkMobileNotConnectedXIcon,
                                          info.image.height(),
                                          kMobileNotConnectedXIconColor));
  } else {
    network_image = info.image;
  }
  view->AddIconAndLabel(network_image, info.label);
  if (info.connected)
    SetupConnectedScrollListItem(view);
  else if (info.connecting)
    SetupConnectingScrollListItem(view);
  view->SetTooltipText(info.tooltip);

  // Add an additional icon to the right of the label for networks
  // that require it (e.g. Tether, controlled by extension).
  views::View* icon = CreatePowerStatusView(info);
  if (icon) {
    view->AddRightView(icon, views::CreateEmptyBorder(gfx::Insets(
                                 0 /* top */, 0 /* left */, 0 /* bottom */,
                                 kPowerStatusPaddingRight)));
  } else {
    icon = CreatePolicyView(info);
    if (!icon)
      icon = CreateControlledByExtensionView(info);
    if (icon)
      view->AddRightView(icon);
  }

  needs_relayout_ = true;
}

views::View* NetworkListView::CreatePowerStatusView(const NetworkInfo& info) {
  // Mobile can be Cellular or Tether.
  if (info.type != NetworkInfo::Type::MOBILE)
    return nullptr;

  const chromeos::NetworkState* network =
      NetworkHandler::Get()->network_state_handler()->GetNetworkStateFromGuid(
          info.guid);

  // Only return a battery icon for Tether network type.
  if (!NetworkTypePattern::Tether().MatchesType(network->type()))
    return nullptr;

  views::ImageView* icon = TrayPopupUtils::CreateMoreImageView();
  PowerStatus::BatteryImageInfo icon_info;
  icon_info.charge_percent = network->battery_percentage();
  icon->SetImage(
      PowerStatus::GetBatteryImage(icon_info, kMobileNetworkBatteryIconSize,
                                   kMenuIconColorDisabled, kMenuIconColor));

  // Show the numeric battery percentage on hover.
  icon->set_tooltip_text(base::FormatPercent(network->battery_percentage()));

  return icon;
}

views::View* NetworkListView::CreatePolicyView(const NetworkInfo& info) {
  // Check if the network is managed by policy.
  const chromeos::NetworkState* network =
      NetworkHandler::Get()->network_state_handler()->GetNetworkStateFromGuid(
          info.guid);
  if (!network || !network->IsManagedByPolicy())
    return nullptr;

  views::ImageView* controlled_icon = TrayPopupUtils::CreateMainImageView();
  controlled_icon->SetImage(
      gfx::CreateVectorIcon(kSystemMenuBusinessIcon, kMenuIconColor));
  return controlled_icon;
}

views::View* NetworkListView::CreateControlledByExtensionView(
    const NetworkInfo& info) {
  const chromeos::NetworkState* network =
      NetworkHandler::Get()->network_state_handler()->GetNetworkStateFromGuid(
          info.guid);
  if (!network || !network->captive_portal_provider())
    return nullptr;

  views::ImageView* controlled_icon = TrayPopupUtils::CreateMainImageView();
  controlled_icon->SetImage(
      gfx::CreateVectorIcon(kCaptivePortalIcon, kMenuIconColor));
  controlled_icon->set_tooltip_text(l10n_util::GetStringFUTF16(
      IDS_ASH_STATUS_TRAY_EXTENSION_CONTROLLED_WIFI,
      base::UTF8ToUTF16(network->captive_portal_provider()->name)));
  controlled_icon->set_id(VIEW_ID_EXTENSION_CONTROLLED_WIFI);
  return controlled_icon;
}

std::unique_ptr<std::set<std::string>> NetworkListView::UpdateNetworkChildren(
    NetworkInfo::Type type,
    int index) {
  std::unique_ptr<std::set<std::string>> new_guids(new std::set<std::string>);
  for (const auto& info : network_list_) {
    if (info->type != type)
      continue;
    UpdateNetworkChild(index++, info.get());
    new_guids->insert(info->guid);
  }
  return new_guids;
}

void NetworkListView::UpdateNetworkChild(int index, const NetworkInfo* info) {
  HoverHighlightView* network_view = nullptr;
  NetworkGuidMap::const_iterator found = network_guid_map_.find(info->guid);
  if (found == network_guid_map_.end()) {
    network_view = new HoverHighlightView(this);
    UpdateViewForNetwork(network_view, *info);
  } else {
    network_view = found->second;
    if (NeedUpdateViewForNetwork(*info))
      UpdateViewForNetwork(network_view, *info);
  }
  PlaceViewAtIndex(network_view, index);
  if (info->disable)
    network_view->SetEnabled(false);
  network_map_[network_view] = info->guid;
  network_guid_map_[info->guid] = network_view;
}

void NetworkListView::PlaceViewAtIndex(views::View* view, int index) {
  if (view->parent() != scroll_content()) {
    scroll_content()->AddChildViewAt(view, index);
  } else {
    // No re-order and re-layout is necessary if |view| is already at |index|.
    if (index < scroll_content()->child_count() &&
        scroll_content()->child_at(index) == view) {
      return;
    }
    scroll_content()->ReorderChildView(view, index);
  }
  needs_relayout_ = true;
}

void NetworkListView::UpdateInfoLabel(int message_id,
                                      int insertion_index,
                                      TrayInfoLabel** info_label_ptr) {
  TrayInfoLabel* info_label = *info_label_ptr;
  if (!message_id) {
    if (info_label) {
      needs_relayout_ = true;
      delete info_label;
      *info_label_ptr = nullptr;
    }
    return;
  }
  if (!info_label)
    info_label = new TrayInfoLabel(nullptr /* delegate */, message_id);
  else
    info_label->Update(message_id);

  PlaceViewAtIndex(info_label, insertion_index);
  *info_label_ptr = info_label;
}

int NetworkListView::UpdateSectionHeaderRow(NetworkTypePattern pattern,
                                            bool enabled,
                                            int child_index,
                                            SectionHeaderRowView** view,
                                            views::Separator** separator_view) {
  if (!*view) {
    if (pattern.MatchesPattern(NetworkTypePattern::Mobile()))
      *view = new MobileHeaderRowView(
          NetworkHandler::Get()->network_state_handler());
    else if (pattern.Equals(NetworkTypePattern::WiFi()))
      *view = new WifiHeaderRowView();
    else
      NOTREACHED();
    (*view)->Init(enabled);
  }
  // Show or hide a separator above the header. The separator should only be
  // visible when the header row is not at the top of the list.
  if (child_index > 0) {
    if (!*separator_view)
      *separator_view = CreateListSubHeaderSeparator();
    PlaceViewAtIndex(*separator_view, child_index++);
  } else {
    if (*separator_view)
      delete *separator_view;
    *separator_view = nullptr;
  }

  bool default_toggle_enabled = !IsSecondaryUser();
  // Mobile updates its toggle state independently.
  if (!pattern.MatchesPattern(NetworkTypePattern::Mobile()))
    (*view)->SetToggleState(default_toggle_enabled, enabled /* is_on */);
  PlaceViewAtIndex(*view, child_index++);
  return child_index;
}

void NetworkListView::NetworkIconChanged() {
  Update();
}

bool NetworkListView::NeedUpdateViewForNetwork(const NetworkInfo& info) const {
  NetworkInfoMap::const_iterator found = last_network_info_map_.find(info.guid);
  if (found == last_network_info_map_.end()) {
    // If we cannot find |info| in |last_network_info_map_|, just return true
    // since this is a new network so we have nothing to compare.
    return true;
  } else {
    return *found->second != info;
  }
}

TriView* NetworkListView::CreateConnectionWarning() {
  // Set up layout and apply sticky row property.
  TriView* connection_warning = TrayPopupUtils::CreateDefaultRowView();
  TrayPopupUtils::ConfigureAsStickyHeader(connection_warning);

  // Set 'info' icon on left side.
  views::ImageView* image_view = TrayPopupUtils::CreateMainImageView();
  image_view->SetImage(
      gfx::CreateVectorIcon(kSystemMenuInfoIcon, kMenuIconColor));
  image_view->SetBackground(views::CreateSolidBackground(SK_ColorTRANSPARENT));
  connection_warning->AddView(TriView::Container::START, image_view);

  // Set message label in middle of row.
  views::Label* label = TrayPopupUtils::CreateDefaultLabel();
  label->SetText(
      l10n_util::GetStringUTF16(IDS_ASH_STATUS_TRAY_NETWORK_MONITORED_WARNING));
  label->SetBackground(views::CreateSolidBackground(SK_ColorTRANSPARENT));
  TrayPopupItemStyle style(TrayPopupItemStyle::FontStyle::DETAILED_VIEW_LABEL);
  style.SetupLabel(label);
  connection_warning->AddView(TriView::Container::CENTER, label);
  connection_warning->SetContainerBorder(
      TriView::Container::CENTER, views::CreateEmptyBorder(gfx::Insets(
                                      0, 0, 0, kTrayPopupLabelRightPadding)));

  // Nothing to the right of the text.
  connection_warning->SetContainerVisible(TriView::Container::END, false);
  return connection_warning;
}

}  // namespace tray
}  // namespace ash
