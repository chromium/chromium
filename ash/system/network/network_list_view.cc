// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/network/network_list_view.h"

#include <memory>
#include <utility>

#include "ash/public/cpp/ash_view_ids.h"
#include "ash/resources/vector_icons/vector_icons.h"
#include "ash/session/session_controller_impl.h"
#include "ash/shell.h"
#include "ash/strings/grit/ash_strings.h"
#include "ash/style/ash_color_provider.h"
#include "ash/style/color_util.h"
#include "ash/system/model/system_tray_model.h"
#include "ash/system/network/network_icon.h"
#include "ash/system/network/network_icon_animation.h"
#include "ash/system/network/network_info.h"
#include "ash/system/network/network_section_header_view.h"
#include "ash/system/network/network_state_list_detailed_view.h"
#include "ash/system/network/network_utils.h"
#include "ash/system/network/tray_network_state_model.h"
#include "ash/system/power/power_status.h"
#include "ash/system/tray/hover_highlight_view.h"
#include "ash/system/tray/tray_info_label.h"
#include "ash/system/tray/tray_popup_utils.h"
#include "ash/system/tray/tray_utils.h"
#include "ash/system/tray/tri_view.h"
#include "base/bind.h"
#include "base/i18n/number_formatting.h"
#include "base/strings/utf_string_conversions.h"
#include "chromeos/services/network_config/public/cpp/cros_network_config_util.h"
#include "chromeos/services/network_config/public/mojom/cros_network_config.mojom.h"
#include "components/device_event_log/device_event_log.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/ui_base_features.h"
#include "ui/gfx/image/image_skia_operations.h"
#include "ui/gfx/paint_vector_icon.h"
#include "ui/views/accessibility/view_accessibility.h"
#include "ui/views/background.h"
#include "ui/views/border.h"
#include "ui/views/controls/image_view.h"
#include "ui/views/controls/scroll_view.h"
#include "ui/views/controls/separator.h"

using chromeos::network_config::IsInhibited;
using chromeos::network_config::NetworkTypeMatchesType;
using chromeos::network_config::StateIsConnected;

using chromeos::network_config::mojom::ActivationStateType;
using chromeos::network_config::mojom::ConnectionStateType;
using chromeos::network_config::mojom::DeviceStatePropertiesPtr;
using chromeos::network_config::mojom::DeviceStateType;
using chromeos::network_config::mojom::FilterType;
using chromeos::network_config::mojom::NetworkFilter;
using chromeos::network_config::mojom::NetworkStateProperties;
using chromeos::network_config::mojom::NetworkStatePropertiesPtr;
using chromeos::network_config::mojom::NetworkType;
using chromeos::network_config::mojom::OncSource;
using chromeos::network_config::mojom::ProxyMode;

namespace ash {
namespace {

const int kMobileNetworkBatteryIconSize = 20;
const int kPowerStatusPaddingRight = 10;
const double kAlphaValueForInhibitedIconOpacity = 0.3;

bool IsSecondaryUser() {
  SessionControllerImpl* session_controller =
      Shell::Get()->session_controller();
  return session_controller->IsActiveUserSessionStarted() &&
         !session_controller->IsUserPrimary();
}

SkColor GetIconColor() {
  return AshColorProvider::Get()->GetContentLayerColor(
      AshColorProvider::ContentLayerType::kIconColorPrimary);
}

bool IsManagedByPolicy(const NetworkInfo& info) {
  return info.source == OncSource::kDevicePolicy ||
         info.source == OncSource::kUserPolicy;
}

bool ShouldShowActivateCellularNetwork(const NetworkInfo& info) {
  return NetworkTypeMatchesType(info.type, NetworkType::kCellular) &&
         info.activation_state == ActivationStateType::kNotActivated &&
         info.sim_eid.empty();
}

bool ShouldShowContactCarrier(const NetworkInfo& info) {
  return NetworkTypeMatchesType(info.type, NetworkType::kCellular) &&
         info.activation_state == ActivationStateType::kNotActivated &&
         !info.sim_eid.empty();
}

gfx::ImageSkia GetNetworkImageForNetwork(const NetworkInfo& info) {
  gfx::ImageSkia network_image;
  if (NetworkTypeMatchesType(info.type, NetworkType::kMobile) &&
      info.connection_state == ConnectionStateType::kNotConnected) {
    // Mobile icons which are not connecting or connected should display a small
    // "X" icon superimposed so that it is clear that they are disconnected.
    network_image = gfx::ImageSkiaOperations::CreateSuperimposedImage(
        info.image, gfx::CreateVectorIcon(kNetworkMobileNotConnectedXIcon,
                                          info.image.height(), GetIconColor()));
  } else {
    network_image = info.image;
  }

  // When the network is disabled, its appearance should be grayed out to
  // indicate users that these networks are unavailable. We must change the
  // image before we add it to the view, and then alter the label and sub-label
  // if they exist after it is added to the view.
  if (info.disable) {
    network_image = gfx::ImageSkiaOperations::CreateTransparentImage(
        network_image, kAlphaValueForInhibitedIconOpacity);
  }
  return network_image;
}

bool ShouldShowUnlockCellularNetwork(const NetworkInfo& info) {
  return NetworkTypeMatchesType(info.type, NetworkType::kCellular) &&
         info.sim_locked;
}

// returns 0 if there is no cellular subtext
int GetCellularNetworkSubText(const NetworkInfo& info) {
  if (ShouldShowActivateCellularNetwork(info))
    return IDS_ASH_STATUS_TRAY_NETWORK_STATUS_CLICK_TO_ACTIVATE;
  if (ShouldShowContactCarrier(info))
    return IDS_ASH_STATUS_TRAY_NETWORK_UNAVAILABLE_SIM_NETWORK;
  if (!ShouldShowUnlockCellularNetwork(info))
    return 0;
  if (Shell::Get()->session_controller()->IsActiveUserSessionStarted())
    return IDS_ASH_STATUS_TRAY_NETWORK_STATUS_CLICK_TO_UNLOCK;
  return IDS_ASH_STATUS_TRAY_NETWORK_STATUS_SIGN_IN_TO_UNLOCK;
}

// Returns color for cellular network item text label.
SkColor GetCellularNetworkPrimaryTextColor(const NetworkInfo& info) {
  return AshColorProvider::Get()->GetContentLayerColor(
      AshColorProvider::ContentLayerType::kTextColorPrimary);
}

// Returns color for cellular network item sub text label.
SkColor GetCellularNetworkSubTextColor(const NetworkInfo& info) {
  return AshColorProvider::Get()->GetContentLayerColor(
      AshColorProvider::ContentLayerType::kTextColorWarning);
}

// Updates the disabled network item's label text colors to grey if the item
// is disabled.
void UpdateDisabledListItemTextColor(HoverHighlightView* view,
                                     const NetworkInfo& info) {
  // The network row is disabled if blocked by policy, device is inhibited or
  // when SIM is locked and user is not logged in.
  if (!info.disable) {
    return;
  }

  if (view->text_label()) {
    SkColor primary_text_color = view->text_label()->GetEnabledColor();
    view->text_label()->SetEnabledColor(
        ColorUtil::GetDisabledColor(primary_text_color));
  }
  if (view->sub_text_label()) {
    SkColor sub_text_color = view->sub_text_label()->GetEnabledColor();
    view->sub_text_label()->SetEnabledColor(
        ColorUtil::GetDisabledColor(sub_text_color));
  }
}

void SetupCellularListItemWithSubtext(HoverHighlightView* view,
                                      const NetworkInfo& info,
                                      int cellular_subtext_message_id) {
  if (view->text_label()) {
    view->text_label()->SetEnabledColor(
        GetCellularNetworkPrimaryTextColor(info));
  }
  view->SetSubText(l10n_util::GetStringUTF16(cellular_subtext_message_id));
  view->sub_text_label()->SetEnabledColor(GetCellularNetworkSubTextColor(info));
}

bool ComputeNetworkDisabledProperty(const NetworkStatePropertiesPtr& network,
                                    const NetworkInfo& info,
                                    ActivationStateType activation_state,
                                    bool inhibited) {
  // If user is not logged in and SIM is locked disable the row.
  if (!Shell::Get()->session_controller()->IsActiveUserSessionStarted() &&
      ShouldShowUnlockCellularNetwork(info)) {
    return info.sim_locked;
  }
  // If the device is inhibited or network is blocked by policy, the network
  // row should be disabled.
  return activation_state == ActivationStateType::kActivating ||
         network->prohibited_by_policy || inhibited;
}

}  // namespace

// NetworkListView:

NetworkListView::NetworkListView(DetailedViewDelegate* delegate,
                                 LoginStatus login)
    : NetworkStateListDetailedView(delegate, LIST_TYPE_NETWORK, login) {}

NetworkListView::~NetworkListView() {
  network_icon::NetworkIconAnimation::GetInstance()->RemoveObserver(this);
}

void NetworkListView::UpdateNetworkList() {
  CHECK(scroll_content());
  model()->cros_network_config()->GetNetworkStateList(
      NetworkFilter::New(FilterType::kVisible, NetworkType::kAll,
                         chromeos::network_config::mojom::kNoLimit),
      base::BindOnce(&NetworkListView::OnGetNetworkStateList,
                     weak_ptr_factory_.GetWeakPtr()));
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

const char* NetworkListView::GetClassName() const {
  return "NetworkListView";
}

void NetworkListView::OnGetNetworkStateList(
    std::vector<NetworkStatePropertiesPtr> networks) {
  // |network_list_| contains all the info and is going to be cleared and
  // recreated. Save them to |last_network_info_map_|.
  last_network_info_map_.clear();
  for (auto& info : network_list_)
    last_network_info_map_[info->guid] = std::move(info);

  bool animating = false;
  network_list_.clear();
  vpn_connected_ = false;
  wifi_has_networks_ = false;
  mobile_has_networks_ = false;
  tether_has_networks_ = false;
  for (auto& network : networks) {
    ConnectionStateType connection_state = network->connection_state;
    if (network->type == NetworkType::kVPN) {
      if (chromeos::network_config::StateIsConnected(connection_state))
        vpn_connected_ = true;
      continue;
    }

    auto info = std::make_unique<NetworkInfo>(network->guid);
    bool inhibited = false;
    ActivationStateType activation_state = ActivationStateType::kUnknown;
    const chromeos::network_config::mojom::DeviceStateProperties*
        cellular_device = model()->GetDevice(NetworkType::kCellular);
    switch (network->type) {
      case NetworkType::kCellular:
        mobile_has_networks_ = true;
        activation_state =
            network->type_state->get_cellular()->activation_state;
        info->activation_state = activation_state;
        info->sim_locked = network->type_state->get_cellular()->sim_locked;
        info->sim_eid = network->type_state->get_cellular()->eid;

        if (cellular_device && IsInhibited(cellular_device))
          inhibited = true;
        // If cellular is not enabled, skip cellular networks with no service.
        if (model()->GetDeviceState(NetworkType::kCellular) !=
                DeviceStateType::kEnabled &&
            activation_state == ActivationStateType::kNoService) {
          continue;
        }
        break;
      case NetworkType::kWiFi:
        wifi_has_networks_ = true;
        info->secured = network->type_state->get_wifi()->security !=
                        chromeos::network_config::mojom::SecurityType::kNone;
        break;
      case NetworkType::kTether:
        mobile_has_networks_ = true;
        tether_has_networks_ = true;
        info->battery_percentage =
            network->type_state->get_tether()->battery_percentage;
        break;
      default:
        break;
    }

    info->label = network_icon::GetLabelForNetworkList(network.get());
    // |network_list_| only contains non virtual networks.
    info->image = network_icon::GetImageForNonVirtualNetwork(
        network.get(), network_icon::ICON_TYPE_LIST, false /* badge_vpn */);

    info->type = network->type;
    info->disable = ComputeNetworkDisabledProperty(network, *info,
                                                   activation_state, inhibited);

    // If the device state is inhibited, we want to have the cellular network
    // rows not connectable.
    info->connectable = network->connectable && !inhibited && !info->sim_locked;

    if (network->prohibited_by_policy) {
      info->tooltip =
          l10n_util::GetStringUTF16(IDS_ASH_STATUS_TRAY_NETWORK_PROHIBITED);
    }

    info->connection_state = connection_state;

    info->signal_strength =
        chromeos::network_config::GetWirelessSignalStrength(network.get());

    info->source = network->source;

    if (!animating && connection_state == ConnectionStateType::kConnecting)
      animating = true;
    network_list_.push_back(std::move(info));
  }
  if (animating)
    network_icon::NetworkIconAnimation::GetInstance()->AddObserver(this);
  else
    network_icon::NetworkIconAnimation::GetInstance()->RemoveObserver(this);

  UpdateNetworkListInternal();
}

void NetworkListView::UpdateNetworkListInternal() {
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
    // The within_bounds check is necessary when the network list goes beyond
    // the visible area (i.e. scrolling) and the mouse is below the tray pop-up.
    // The items not in view in the tray pop-up keep going down and have
    // View::GetVisibility() == true but they are masked and not seen by the
    // user. When the mouse is below the list where the item would be if the
    // list continued downward, IsMouseHovered() is true and this will trigger
    // an incorrect programmatic scroll if we don't stop it. The bounds check
    // ensures the view is actually visible within the tray pop-up.
    bool within_bounds =
        this->GetBoundsInScreen().Intersects(iter.second->GetBoundsInScreen());
    if (within_bounds && iter.second->IsMouseHovered()) {
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
  // Keep an index where the next child should be inserted.
  size_t index = 0;

  const NetworkStateProperties* default_network = model()->default_network();
  bool using_proxy =
      default_network && default_network->proxy_mode != ProxyMode::kDirect;
  // Show a warning that the connection might be monitored if connected to a VPN
  // or if the default network has a proxy installed.
  if (vpn_connected_ || using_proxy) {
    if (!connection_warning_)
      connection_warning_ = CreateConnectionWarning();
    PlaceViewAtIndex(connection_warning_, index++);
  }

  // First add Ethernet networks.
  std::unique_ptr<std::set<std::string>> new_guids =
      UpdateNetworkChildren(NetworkType::kEthernet, index);
  index += new_guids->size();

  if (ShouldMobileDataSectionBeShown()) {
    if (!mobile_header_view_) {
      RecordDetailedViewSection(DetailedViewSection::kMobileSection);
      mobile_header_view_ = new MobileSectionHeaderView();
    }

    index = UpdateNetworkSectionHeader(
        NetworkType::kMobile, false /* enabled */, index, mobile_header_view_,
        &mobile_separator_view_);

    std::unique_ptr<std::set<std::string>> new_cellular_guids =
        UpdateNetworkChildren(NetworkType::kMobile, index);
    int mobile_status_message =
        mobile_header_view_->UpdateToggleAndGetStatusMessage(
            mobile_has_networks_, tether_has_networks_);
    // |mobile_status_message| may be zero. Passing zero to UpdateInfoLabel
    // clears the label.
    UpdateInfoLabel(mobile_status_message, index, &mobile_status_message_);
    if (mobile_status_message)
      ++index;
    index += new_cellular_guids->size();
    new_guids->insert(new_cellular_guids->begin(), new_cellular_guids->end());
  } else if (mobile_header_view_) {
    scroll_content()->RemoveChildView(mobile_header_view_);
    delete mobile_header_view_;
    mobile_header_view_ = nullptr;
    needs_relayout_ = true;
  }

  if (!wifi_header_view_) {
    RecordDetailedViewSection(DetailedViewSection::kWifiSection);
    wifi_header_view_ = new WifiSectionHeaderView();
  }

  bool wifi_enabled =
      model()->GetDeviceState(NetworkType::kWiFi) == DeviceStateType::kEnabled;
  index = UpdateNetworkSectionHeader(NetworkType::kWiFi, wifi_enabled, index,
                                     wifi_header_view_, &wifi_separator_view_);

  if (!wifi_enabled) {
    UpdateInfoLabel(IDS_ASH_STATUS_TRAY_NETWORK_WIFI_DISABLED, index,
                    &wifi_status_message_);
    return new_guids;
  }

  bool should_clear_info_label = true;
  if (!wifi_has_networks_) {
    UpdateInfoLabel(IDS_ASH_STATUS_TRAY_NETWORK_WIFI_ENABLED, index,
                    &wifi_status_message_);
    ++index;
    should_clear_info_label = false;
  }

  // Add Wi-Fi networks.
  std::unique_ptr<std::set<std::string>> new_wifi_guids =
      UpdateNetworkChildren(NetworkType::kWiFi, index);
  index += new_wifi_guids->size();
  new_guids->insert(new_wifi_guids->begin(), new_wifi_guids->end());

  // No networks or other messages (fallback).
  if (index == 0) {
    UpdateInfoLabel(IDS_ASH_STATUS_TRAY_NO_NETWORKS, index,
                    &wifi_status_message_);
  } else if (should_clear_info_label) {
    // Update the label to show nothing.
    UpdateInfoLabel(0, index, &wifi_status_message_);
  }

  return new_guids;
}

bool NetworkListView::ShouldMobileDataSectionBeShown() {
  // The section should always be shown if Cellular networks are available.
  if (model()->GetDeviceState(NetworkType::kCellular) !=
      DeviceStateType::kUnavailable) {
    return true;
  }

  DeviceStateType tether_state = model()->GetDeviceState(NetworkType::kTether);
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

void NetworkListView::UpdateViewForNetwork(HoverHighlightView* view,
                                           const NetworkInfo& info) {
  view->Reset();
  view->AddIconAndLabel(GetNetworkImageForNetwork(info), info.label);

  int cellular_subtext_message_id = GetCellularNetworkSubText(info);
  if (cellular_subtext_message_id) {
    SetupCellularListItemWithSubtext(view, info, cellular_subtext_message_id);
  } else {
    if (StateIsConnected(info.connection_state)) {
      SetupConnectedScrollListItem(view);
    } else if (info.connection_state == ConnectionStateType::kConnecting) {
      SetupConnectingScrollListItem(view);
    }
  }
  UpdateDisabledListItemTextColor(view, info);
  view->SetTooltipText(info.tooltip);

  // Add an additional icon to the right of the label for networks
  // that require it (e.g. Tether, controlled by extension).
  views::View* icon = CreatePowerStatusView(info);
  if (icon) {
    view->AddRightView(icon, views::CreateEmptyBorder(gfx::Insets::TLBR(
                                 0, 0, 0, kPowerStatusPaddingRight)));
  } else {
    icon = CreatePolicyView(info);
    if (icon)
      view->AddRightView(icon);
  }

  view->SetAccessibleName(GenerateAccessibilityLabel(info));
  view->GetViewAccessibility().OverrideDescription(
      GenerateAccessibilityDescription(info));

  needs_relayout_ = true;
}

std::u16string NetworkListView::GenerateAccessibilityLabel(
    const NetworkInfo& info) {
  if (CanNetworkConnect(info.connection_state, info.type, info.activation_state,
                        info.connectable, info.sim_eid)) {
    return l10n_util::GetStringFUTF16(
        IDS_ASH_STATUS_TRAY_NETWORK_A11Y_LABEL_CONNECT, info.label);
  }

  if (ShouldShowActivateCellularNetwork(info)) {
    return l10n_util::GetStringFUTF16(
        IDS_ASH_STATUS_TRAY_NETWORK_A11Y_LABEL_ACTIVATE, info.label);
  }

  if (ShouldShowContactCarrier(info)) {
    return l10n_util::GetStringFUTF16(
        IDS_ASH_STATUS_TRAY_NETWORK_A11Y_UNAVAILABLE_SIM_NETWORK, info.label);
  }

  return l10n_util::GetStringFUTF16(IDS_ASH_STATUS_TRAY_NETWORK_A11Y_LABEL_OPEN,
                                    info.label);
}

std::u16string NetworkListView::GenerateAccessibilityDescription(
    const NetworkInfo& info) {
  std::u16string connection_status;
  if (StateIsConnected(info.connection_state) ||
      info.connection_state == ConnectionStateType::kConnecting) {
    connection_status = l10n_util::GetStringUTF16(
        StateIsConnected(info.connection_state)
            ? IDS_ASH_STATUS_TRAY_NETWORK_STATUS_CONNECTED
            : IDS_ASH_STATUS_TRAY_NETWORK_STATUS_CONNECTING);
  }

  switch (info.type) {
    case NetworkType::kEthernet:
      if (!connection_status.empty()) {
        if (IsManagedByPolicy(info)) {
          return l10n_util::GetStringFUTF16(
              IDS_ASH_STATUS_TRAY_ETHERNET_A11Y_DESC_MANAGED_WITH_CONNECTION_STATUS,
              connection_status);
        }
        return connection_status;
      }
      if (IsManagedByPolicy(info)) {
        return l10n_util::GetStringUTF16(
            IDS_ASH_STATUS_TRAY_ETHERNET_A11Y_DESC_MANAGED);
      }
      return info.label;
    case NetworkType::kWiFi: {
      std::u16string security_label = l10n_util::GetStringUTF16(
          info.secured ? IDS_ASH_STATUS_TRAY_NETWORK_STATUS_SECURED
                       : IDS_ASH_STATUS_TRAY_NETWORK_STATUS_UNSECURED);
      if (!connection_status.empty()) {
        if (IsManagedByPolicy(info)) {
          return l10n_util::GetStringFUTF16(
              IDS_ASH_STATUS_TRAY_WIFI_NETWORK_A11Y_DESC_MANAGED_WITH_CONNECTION_STATUS,
              security_label, connection_status,
              base::FormatPercent(info.signal_strength));
        }
        return l10n_util::GetStringFUTF16(
            IDS_ASH_STATUS_TRAY_WIFI_NETWORK_A11Y_DESC_WITH_CONNECTION_STATUS,
            security_label, connection_status,
            base::FormatPercent(info.signal_strength));
      }
      if (IsManagedByPolicy(info)) {
        return l10n_util::GetStringFUTF16(
            IDS_ASH_STATUS_TRAY_WIFI_NETWORK_A11Y_DESC_MANAGED, security_label,
            base::FormatPercent(info.signal_strength));
      }
      return l10n_util::GetStringFUTF16(
          IDS_ASH_STATUS_TRAY_WIFI_NETWORK_A11Y_DESC, security_label,
          base::FormatPercent(info.signal_strength));
    }
    case NetworkType::kCellular:
      if (ShouldShowActivateCellularNetwork(info)) {
        return l10n_util::GetStringUTF16(
            IDS_ASH_STATUS_TRAY_NETWORK_STATUS_CLICK_TO_ACTIVATE);
      }
      if (ShouldShowContactCarrier(info)) {
        return l10n_util::GetStringUTF16(
            IDS_ASH_STATUS_TRAY_NETWORK_UNAVAILABLE_SIM_NETWORK);
      }
      if (info.sim_locked) {
        if (Shell::Get()->session_controller()->IsActiveUserSessionStarted()) {
          return l10n_util::GetStringUTF16(
              IDS_ASH_STATUS_TRAY_NETWORK_STATUS_CLICK_TO_UNLOCK);
        }
        return l10n_util::GetStringUTF16(
            IDS_ASH_STATUS_TRAY_NETWORK_STATUS_SIGN_IN_TO_UNLOCK);
      }
      if (!connection_status.empty()) {
        if (IsManagedByPolicy(info)) {
          return l10n_util::GetStringFUTF16(
              IDS_ASH_STATUS_TRAY_CELLULAR_NETWORK_A11Y_DESC_MANAGED_WITH_CONNECTION_STATUS,
              connection_status, base::FormatPercent(info.signal_strength));
        }
        return l10n_util::GetStringFUTF16(
            IDS_ASH_STATUS_TRAY_CELLULAR_NETWORK_A11Y_DESC_WITH_CONNECTION_STATUS,
            connection_status, base::FormatPercent(info.signal_strength));
      }
      if (IsManagedByPolicy(info)) {
        return l10n_util::GetStringFUTF16(
            IDS_ASH_STATUS_TRAY_CELLULAR_NETWORK_A11Y_DESC_MANAGED,
            base::FormatPercent(info.signal_strength));
      }
      return l10n_util::GetStringFUTF16(
          IDS_ASH_STATUS_TRAY_CELLULAR_NETWORK_A11Y_DESC,
          base::FormatPercent(info.signal_strength));
    case NetworkType::kTether:
      if (!connection_status.empty()) {
        return l10n_util::GetStringFUTF16(
            IDS_ASH_STATUS_TRAY_TETHER_NETWORK_A11Y_DESC_WITH_CONNECTION_STATUS,
            connection_status, base::FormatPercent(info.signal_strength),
            base::FormatPercent(info.battery_percentage));
      }
      return l10n_util::GetStringFUTF16(
          IDS_ASH_STATUS_TRAY_TETHER_NETWORK_A11Y_DESC,
          base::FormatPercent(info.signal_strength),
          base::FormatPercent(info.battery_percentage));
    default:
      return u"";
  }
}

views::View* NetworkListView::CreatePowerStatusView(const NetworkInfo& info) {
  // Mobile can be Cellular or Tether.
  if (!NetworkTypeMatchesType(info.type, NetworkType::kMobile))
    return nullptr;

  // Only return a battery icon for Tether network type.
  if (info.type != NetworkType::kTether)
    return nullptr;

  views::ImageView* icon = new views::ImageView;
  const SkColor icon_color = GetIconColor();
  icon->SetPreferredSize(gfx::Size(kMenuIconSize, kMenuIconSize));
  icon->SetFlipCanvasOnPaintForRTLUI(true);
  PowerStatus::BatteryImageInfo icon_info;
  icon_info.charge_percent = info.battery_percentage;
  icon->SetImage(PowerStatus::GetBatteryImage(
      icon_info, kMobileNetworkBatteryIconSize,
      ColorUtil::GetSecondToneColor(icon_color), icon_color));

  // Show the numeric battery percentage on hover.
  icon->SetTooltipText(base::FormatPercent(info.battery_percentage));

  return icon;
}

views::View* NetworkListView::CreatePolicyView(const NetworkInfo& info) {
  // Check if the network is managed by policy.
  OncSource source = info.source;
  if (source != OncSource::kDevicePolicy && source != OncSource::kUserPolicy)
    return nullptr;

  views::ImageView* controlled_icon =
      TrayPopupUtils::CreateMainImageView(/*use_wide_layout=*/false);
  controlled_icon->SetImage(
      gfx::CreateVectorIcon(kSystemMenuBusinessIcon, GetIconColor()));
  return controlled_icon;
}

std::unique_ptr<std::set<std::string>> NetworkListView::UpdateNetworkChildren(
    NetworkType type,
    size_t index) {
  std::unique_ptr<std::set<std::string>> new_guids(new std::set<std::string>);
  for (const auto& info : network_list_) {
    if (!NetworkTypeMatchesType(info->type, type))
      continue;
    UpdateNetworkChild(index++, info.get());
    new_guids->insert(info->guid);
  }
  return new_guids;
}

void NetworkListView::UpdateNetworkChild(size_t index,
                                         const NetworkInfo* info) {
  HoverHighlightView* network_view = nullptr;
  NetworkGuidMap::const_iterator found = network_guid_map_.find(info->guid);

  // This value is used to determine whether at least one network of |type| type
  // already existed prior to this method.
  bool has_reordered_a_network = false;

  if (found == network_guid_map_.end()) {
    network_view = new HoverHighlightView(this);
    UpdateViewForNetwork(network_view, *info);
  } else {
    has_reordered_a_network = true;
    network_view = found->second;
    if (NeedUpdateViewForNetwork(*info))
      UpdateViewForNetwork(network_view, *info);
  }

  // Only emit ethernet metric each time we show Ethernet section
  // for the first time. We use |has_reordered_a_network| to determine
  // if Ethernet networks already exist in network detailed list.
  if (NetworkTypeMatchesType(info->type, NetworkType::kEthernet) &&
      !has_reordered_a_network) {
    RecordDetailedViewSection(DetailedViewSection::kEthernetSection);
  }

  PlaceViewAtIndex(network_view, index);
  network_view->SetEnabled(!info->disable);
  network_map_[network_view] = info->guid;
  network_guid_map_[info->guid] = network_view;
}

void NetworkListView::PlaceViewAtIndex(views::View* view, size_t index) {
  if (view->parent() != scroll_content()) {
    scroll_content()->AddChildViewAt(view, index);
  } else if (index > 0 && index < scroll_content()->children().size() &&
             scroll_content()->children()[index] == view) {
    // ReorderChildView() would no-op in this case, but we still want to avoid
    // setting |needs_relayout_|.
    return;
  } else {
    scroll_content()->ReorderChildView(view, index);
  }
  needs_relayout_ = true;
}

void NetworkListView::UpdateInfoLabel(int message_id,
                                      size_t insertion_index,
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
    info_label = new TrayInfoLabel(message_id);
  else
    info_label->Update(message_id);

  PlaceViewAtIndex(info_label, insertion_index);
  *info_label_ptr = info_label;
}

size_t NetworkListView::UpdateNetworkSectionHeader(
    chromeos::network_config::mojom::NetworkType type,
    bool enabled,
    size_t child_index,
    NetworkSectionHeaderView* view,
    views::Separator** separator_view) {
  // Show or hide a separator above the header. The separator should only be
  // visible when the header row is not at the top of the list.
  if (child_index > 0) {
    if (!*separator_view)
      *separator_view = TrayPopupUtils::CreateListSubHeaderSeparator();
    PlaceViewAtIndex(*separator_view, child_index++);
  } else {
    if (*separator_view)
      delete *separator_view;
    *separator_view = nullptr;
  }

  // Mobile updates its toggle state independently.
  if (!NetworkTypeMatchesType(type, NetworkType::kMobile))
    view->SetToggleState(true /* toggle_enabled */, enabled /* is_on */);
  PlaceViewAtIndex(view, child_index++);
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
  TriView* connection_warning = TrayPopupUtils::CreateDefaultRowView(
      /*use_wide_layout=*/false);
  TrayPopupUtils::ConfigureAsStickyHeader(connection_warning);

  // Set 'info' icon on left side.
  views::ImageView* image_view =
      TrayPopupUtils::CreateMainImageView(/*use_wide_layout=*/false);
  image_view->SetImage(
      gfx::CreateVectorIcon(kSystemMenuInfoIcon, GetIconColor()));
  image_view->SetBackground(views::CreateSolidBackground(SK_ColorTRANSPARENT));
  connection_warning->AddView(TriView::Container::START, image_view);

  // Set message label in middle of row.
  views::Label* label = TrayPopupUtils::CreateDefaultLabel();
  label->SetText(
      l10n_util::GetStringUTF16(IDS_ASH_STATUS_TRAY_NETWORK_MONITORED_WARNING));
  label->SetBackground(views::CreateSolidBackground(SK_ColorTRANSPARENT));
  label->SetEnabledColor(AshColorProvider::Get()->GetContentLayerColor(
      AshColorProvider::ContentLayerType::kTextColorPrimary));
  TrayPopupUtils::SetLabelFontList(
      label, TrayPopupUtils::FontStyle::kDetailedViewLabel);

  connection_warning->AddView(TriView::Container::CENTER, label);
  connection_warning->SetContainerBorder(
      TriView::Container::CENTER, views::CreateEmptyBorder(gfx::Insets::TLBR(
                                      0, 0, 0, kTrayPopupLabelRightPadding)));

  // Nothing to the right of the text.
  connection_warning->SetContainerVisible(TriView::Container::END, false);
  return connection_warning;
}

}  // namespace ash
