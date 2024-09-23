// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/network/network_state_list_detailed_view.h"

#include <algorithm>

#include "ash/public/cpp/system_tray_client.h"
#include "ash/session/session_controller_impl.h"
#include "ash/shell.h"
#include "ash/strings/grit/ash_strings.h"
#include "ash/system/model/system_tray_model.h"
#include "ash/system/network/network_utils.h"
#include "ash/system/network/tray_network_state_model.h"
#include "ash/system/tray/system_menu_button.h"
#include "ash/system/tray/tri_view.h"
#include "base/functional/bind.h"
#include "base/memory/raw_ptr.h"
#include "base/metrics/user_metrics.h"
#include "base/strings/utf_string_conversions.h"
#include "base/time/time.h"
#include "chromeos/ash/components/network/network_connect.h"
#include "chromeos/services/network_config/public/mojom/cros_network_config.mojom.h"
#include "net/base/ip_address.h"
#include "third_party/cros_system_api/dbus/service_constants.h"
#include "ui/accessibility/ax_enums.mojom.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/base/mojom/dialog_button.mojom.h"
#include "ui/views/bubble/bubble_dialog_delegate_view.h"
#include "ui/views/controls/button/button.h"
#include "ui/views/controls/label.h"
#include "ui/views/layout/fill_layout.h"
#include "ui/views/layout/layout_manager.h"
#include "ui/views/widget/widget.h"

namespace ash {
namespace {

using base::UserMetricsAction;
using chromeos::network_config::mojom::ActivationStateType;
using chromeos::network_config::mojom::ConnectionStateType;
using chromeos::network_config::mojom::DeviceStateProperties;
using chromeos::network_config::mojom::DeviceStateType;
using chromeos::network_config::mojom::NetworkStateProperties;
using chromeos::network_config::mojom::NetworkStatePropertiesPtr;
using chromeos::network_config::mojom::NetworkType;

// Delay between scan requests.
constexpr int kRequestScanDelaySeconds = 10;

// 00:00:00:00:00:00 is provided when a device MAC address cannot be retrieved.
constexpr char kMissingMacAddress[] = "00:00:00:00:00:00";

// This margin value is used throughout the bubble:
// - margins inside the border
// - horizontal spacing between bubble border and parent bubble border
// - distance between top of this bubble's border and the bottom of the anchor
//   view (horizontal rule).
constexpr int kBubbleMargin = 8;

// Elevation used for the bubble shadow effect (tiny).
constexpr int kBubbleShadowElevation = 2;

bool IsSecondaryUser() {
  SessionControllerImpl* session_controller =
      Shell::Get()->session_controller();
  return session_controller->IsActiveUserSessionStarted() &&
         !session_controller->IsUserPrimary();
}

bool NetworkTypeIsConfigurable(NetworkType type) {
  switch (type) {
    case NetworkType::kVPN:
    case NetworkType::kWiFi:
      return true;
    case NetworkType::kAll:
    case NetworkType::kCellular:
    case NetworkType::kEthernet:
    case NetworkType::kMobile:
    case NetworkType::kTether:
    case NetworkType::kWireless:
      return false;
  }
  NOTREACHED();
}

}  // namespace

bool CanNetworkConnect(
    chromeos::network_config::mojom::ConnectionStateType connection_state,
    chromeos::network_config::mojom::NetworkType type,
    chromeos::network_config::mojom::ActivationStateType activation_state,
    bool connectable,
    std::string sim_eid) {
  // Network can be connected to if the network is not connected and:
  // * The network is connectable or
  // * The active user is primary and the network is configurable or
  // * The network is cellular and activated
  if (connection_state != ConnectionStateType::kNotConnected) {
    return false;
  }

  // Network cannot be connected to if it is an unactivated eSIM network.
  if (type == NetworkType::kCellular &&
      activation_state == ActivationStateType::kNotActivated &&
      !sim_eid.empty()) {
    return false;
  }

  if (connectable) {
    return true;
  }
  if (!IsSecondaryUser() && NetworkTypeIsConfigurable(type)) {
    return true;
  }
  if (type == NetworkType::kCellular &&
      activation_state == ActivationStateType::kActivated) {
    return true;
  }
  return false;
}

// A bubble which displays network info.
class NetworkStateListDetailedView::InfoBubble
    : public views::BubbleDialogDelegateView {
 public:
  InfoBubble(views::View* anchor,
             views::View* content,
             NetworkStateListDetailedView* detailed_view)
      : views::BubbleDialogDelegateView(anchor, views::BubbleBorder::TOP_RIGHT),
        detailed_view_(detailed_view) {
    SetButtons(static_cast<int>(ui::mojom::DialogButton::kNone));
    set_margins(gfx::Insets(kBubbleMargin));
    SetArrow(views::BubbleBorder::NONE);
    set_shadow(views::BubbleBorder::NO_SHADOW);
    SetNotifyEnterExitOnChild(true);
    SetLayoutManager(std::make_unique<views::FillLayout>());
    AddChildView(content);
  }

  InfoBubble(const InfoBubble&) = delete;
  InfoBubble& operator=(const InfoBubble&) = delete;

  ~InfoBubble() override {
    // The detailed view can be destructed before info bubble is destructed.
    // Call OnInfoBubbleDestroyed only if the detailed view is live.
    if (detailed_view_) {
      detailed_view_->OnInfoBubbleDestroyed();
    }
  }

  void OnNetworkStateListDetailedViewIsDeleting() { detailed_view_ = nullptr; }

 private:
  // View:
  gfx::Size CalculatePreferredSize(
      const views::SizeBounds& available_size) const override {
    // This bubble should be inset by kBubbleMargin on both left and right
    // relative to the parent bubble.
    const gfx::Size anchor_size = GetAnchorView()->size();
    int contents_width =
        anchor_size.width() - 2 * kBubbleMargin - margins().width();
    return gfx::Size(
        contents_width,
        GetLayoutManager()->GetPreferredHeightForWidth(this, contents_width));
  }

  void OnMouseExited(const ui::MouseEvent& event) override {
    // Like the user switching bubble/menu, hide the bubble when the mouse
    // exits.
    if (detailed_view_) {
      detailed_view_->ResetInfoBubble();
    }
  }

  void OnBeforeBubbleWidgetInit(views::Widget::InitParams* params,
                                views::Widget* widget) const override {
    params->shadow_type = views::Widget::InitParams::ShadowType::kDrop;
    params->shadow_elevation = kBubbleShadowElevation;
    params->name = "NetworkStateListDetailedView::InfoBubble";
  }

  // Not owned.
  raw_ptr<NetworkStateListDetailedView> detailed_view_;
};

//------------------------------------------------------------------------------
// NetworkStateListDetailedView

NetworkStateListDetailedView::NetworkStateListDetailedView(
    DetailedViewDelegate* delegate,
    NetworkDetailedViewListType list_type,
    LoginStatus login)
    : TrayDetailedView(delegate),
      list_type_(list_type),
      login_(login),
      model_(Shell::Get()->system_tray_model()->network_state_model()),
      info_button_(nullptr),
      settings_button_(nullptr),
      info_bubble_(nullptr) {
  OverrideProgressBarAccessibleName(l10n_util::GetStringUTF16(
      IDS_ASH_STATUS_TRAY_NETWORK_PROGRESS_ACCESSIBLE_NAME));
}

NetworkStateListDetailedView::~NetworkStateListDetailedView() {
  model_->RemoveObserver(this);
  if (info_bubble_) {
    info_bubble_->OnNetworkStateListDetailedViewIsDeleting();
  }
  ResetInfoBubble();
}

void NetworkStateListDetailedView::ToggleInfoBubbleForTesting() {
  ToggleInfoBubble();
}

void NetworkStateListDetailedView::Init() {
  CreateScrollableList();
  CreateTitleRow(GetStringIdForNetworkDetailedViewTitleRow(list_type_));

  model_->AddObserver(this);
  Update();

  if (list_type_ == LIST_TYPE_NETWORK && IsWifiEnabled()) {
    ScanAndStartTimer();
  }
}

void NetworkStateListDetailedView::Update() {
  UpdateNetworkList();
  UpdateHeaderButtons();
  UpdateScanningBar();
  DeprecatedLayoutImmediately();
}

void NetworkStateListDetailedView::ActiveNetworkStateChanged() {
  Update();
}

void NetworkStateListDetailedView::NetworkListChanged() {
  Update();
}

void NetworkStateListDetailedView::HandleViewClicked(views::View* view) {
  if (login_ == LoginStatus::LOCKED) {
    return;
  }

  std::string guid;
  if (!IsNetworkEntry(view, &guid)) {
    return;
  }

  model_->cros_network_config()->GetNetworkState(
      guid, base::BindOnce(&NetworkStateListDetailedView::HandleViewClickedImpl,
                           weak_ptr_factory_.GetWeakPtr()));
}

void NetworkStateListDetailedView::HandleViewClickedImpl(
    NetworkStatePropertiesPtr network) {
  if (network) {
    // If the network is locked and is cellular show SIM unlock dialog in OS
    // Settings.
    if (network->type == NetworkType::kCellular &&
        network->type_state->get_cellular()->sim_locked) {
      if (!Shell::Get()->session_controller()->ShouldEnableSettings()) {
        return;
      }
      Shell::Get()->system_tray_model()->client()->ShowSettingsSimUnlock();
      return;
    }

    if (CanNetworkConnect(
            network->connection_state, network->type,
            network->type == NetworkType::kCellular
                ? network->type_state->get_cellular()->activation_state
                : ActivationStateType::kUnknown,
            network->connectable,
            network->type == NetworkType::kCellular
                ? network->type_state->get_cellular()->eid
                : "")) {
      base::RecordAction(
          list_type_ == LIST_TYPE_VPN
              ? UserMetricsAction("StatusArea_VPN_ConnectToNetwork")
              : UserMetricsAction("StatusArea_Network_ConnectConfigured"));
      NetworkConnect::Get()->ConnectToNetworkId(network->guid);
      return;
    }
  }
  // If the network is no longer available or not connectable or configurable,
  // show the Settings UI.
  base::RecordAction(
      list_type_ == LIST_TYPE_VPN
          ? UserMetricsAction("StatusArea_VPN_ConnectionDetails")
          : UserMetricsAction("StatusArea_Network_ConnectionDetails"));
  Shell::Get()->system_tray_model()->client()->ShowNetworkSettings(
      network ? network->guid : std::string());
}

void NetworkStateListDetailedView::CreateExtraTitleRowButtons() {
  if (login_ == LoginStatus::LOCKED) {
    return;
  }

  DCHECK(!info_button_);
  tri_view()->SetContainerVisible(TriView::Container::END, true);

  info_button_ = CreateInfoButton(
      base::BindRepeating(&NetworkStateListDetailedView::ToggleInfoBubble,
                          base::Unretained(this)),
      IDS_ASH_STATUS_TRAY_NETWORK_INFO);
  tri_view()->AddView(TriView::Container::END, info_button_);

  DCHECK(!settings_button_);
  settings_button_ = CreateSettingsButton(
      base::BindRepeating(&NetworkStateListDetailedView::ShowSettings,
                          base::Unretained(this)),
      IDS_ASH_STATUS_TRAY_NETWORK_SETTINGS);
  tri_view()->AddView(TriView::Container::END, settings_button_);
}

void NetworkStateListDetailedView::ShowSettings() {
  base::RecordAction(list_type_ == LIST_TYPE_VPN
                         ? UserMetricsAction("StatusArea_VPN_Settings")
                         : UserMetricsAction("StatusArea_Network_Settings"));
  const std::string guid = model_->default_network()
                               ? model_->default_network()->guid
                               : std::string();

  base::RecordAction(base::UserMetricsAction(
      "ChromeOS.SystemTray.Network.SettingsButtonPressed"));

  // Showing network settings window may close the bubble (and destroy this
  // view). Explicitly request bubble closure here, before showing network
  // settings.
  CloseBubble();  // Deletes |this|.

  SystemTrayClient* system_tray_client =
      Shell::Get()->system_tray_model()->client();
  if (system_tray_client) {
    system_tray_client->ShowNetworkSettings(guid);
  }
}

void NetworkStateListDetailedView::UpdateHeaderButtons() {
  if (settings_button_) {
    if (login_ == LoginStatus::NOT_LOGGED_IN) {
      // When not logged in, only enable the settings button if there is a
      // default (i.e. connected or connecting) network to show settings for.
      settings_button_->SetEnabled(model_->default_network());
    } else {
      // Otherwise, enable if showing settings is allowed. There are situations
      // (supervised user creation flow) when the session is started but UI flow
      // continues within login UI, i.e., no browser window is yet available.
      settings_button_->SetEnabled(
          Shell::Get()->session_controller()->ShouldEnableSettings());
    }
  }
}

void NetworkStateListDetailedView::UpdateScanningBar() {
  if (list_type_ != LIST_TYPE_NETWORK) {
    return;
  }

  bool is_wifi_enabled = IsWifiEnabled();
  if (is_wifi_enabled && !network_scan_repeating_timer_.IsRunning()) {
    ScanAndStartTimer();
  }

  if (!is_wifi_enabled && network_scan_repeating_timer_.IsRunning()) {
    network_scan_repeating_timer_.Stop();
  }

  bool scanning_bar_visible = false;
  if (is_wifi_enabled) {
    const DeviceStateProperties* wifi = model_->GetDevice(NetworkType::kWiFi);
    const DeviceStateProperties* tether =
        model_->GetDevice(NetworkType::kTether);
    scanning_bar_visible =
        (wifi && wifi->scanning) || (tether && tether->scanning);
  }
  ShowProgress(-1, scanning_bar_visible);
}

void NetworkStateListDetailedView::ToggleInfoBubble() {
  if (ResetInfoBubble()) {
    return;
  }

  info_bubble_ = new InfoBubble(tri_view(), CreateNetworkInfoView(), this);
  views::BubbleDialogDelegateView::CreateBubble(info_bubble_)->Show();
  info_bubble_->NotifyAccessibilityEvent(ax::mojom::Event::kAlert, false);
}

bool NetworkStateListDetailedView::ResetInfoBubble() {
  if (!info_bubble_) {
    return false;
  }

  info_bubble_->GetWidget()->Close();
  return true;
}

void NetworkStateListDetailedView::OnInfoBubbleDestroyed() {
  info_bubble_ = nullptr;

  // Widget of info bubble is activated while info bubble is shown. To move
  // focus back to the widget of this view, activate it again here.
  GetWidget()->Activate();
}

views::View* NetworkStateListDetailedView::CreateNetworkInfoView() {
  std::string ipv4_address, ipv6_address;
  const NetworkStateProperties* network = model_->default_network();
  const DeviceStateProperties* device =
      network ? model_->GetDevice(network->type) : nullptr;
  if (device) {
    if (device->ipv4_address) {
      ipv4_address = device->ipv4_address->ToString();
    }
    if (device->ipv6_address) {
      ipv6_address = device->ipv6_address->ToString();
    }
  }

  std::string ethernet_address, wifi_address, cellular_address;
  if (list_type_ == LIST_TYPE_NETWORK) {
    const DeviceStateProperties* ethernet =
        model_->GetDevice(NetworkType::kEthernet);
    if (ethernet && ethernet->mac_address) {
      ethernet_address = *ethernet->mac_address;
    }
    const DeviceStateProperties* wifi = model_->GetDevice(NetworkType::kWiFi);
    if (wifi && wifi->mac_address) {
      wifi_address = *wifi->mac_address;
    }
    const DeviceStateProperties* cellular =
        model_->GetDevice(NetworkType::kCellular);
    if (cellular && cellular->mac_address) {
      cellular_address = *cellular->mac_address;
    }
  }

  std::u16string bubble_text;
  auto maybe_add_mac_address = [&bubble_text](const std::string& address,
                                              int ids) {
    if (address.empty() || address == kMissingMacAddress) {
      return;
    }

    if (!bubble_text.empty()) {
      bubble_text += u"\n";
    }

    bubble_text += l10n_util::GetStringFUTF16(ids, base::UTF8ToUTF16(address));
  };

  maybe_add_mac_address(ipv4_address, IDS_ASH_STATUS_TRAY_IP_ADDRESS);
  maybe_add_mac_address(ipv6_address, IDS_ASH_STATUS_TRAY_IPV6_ADDRESS);
  maybe_add_mac_address(ethernet_address, IDS_ASH_STATUS_TRAY_ETHERNET_ADDRESS);
  maybe_add_mac_address(wifi_address, IDS_ASH_STATUS_TRAY_WIFI_ADDRESS);
  maybe_add_mac_address(cellular_address, IDS_ASH_STATUS_TRAY_CELLULAR_ADDRESS);

  // Avoid an empty bubble in the unlikely event that there is no network
  // information at all.
  if (bubble_text.empty()) {
    bubble_text = l10n_util::GetStringUTF16(IDS_ASH_STATUS_TRAY_NO_NETWORKS);
  }

  auto* label = new views::Label(bubble_text);
  label->SetMultiLine(true);
  label->SetHorizontalAlignment(gfx::ALIGN_TO_HEAD);
  label->SetSelectable(true);
  return label;
}

void NetworkStateListDetailedView::ScanAndStartTimer() {
  CallRequestScan();
  network_scan_repeating_timer_.Start(
      FROM_HERE, base::Seconds(kRequestScanDelaySeconds), this,
      &NetworkStateListDetailedView::CallRequestScan);
}

void NetworkStateListDetailedView::CallRequestScan() {
  if (!IsWifiEnabled()) {
    return;
  }

  VLOG(1) << "Requesting Network Scan.";
  model_->cros_network_config()->RequestNetworkScan(NetworkType::kWiFi);
  model_->cros_network_config()->RequestNetworkScan(NetworkType::kTether);
}

bool NetworkStateListDetailedView::IsWifiEnabled() {
  return model_->GetDeviceState(NetworkType::kWiFi) ==
         DeviceStateType::kEnabled;
}

BEGIN_METADATA(NetworkStateListDetailedView)
END_METADATA

}  // namespace ash
