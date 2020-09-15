// Copyright 2016 The Chromium Authors. All rights reserved.
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
#include "ash/system/model/system_tray_model.h"
#include "ash/system/network/network_icon.h"
#include "ash/system/network/network_icon_animation.h"
#include "ash/system/network/network_info.h"
#include "ash/system/network/network_section_header_view.h"
#include "ash/system/network/network_state_list_detailed_view.h"
#include "ash/system/network/tray_network_state_model.h"
#include "ash/system/power/power_status.h"
#include "ash/system/tray/hover_highlight_view.h"
#include "ash/system/tray/tray_info_label.h"
#include "ash/system/tray/tray_popup_item_style.h"
#include "ash/system/tray/tray_popup_utils.h"
#include "ash/system/tray/tri_view.h"
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
#include "ui/views/controls/image_view.h"
#include "ui/views/controls/scroll_view.h"
#include "ui/views/controls/separator.h"

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
namespace tray {
namespace {

const int kMobileNetworkBatteryIconSize = 18;
const int kPowerStatusPaddingRight = 10;

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
    ActivationStateType activation_state = ActivationStateType::kUnknown;
    switch (network->type) {
      case NetworkType::kCellular:
        activation_state =
            network->type_state->get_cellular()->activation_state;
        // If cellular is not enabled, skip cellular networks with no service.
        if (model()->GetDeviceState(NetworkType::kCellular) !=
                DeviceStateType::kEnabled &&
            activation_state == ActivationStateType::kNoService) {
          continue;
        }
        // Real (non 'default') Cellular networks are always connectable.
        if (network->connectable)
          mobile_has_networks_ = true;
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
    info->disable = activation_state == ActivationStateType::kActivating ||
                    network->prohibited_by_policy;
    info->connectable = network->connectable;
    if (network->prohibited_by_policy) {
      info->tooltip =
          l10n_util::GetStringUTF16(IDS_ASH_STATUS_TRAY_NETWORK_PROHIBITED);
    }

    info->connection_state = connection_state;

    info->signal_strength =
        chromeos::network_config::GetWirelessSignalStrength(network.get());

    if (network->captive_portal_provider) {
      info->captive_portal_provider_name =
          network->captive_portal_provider->name;
    }

    info->type = network->type;
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
  int index = 0;

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
    if (!mobile_header_view_)
      mobile_header_view_ = new MobileSectionHeaderView();

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

  if (!wifi_header_view_)
    wifi_header_view_ = new WifiSectionHeaderView();

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
  view->AddIconAndLabel(network_image, info.label);
  if (StateIsConnected(info.connection_state))
    SetupConnectedScrollListItem(view);
  else if (info.connection_state == ConnectionStateType::kConnecting)
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

  view->SetAccessibleName(GenerateAccessibilityLabel(info));
  view->GetViewAccessibility().OverrideDescription(
      GenerateAccessibilityDescription(info));

  needs_relayout_ = true;
}

base::string16 NetworkListView::GenerateAccessibilityLabel(
    const NetworkInfo& info) {
  if (CanNetworkConnect(info.connection_state, info.type, info.connectable)) {
    return l10n_util::GetStringFUTF16(
        IDS_ASH_STATUS_TRAY_NETWORK_A11Y_LABEL_CONNECT, info.label);
  }
  return l10n_util::GetStringFUTF16(IDS_ASH_STATUS_TRAY_NETWORK_A11Y_LABEL_OPEN,
                                    info.label);
}

base::string16 NetworkListView::GenerateAccessibilityDescription(
    const NetworkInfo& info) {
  base::string16 connection_status;
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
        return l10n_util::GetStringFUTF16(
            IDS_ASH_STATUS_TRAY_ETHERNET_A11Y_DESC_MANAGED, info.label,
            connection_status);
      }
      return info.label;
    case NetworkType::kWiFi: {
      base::string16 security_label = l10n_util::GetStringUTF16(
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
      return base::ASCIIToUTF16("");
  }
}  // namespace tray

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
  icon->EnableCanvasFlippingForRTLUI(true);
  PowerStatus::BatteryImageInfo icon_info;
  icon_info.charge_percent = info.battery_percentage;
  icon->SetImage(PowerStatus::GetBatteryImage(
      icon_info, kMobileNetworkBatteryIconSize,
      AshColorProvider::GetSecondToneColor(icon_color), icon_color));

  // Show the numeric battery percentage on hover.
  icon->SetTooltipText(base::FormatPercent(info.battery_percentage));

  return icon;
}

views::View* NetworkListView::CreatePolicyView(const NetworkInfo& info) {
  // Check if the network is managed by policy.
  OncSource source = info.source;
  if (source != OncSource::kDevicePolicy && source != OncSource::kUserPolicy)
    return nullptr;

  views::ImageView* controlled_icon = TrayPopupUtils::CreateMainImageView();
  controlled_icon->SetImage(
      gfx::CreateVectorIcon(kSystemMenuBusinessIcon, GetIconColor()));
  return controlled_icon;
}

views::View* NetworkListView::CreateControlledByExtensionView(
    const NetworkInfo& info) {
  if (info.captive_portal_provider_name.empty())
    return nullptr;

  views::ImageView* controlled_icon = TrayPopupUtils::CreateMainImageView();
  controlled_icon->SetImage(
      gfx::CreateVectorIcon(kCaptivePortalIcon, GetIconColor()));
  controlled_icon->SetTooltipText(l10n_util::GetStringFUTF16(
      IDS_ASH_STATUS_TRAY_EXTENSION_CONTROLLED_WIFI,
      base::UTF8ToUTF16(info.captive_portal_provider_name)));
  controlled_icon->SetID(VIEW_ID_EXTENSION_CONTROLLED_WIFI);
  return controlled_icon;
}

std::unique_ptr<std::set<std::string>> NetworkListView::UpdateNetworkChildren(
    NetworkType type,
    int index) {
  std::unique_ptr<std::set<std::string>> new_guids(new std::set<std::string>);
  for (const auto& info : network_list_) {
    if (!NetworkTypeMatchesType(info->type, type))
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
  } else if (index > 0 && size_t{index} < scroll_content()->children().size() &&
             scroll_content()->children()[size_t{index}] == view) {
    // ReorderChildView() would no-op in this case, but we still want to avoid
    // setting |needs_relayout_|.
    return;
  } else {
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

int NetworkListView::UpdateNetworkSectionHeader(
    chromeos::network_config::mojom::NetworkType type,
    bool enabled,
    int child_index,
    NetworkSectionHeaderView* view,
    views::Separator** separator_view) {
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
  TriView* connection_warning = TrayPopupUtils::CreateDefaultRowView();
  TrayPopupUtils::ConfigureAsStickyHeader(connection_warning);

  // Set 'info' icon on left side.
  views::ImageView* image_view = TrayPopupUtils::CreateMainImageView();
  image_view->SetImage(
      gfx::CreateVectorIcon(kSystemMenuInfoIcon, GetIconColor()));
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
