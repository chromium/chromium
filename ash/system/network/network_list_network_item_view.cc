// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/network/network_list_network_item_view.h"

#include <optional>
#include <string>

#include "ash/constants/ash_features.h"
#include "ash/resources/vector_icons/vector_icons.h"
#include "ash/session/session_controller_impl.h"
#include "ash/shell.h"
#include "ash/strings/grit/ash_strings.h"
#include "ash/style/ash_color_provider.h"
#include "ash/style/color_util.h"
#include "ash/style/typography.h"
#include "ash/system/model/system_tray_model.h"
#include "ash/system/network/active_network_icon.h"
#include "ash/system/network/network_icon.h"
#include "ash/system/network/network_icon_animation.h"
#include "ash/system/network/network_utils.h"
#include "ash/system/network/tray_network_state_model.h"
#include "ash/system/power/power_status.h"
#include "ash/system/tray/hover_highlight_view.h"
#include "ash/system/tray/tray_info_label.h"
#include "ash/system/tray/tray_popup_utils.h"
#include "ash/system/tray/tray_utils.h"
#include "base/i18n/number_formatting.h"
#include "chromeos/constants/chromeos_features.h"
#include "chromeos/services/network_config/public/cpp/cros_network_config_util.h"
#include "chromeos/services/network_config/public/mojom/cros_network_config.mojom.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/chromeos/styles/cros_tokens_color_mappings.h"
#include "ui/gfx/image/image_skia.h"
#include "ui/gfx/image/image_skia_operations.h"
#include "ui/gfx/paint_vector_icon.h"
#include "ui/views/accessibility/view_accessibility.h"
#include "ui/views/controls/image_view.h"
#include "ui/views/view_utils.h"

namespace ash {

namespace {

using ::chromeos::network_config::IsInhibited;
using ::chromeos::network_config::NetworkTypeMatchesType;
using ::chromeos::network_config::StateIsConnected;
using ::chromeos::network_config::mojom::ActivationStateType;
using ::chromeos::network_config::mojom::CellularStateProperties;
using ::chromeos::network_config::mojom::ConnectionStateType;
using ::chromeos::network_config::mojom::NetworkStatePropertiesPtr;
using ::chromeos::network_config::mojom::NetworkType;
using ::chromeos::network_config::mojom::OncSource;
using ::chromeos::network_config::mojom::SecurityType;

const int kMobileNetworkBatteryIconSize = 20;
const int kPowerStatusPaddingRight = 10;
const double kAlphaValueForInhibitedIconOpacity = 0.3;

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

ActivationStateType GetNetworkActivationState(
    const NetworkStatePropertiesPtr& network_properties) {
  if (NetworkTypeMatchesType(network_properties->type,
                             NetworkType::kCellular)) {
    return network_properties->type_state->get_cellular()->activation_state;
  }

  return ActivationStateType::kUnknown;
}

bool IsCellularNetworkSimLocked(
    const NetworkStatePropertiesPtr& network_properties) {
  DCHECK(
      NetworkTypeMatchesType(network_properties->type, NetworkType::kCellular));
  return network_properties->type_state->get_cellular()->sim_locked;
}

bool IsCellularNetworkCarrierLocked(
    const NetworkStatePropertiesPtr& network_properties) {
  CHECK(
      NetworkTypeMatchesType(network_properties->type, NetworkType::kCellular));
  return network_properties->type_state->get_cellular()->sim_locked &&
         network_properties->type_state->get_cellular()->sim_lock_type ==
             "network-pin";
}

bool IsNetworkConnectable(const NetworkStatePropertiesPtr& network_properties) {
  // The network must not already be connected to be able to be connected to.
  if (network_properties->connection_state !=
      ConnectionStateType::kNotConnected) {
    return false;
  }

  if (NetworkTypeMatchesType(network_properties->type,
                             NetworkType::kCellular)) {
    // Cellular networks must be activated, uninhibited, and have an unlocked
    // SIM to be able to be connected to.
    const CellularStateProperties* cellular =
        network_properties->type_state->get_cellular().get();

    if (cellular->activation_state == ActivationStateType::kNotActivated &&
        !cellular->eid.empty()) {
      return false;
    }

    if (IsNetworkInhibited(network_properties) || cellular->sim_locked) {
      return false;
    }

    if (cellular->activation_state == ActivationStateType::kActivated) {
      return true;
    }
  }

  // The network can be connected to if the network is connectable.
  if (network_properties->connectable) {
    return true;
  }

  // Network can be connected to if the active user is the primary user and the
  // network is configurable.
  if (!IsSecondaryUser() &&
      NetworkTypeIsConfigurable(network_properties->type)) {
    return true;
  }

  return false;
}

bool IsWifiNetworkSecured(const NetworkStatePropertiesPtr& network_properties) {
  DCHECK(NetworkTypeMatchesType(network_properties->type, NetworkType::kWiFi));
  return network_properties->type_state->get_wifi()->security !=
         SecurityType::kNone;
}

bool IsNetworkManagedByPolicy(
    const NetworkStatePropertiesPtr& network_properties) {
  return network_properties->source == OncSource::kDevicePolicy ||
         network_properties->source == OncSource::kUserPolicy;
}

bool IsCellularNetworkUnActivated(
    const NetworkStatePropertiesPtr& network_properties) {
  return GetNetworkActivationState(network_properties) ==
             ActivationStateType::kNotActivated &&
         network_properties->type_state->get_cellular()->eid.empty();
}

bool ShouldShowContactCarrier(
    const NetworkStatePropertiesPtr& network_properties) {
  return GetNetworkActivationState(network_properties) ==
             ActivationStateType::kNotActivated &&
         !network_properties->type_state->get_cellular()->eid.empty();
}

gfx::ImageSkia GetNetworkImageForNetwork(
    const ui::ColorProvider* color_provider,
    const NetworkStatePropertiesPtr& network_properties) {
  gfx::ImageSkia network_image;

  if (NetworkTypeMatchesType(network_properties->type,
                             NetworkType::kCellular) &&
      IsCellularNetworkCarrierLocked(network_properties)) {
    network_image = network_icon::GetImageForCarrierLockedNetwork(
        color_provider, network_icon::ICON_TYPE_LIST);
  } else if (IsCellularNetworkUnActivated(network_properties) &&
             Shell::Get()->session_controller()->login_status() ==
                 LoginStatus::NOT_LOGGED_IN) {
    network_image =
        network_icon::GetImageForPSimPendingActivationWhileLoggedOut(
            color_provider, network_icon::ICON_TYPE_LIST);
  } else {
    const gfx::ImageSkia image = network_icon::GetImageForNonVirtualNetwork(
        color_provider, network_properties.get(), network_icon::ICON_TYPE_LIST,
        /*badge_vpn=*/false);

    if (NetworkTypeMatchesType(network_properties->type,
                               NetworkType::kMobile) &&
        network_properties->connection_state ==
            ConnectionStateType::kNotConnected) {
      // Mobile icons which are not connecting or connected should display a
      // small "X" icon superimposed so that it is clear that they are
      // disconnected.
      const SkColor icon_color = network_icon::GetDefaultColorForIconType(
          color_provider, network_icon::ICON_TYPE_LIST);
      network_image = gfx::ImageSkiaOperations::CreateSuperimposedImage(
          image, gfx::CreateVectorIcon(kNetworkMobileNotConnectedXIcon,
                                       image.height(), icon_color));
    } else {
      network_image = image;
    }
  }

  // When the network is disabled, its appearance should be grayed out to
  // indicate users that these networks are unavailable. We must change the
  // image before we add it to the view, and then alter the label and sub-label
  // if they exist after it is added to the view.
  if (IsNetworkDisabled(network_properties)) {
    network_image = gfx::ImageSkiaOperations::CreateTransparentImage(
        network_image, kAlphaValueForInhibitedIconOpacity);
  }
  return network_image;
}

int GetCellularNetworkSubText(
    const NetworkStatePropertiesPtr& network_properties) {
  if (IsCellularNetworkCarrierLocked(network_properties)) {
    return IDS_ASH_STATUS_TRAY_NETWORK_STATUS_CARRIER_LOCKED;
  }

  if (IsCellularNetworkUnActivated(network_properties)) {
    if (Shell::Get()->session_controller()->login_status() ==
        LoginStatus::NOT_LOGGED_IN) {
      return IDS_ASH_STATUS_TRAY_NETWORK_STATUS_ACTIVATE_AFTER_DEVICE_SETUP;
    }
    return IDS_ASH_STATUS_TRAY_NETWORK_STATUS_CLICK_TO_ACTIVATE;
  }

  if (ShouldShowContactCarrier(network_properties)) {
    return IDS_ASH_STATUS_TRAY_NETWORK_UNAVAILABLE_SIM_NETWORK;
  }
  if (!IsCellularNetworkSimLocked(network_properties)) {
    return 0;
  }
  if (Shell::Get()->session_controller()->IsActiveUserSessionStarted()) {
    return IDS_ASH_STATUS_TRAY_NETWORK_STATUS_CLICK_TO_UNLOCK;
  }
  return IDS_ASH_STATUS_TRAY_NETWORK_STATUS_SIGN_IN_TO_UNLOCK;
}

}  // namespace

NetworkListNetworkItemView::NetworkListNetworkItemView(
    ViewClickListener* listener)
    : NetworkListItemView(listener) {}

NetworkListNetworkItemView::~NetworkListNetworkItemView() {
  network_icon::NetworkIconAnimation::GetInstance()->RemoveObserver(this);
}

void NetworkListNetworkItemView::UpdateViewForNetwork(
    const NetworkStatePropertiesPtr& network_properties) {
  network_properties_ = mojo::Clone(network_properties);

  Reset();

  if (!GetColorProvider()) {
    return;
  }

  const std::u16string label = GetLabel();

  AddIconAndLabel(
      GetNetworkImageForNetwork(GetColorProvider(), network_properties_),
      label);

  if (network_properties_.get()->type == NetworkType::kCellular) {
    SetupCellularSubtext();
  } else {
    SetupNetworkSubtext();
  }

  if (text_label()) {
    text_label()->SetEnabledColorId(cros_tokens::kCrosSysOnSurface);
    TypographyProvider::Get()->StyleLabel(TypographyToken::kCrosButton2,
                                          *text_label());
  }

  if (IsNetworkDisabled(network_properties)) {
    UpdateDisabledTextColor();
  }

  if (network_properties_->prohibited_by_policy) {
    SetTooltipText(
        l10n_util::GetStringUTF16(IDS_ASH_STATUS_TRAY_NETWORK_PROHIBITED));
  }

  // Add an additional icon to the right of the label for networks
  // that require it (e.g. Tether, controlled by extension).
  if (network_properties_->type == NetworkType::kTether) {
    AddPowerStatusView();
  } else if (IsNetworkManagedByPolicy(network_properties)) {
    AddPolicyView();
  }

  const bool is_connecting =
      network_properties_->connection_state ==
      chromeos::network_config::mojom::ConnectionStateType::kConnecting;

  if (is_connecting) {
    network_icon::NetworkIconAnimation::GetInstance()->AddObserver(this);
  } else {
    network_icon::NetworkIconAnimation::GetInstance()->RemoveObserver(this);
  }

  GetViewAccessibility().SetName(GenerateAccessibilityLabel(label));
  GetViewAccessibility().SetDescription(GenerateAccessibilityDescription());
}

void NetworkListNetworkItemView::NetworkIconChanged() {
  DCHECK(views::IsViewClass<views::ImageView>(left_view()));
  static_cast<views::ImageView*>(left_view())
      ->SetImage(
          GetNetworkImageForNetwork(GetColorProvider(), network_properties_));
}

void NetworkListNetworkItemView::OnThemeChanged() {
  NetworkListItemView::OnThemeChanged();
  if (!network_properties_.is_null()) {
    NetworkIconChanged();
  }
}

void NetworkListNetworkItemView::SetupCellularSubtext() {
  int cellular_subtext_message_id =
      GetCellularNetworkSubText(network_properties_);

  if (!cellular_subtext_message_id) {
    SetupNetworkSubtext();
    return;
  }

  SetSubText(l10n_util::GetStringUTF16(cellular_subtext_message_id));
  sub_text_label()->SetEnabledColorId(cros_tokens::kCrosSysWarning);
  TypographyProvider::Get()->StyleLabel(TypographyToken::kCrosAnnotation1,
                                        *sub_text_label());
}

void NetworkListNetworkItemView::SetupNetworkSubtext() {
  if (network_properties()->connection_state ==
      ConnectionStateType::kConnecting) {
    SetupConnectingScrollListItem(this);
    return;
  }

  if (!StateIsConnected(network_properties()->connection_state)) {
    return;
  }

  std::optional<std::u16string> portal_subtext =
      GetPortalStateSubtext(network_properties()->portal_state);
  if (portal_subtext) {
    SetWarningSubText(this, *portal_subtext);
    return;
  }

  SetupConnectedScrollListItem(this);
}

void NetworkListNetworkItemView::UpdateDisabledTextColor() {
  if (text_label()) {
    SkColor primary_text_color = text_label()->GetEnabledColor();
    text_label()->SetEnabledColor(
        ColorUtil::GetDisabledColor(primary_text_color));
  }
  if (sub_text_label()) {
    SkColor sub_text_color = sub_text_label()->GetEnabledColor();
    sub_text_label()->SetEnabledColor(
        ColorUtil::GetDisabledColor(sub_text_color));
  }
}

void NetworkListNetworkItemView::AddPowerStatusView() {
  auto image_icon = std::make_unique<views::ImageView>();
  const SkColor icon_color = AshColorProvider::Get()->GetContentLayerColor(
      AshColorProvider::ContentLayerType::kIconColorPrimary);
  image_icon->SetPreferredSize(gfx::Size(kMenuIconSize, kMenuIconSize));
  image_icon->SetFlipCanvasOnPaintForRTLUI(true);

  int battery_percentage =
      network_properties()->type_state->get_tether()->battery_percentage;
  PowerStatus::BatteryImageInfo icon_info(icon_color);
  icon_info.charge_percent = battery_percentage;
  image_icon->SetImage(PowerStatus::GetBatteryImageModel(
      icon_info, kMobileNetworkBatteryIconSize));

  // Show the numeric battery percentage on hover.
  image_icon->SetTooltipText(base::FormatPercent(battery_percentage));

  AddRightView(image_icon.release(), views::CreateEmptyBorder(gfx::Insets::TLBR(
                                         0, 0, 0, kPowerStatusPaddingRight)));
}

void NetworkListNetworkItemView::AddPolicyView() {
  std::unique_ptr<views::ImageView> controlled_icon(
      TrayPopupUtils::CreateMainImageView(/*use_wide_layout=*/false));
  const SkColor icon_color = AshColorProvider::Get()->GetContentLayerColor(
      AshColorProvider::ContentLayerType::kIconColorPrimary);
  controlled_icon->SetImage(
      gfx::CreateVectorIcon(kSystemMenuBusinessIcon, icon_color));
  AddRightView(controlled_icon.release());
}

std::u16string NetworkListNetworkItemView::GenerateAccessibilityLabel(
    const std::u16string& label) {
  std::optional<std::u16string> portal_subtext =
      GetPortalStateSubtext(network_properties()->portal_state);
  if (portal_subtext) {
    return l10n_util::GetStringFUTF16(
        IDS_ASH_STATUS_TRAY_NETWORK_A11Y_LABEL_SUBTEXT, label, *portal_subtext);
  }

  if (IsNetworkConnectable(network_properties())) {
    return l10n_util::GetStringFUTF16(
        IDS_ASH_STATUS_TRAY_NETWORK_A11Y_LABEL_CONNECT, label);
  }

  if (IsCellularNetworkUnActivated(network_properties())) {
    if (Shell::Get()->session_controller()->login_status() ==
        LoginStatus::NOT_LOGGED_IN) {
      return l10n_util::GetStringFUTF16(
          IDS_ASH_STATUS_TRAY_NETWORK_A11Y_LABEL_ACTIVATE_AFTER_SETUP, label);
    }
    return l10n_util::GetStringFUTF16(
        IDS_ASH_STATUS_TRAY_NETWORK_A11Y_LABEL_ACTIVATE, label);
  }

  if (ShouldShowContactCarrier(network_properties())) {
    return l10n_util::GetStringFUTF16(
        IDS_ASH_STATUS_TRAY_NETWORK_A11Y_UNAVAILABLE_SIM_NETWORK, label);
  }

  return l10n_util::GetStringFUTF16(IDS_ASH_STATUS_TRAY_NETWORK_A11Y_LABEL_OPEN,
                                    label);
}

std::u16string NetworkListNetworkItemView::GenerateAccessibilityDescription() {
  std::u16string connection_status;

  if (StateIsConnected(network_properties()->connection_state)) {
    std::optional<std::u16string> portal_subtext =
        GetPortalStateSubtext(network_properties()->portal_state);
    if (portal_subtext) {
      connection_status = *portal_subtext;
    } else {
      connection_status = l10n_util::GetStringUTF16(
          IDS_ASH_STATUS_TRAY_NETWORK_STATUS_CONNECTED);
    }
  } else if (network_properties()->connection_state ==
             ConnectionStateType::kConnecting) {
    connection_status = l10n_util::GetStringUTF16(
        IDS_ASH_STATUS_TRAY_NETWORK_STATUS_CONNECTING);
  }

  int signal_strength = chromeos::network_config::GetWirelessSignalStrength(
      network_properties().get());

  switch (network_properties()->type) {
    case NetworkType::kEthernet:
      return GenerateAccessibilityDescriptionForEthernet(connection_status);
    case NetworkType::kWiFi:
      return GenerateAccessibilityDescriptionForWifi(connection_status,
                                                     signal_strength);
    case NetworkType::kCellular:
      return GenerateAccessibilityDescriptionForCellular(connection_status,
                                                         signal_strength);
    case NetworkType::kTether:
      return GenerateAccessibilityDescriptionForTether(connection_status,
                                                       signal_strength);
    default:
      return u"";
  }
}

std::u16string
NetworkListNetworkItemView::GenerateAccessibilityDescriptionForEthernet(
    const std::u16string& connection_status) {
  if (!connection_status.empty()) {
    if (IsNetworkManagedByPolicy(network_properties())) {
      return l10n_util::GetStringFUTF16(
          IDS_ASH_STATUS_TRAY_ETHERNET_A11Y_DESC_MANAGED_WITH_CONNECTION_STATUS,
          connection_status);
    }
    return connection_status;
  }
  if (IsNetworkManagedByPolicy(network_properties())) {
    return l10n_util::GetStringUTF16(
        IDS_ASH_STATUS_TRAY_ETHERNET_A11Y_DESC_MANAGED);
  }
  return l10n_util::GetStringUTF16(IDS_ASH_STATUS_TRAY_ETHERNET);
}

std::u16string
NetworkListNetworkItemView::GenerateAccessibilityDescriptionForWifi(
    const std::u16string& connection_status,
    int signal_strength) {
  const std::u16string security_label = l10n_util::GetStringUTF16(
      IsWifiNetworkSecured(network_properties())
          ? IDS_ASH_STATUS_TRAY_NETWORK_STATUS_SECURED
          : IDS_ASH_STATUS_TRAY_NETWORK_STATUS_UNSECURED);
  if (!connection_status.empty()) {
    if (IsNetworkManagedByPolicy(network_properties())) {
      return l10n_util::GetStringFUTF16(
          IDS_ASH_STATUS_TRAY_WIFI_NETWORK_A11Y_DESC_MANAGED_WITH_CONNECTION_STATUS,
          security_label, connection_status,
          base::FormatPercent(signal_strength));
    }
    return l10n_util::GetStringFUTF16(
        IDS_ASH_STATUS_TRAY_WIFI_NETWORK_A11Y_DESC_WITH_CONNECTION_STATUS,
        security_label, connection_status,
        base::FormatPercent(signal_strength));
  }
  if (IsNetworkManagedByPolicy(network_properties())) {
    return l10n_util::GetStringFUTF16(
        IDS_ASH_STATUS_TRAY_WIFI_NETWORK_A11Y_DESC_MANAGED, security_label,
        base::FormatPercent(signal_strength));
  }
  return l10n_util::GetStringFUTF16(IDS_ASH_STATUS_TRAY_WIFI_NETWORK_A11Y_DESC,
                                    security_label,
                                    base::FormatPercent(signal_strength));
}

std::u16string
NetworkListNetworkItemView::GenerateAccessibilityDescriptionForCellular(
    const std::u16string& connection_status,
    int signal_strength) {
  if (IsCellularNetworkCarrierLocked(network_properties())) {
    return l10n_util::GetStringUTF16(
        IDS_ASH_STATUS_TRAY_NETWORK_STATUS_CARRIER_LOCKED);
  }
  if (IsCellularNetworkUnActivated(network_properties())) {
    if (Shell::Get()->session_controller()->login_status() ==
        LoginStatus::NOT_LOGGED_IN) {
      return l10n_util::GetStringUTF16(
          IDS_ASH_STATUS_TRAY_NETWORK_STATUS_ACTIVATE_AFTER_DEVICE_SETUP);
    }
    return l10n_util::GetStringUTF16(
        IDS_ASH_STATUS_TRAY_NETWORK_STATUS_CLICK_TO_ACTIVATE);
  }
  if (ShouldShowContactCarrier(network_properties())) {
    return l10n_util::GetStringUTF16(
        IDS_ASH_STATUS_TRAY_NETWORK_UNAVAILABLE_SIM_NETWORK);
  }
  if (IsCellularNetworkSimLocked(network_properties())) {
    if (Shell::Get()->session_controller()->IsActiveUserSessionStarted()) {
      return l10n_util::GetStringUTF16(
          IDS_ASH_STATUS_TRAY_NETWORK_STATUS_CLICK_TO_UNLOCK);
    }
    return l10n_util::GetStringUTF16(
        IDS_ASH_STATUS_TRAY_NETWORK_STATUS_SIGN_IN_TO_UNLOCK);
  }
  if (!connection_status.empty()) {
    if (IsNetworkManagedByPolicy(network_properties())) {
      return l10n_util::GetStringFUTF16(
          IDS_ASH_STATUS_TRAY_CELLULAR_NETWORK_A11Y_DESC_MANAGED_WITH_CONNECTION_STATUS,
          connection_status, base::FormatPercent(signal_strength));
    }
    return l10n_util::GetStringFUTF16(
        IDS_ASH_STATUS_TRAY_CELLULAR_NETWORK_A11Y_DESC_WITH_CONNECTION_STATUS,
        connection_status, base::FormatPercent(signal_strength));
  }
  if (IsNetworkManagedByPolicy(network_properties())) {
    return l10n_util::GetStringFUTF16(
        IDS_ASH_STATUS_TRAY_CELLULAR_NETWORK_A11Y_DESC_MANAGED,
        base::FormatPercent(signal_strength));
  }
  return l10n_util::GetStringFUTF16(
      IDS_ASH_STATUS_TRAY_CELLULAR_NETWORK_A11Y_DESC,
      base::FormatPercent(signal_strength));
}

std::u16string
NetworkListNetworkItemView::GenerateAccessibilityDescriptionForTether(
    const std::u16string& connection_status,
    int signal_strength) {
  int battery_percentage =
      network_properties()->type_state->get_tether()->battery_percentage;
  if (!connection_status.empty()) {
    return l10n_util::GetStringFUTF16(
        IDS_ASH_STATUS_TRAY_TETHER_NETWORK_A11Y_DESC_WITH_CONNECTION_STATUS,
        connection_status, base::FormatPercent(signal_strength),
        base::FormatPercent(battery_percentage));
  }
  return l10n_util::GetStringFUTF16(
      IDS_ASH_STATUS_TRAY_TETHER_NETWORK_A11Y_DESC,
      base::FormatPercent(signal_strength),
      base::FormatPercent(battery_percentage));
}

BEGIN_METADATA(NetworkListNetworkItemView)
END_METADATA

}  // namespace ash
