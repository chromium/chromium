// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/network/network_icon.h"

#include <algorithm>
#include <tuple>
#include <utility>

#include "ash/constants/ash_features.h"
#include "ash/public/cpp/network_icon_image_source.h"
#include "ash/resources/vector_icons/vector_icons.h"
#include "ash/strings/grit/ash_strings.h"
#include "ash/style/ash_color_provider.h"
#include "ash/style/color_util.h"
#include "ash/system/network/network_icon_animation.h"
#include "ash/system/network/network_icon_animation_observer.h"
#include "ash/system/tray/tray_constants.h"
#include "base/containers/flat_map.h"
#include "base/memory/ptr_util.h"
#include "base/memory/raw_ptr.h"
#include "base/strings/utf_string_conversions.h"
#include "chromeos/constants/chromeos_features.h"
#include "chromeos/services/network_config/public/cpp/cros_network_config_util.h"
#include "chromeos/services/network_config/public/mojom/cros_network_config.mojom.h"
#include "components/onc/onc_constants.h"
#include "components/vector_icons/vector_icons.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/chromeos/styles/cros_tokens_color_mappings.h"
#include "ui/color/color_provider.h"
#include "ui/gfx/geometry/skia_conversions.h"
#include "ui/gfx/image/image_skia.h"
#include "ui/gfx/image/image_skia_operations.h"
#include "ui/gfx/image/image_skia_source.h"
#include "ui/gfx/paint_vector_icon.h"
#include "ui/gfx/vector_icon_types.h"

using chromeos::network_config::mojom::ActivationStateType;
using chromeos::network_config::mojom::ConnectionStateType;
using chromeos::network_config::mojom::NetworkStateProperties;
using chromeos::network_config::mojom::NetworkType;
using chromeos::network_config::mojom::SecurityType;

namespace ash {
namespace network_icon {

namespace {

// class used for maintaining a map of network state and images.
class NetworkIconImpl {
 public:
  NetworkIconImpl(const ui::ColorProvider* color_provider,
                  const std::string& guid,
                  IconType icon_type,
                  NetworkType network_type);

  NetworkIconImpl(const NetworkIconImpl&) = delete;
  NetworkIconImpl& operator=(const NetworkIconImpl&) = delete;

  // Determines whether or not the associated network might be dirty and if so
  // updates and generates the icon. Does nothing if network no longer exists.
  void Update(const ui::ColorProvider* color_provider,
              const NetworkStateProperties* network,
              bool show_vpn_badge);

  const gfx::ImageSkia& image() const { return image_; }

 private:
  // Updates |strength_index_| for wireless networks. Returns true if changed.
  bool UpdateWirelessStrengthIndex(const NetworkStateProperties* network);

  // Updates the local state for cellular networks. Returns true if changed.
  bool UpdateCellularState(const NetworkStateProperties* network);

  // Gets |badges| based on |network| and the current state.
  void GetBadges(const NetworkStateProperties* network, Badges* badges);

  // Gets the appropriate icon and badges and composites the image.
  void GenerateImage(const NetworkStateProperties* network);

  // Gets the color for the icon
  raw_ptr<const ui::ColorProvider, DanglingUntriaged> color_provider_;

  // Defines color theme and VPN badging
  const IconType icon_type_;

  // Cached state of the network when the icon was last generated.
  SkColor color_;
  ConnectionStateType connection_state_ = ConnectionStateType::kNotConnected;
  int strength_index_ = -1;
  Badge technology_badge_ = {};
  bool show_vpn_badge_ = false;
  bool is_roaming_ = false;

  // Generated icon image.
  gfx::ImageSkia image_;
};

//------------------------------------------------------------------------------
// Maintain a static (global) icon map. Note: Icons are never destroyed;
// it is assumed that a finite and reasonable number of network icons will be
// created during a session.

typedef std::map<std::string, NetworkIconImpl*> NetworkIconMap;

NetworkIconMap* GetIconMapInstance(IconType icon_type, bool create) {
  typedef std::map<IconType, NetworkIconMap*> IconTypeMap;
  static IconTypeMap* s_icon_map = nullptr;
  if (s_icon_map == nullptr) {
    if (!create)
      return nullptr;
    s_icon_map = new IconTypeMap;
  }
  if (s_icon_map->count(icon_type) == 0) {
    if (!create)
      return nullptr;
    (*s_icon_map)[icon_type] = new NetworkIconMap;
  }
  return (*s_icon_map)[icon_type];
}

NetworkIconMap* GetIconMap(IconType icon_type) {
  return GetIconMapInstance(icon_type, true);
}

void PurgeIconMap(IconType icon_type,
                  const std::set<std::string>& network_guids) {
  NetworkIconMap* icon_map = GetIconMapInstance(icon_type, false);
  if (!icon_map)
    return;
  for (NetworkIconMap::iterator loop_iter = icon_map->begin();
       loop_iter != icon_map->end();) {
    NetworkIconMap::iterator cur_iter = loop_iter++;
    if (network_guids.count(cur_iter->first) == 0) {
      delete cur_iter->second;
      icon_map->erase(cur_iter);
    }
  }
}

//------------------------------------------------------------------------------
// Utilities for generating icon images.

// Amount to fade icons while connecting.
const double kConnectingImageAlpha = 0.5;

// Number of discrete images to use for alpha fade animation
const int kNumFadeImages = 10;

bool IsTrayIcon(IconType icon_type) {
  return icon_type == ICON_TYPE_TRAY_REGULAR ||
         icon_type == ICON_TYPE_TRAY_ACTIVE || icon_type == ICON_TYPE_TRAY_OOBE;
}

bool IconTypeHasVPNBadge(IconType icon_type) {
  return (icon_type != ICON_TYPE_LIST && icon_type != ICON_TYPE_MENU_LIST);
}

gfx::ImageSkia CreateNetworkIconImage(const gfx::ImageSkia& icon,
                                      const Badges& badges) {
  return gfx::CanvasImageSource::MakeImageSkia<NetworkIconImageSource>(
      icon.size(), icon, badges);
}

//------------------------------------------------------------------------------
// Utilities for extracting icon images.

ImageType ImageTypeForNetworkType(NetworkType network_type) {
  if (network_type == NetworkType::kWiFi)
    return ARCS;

  if (network_type == NetworkType::kCellular ||
      network_type == NetworkType::kTether) {
    return BARS;
  }

  return NONE;
}

gfx::ImageSkia GetImageForIndex(ImageType image_type,
                                SkColor color,
                                int index) {
  return gfx::CanvasImageSource::MakeImageSkia<SignalStrengthImageSource>(
      image_type, color, gfx::Size(kUnifiedTrayIconSize, kUnifiedTrayIconSize),
      index, kUnifiedTrayNetworkIconPadding);
}

gfx::ImageSkia& ConnectingWirelessImage(const ui::ColorProvider* color_provider,
                                        ImageType image_type,
                                        IconType icon_type,
                                        double animation) {
  // Connecting icons animate by adjusting their signal strength up and down,
  // but the empty (no signal) image is skipped for aesthetic reasons.
  static const int kNumConnectingImages = kNumNetworkImages - 1;

  // Cache of images used to avoid redrawing the icon during every animation;
  // the key is a tuple including a bool representing whether the icon displays
  // bars (as oppose to arcs), a SkColor representing whether the icon is to be
  // displayed in a specific color scheme, the IconType, and an int representing
  // the index of the image (with respect to GetImageForIndex()).
  static base::flat_map<std::tuple<bool, SkColor, IconType, int>,
                        gfx::ImageSkia>
      s_image_cache;

  // Note that if |image_type| is NONE, arcs are displayed by default.
  bool is_bars_image = image_type == BARS;

  int index =
      animation * nextafter(static_cast<float>(kNumConnectingImages), 0);
  index = std::clamp(index, 0, kNumConnectingImages - 1);

  auto map_key = std::make_tuple(
      is_bars_image, GetDefaultColorForIconType(color_provider, icon_type),
      icon_type, index);

  if (!s_image_cache.contains(map_key)) {
    // Lazily cache images.
    // TODO(estade): should the alpha be applied in SignalStrengthImageSource?
    gfx::ImageSkia source = GetImageForIndex(
        image_type, GetDefaultColorForIconType(color_provider, icon_type),
        index + 1);
    s_image_cache[map_key] =
        gfx::ImageSkia(gfx::ImageSkiaOperations::CreateTransparentImage(
            source, kConnectingImageAlpha));
  }

  return s_image_cache[map_key];
}

gfx::ImageSkia ConnectingVpnImage(double animation) {
  float floored_animation_value =
      std::floor(animation * kNumFadeImages) / kNumFadeImages;
  const SkColor icon_color = AshColorProvider::Get()->GetContentLayerColor(
      AshColorProvider::ContentLayerType::kIconColorPrimary);
  return gfx::CreateVectorIcon(
      kNetworkVpnIcon,
      gfx::Tween::ColorValueBetween(
          floored_animation_value,
          SkColorSetA(icon_color, kConnectingImageAlpha), icon_color));
}

int StrengthIndex(int strength) {
  if (strength <= 0)
    return 0;

  if (strength >= 100)
    return kNumNetworkImages - 1;

  // Return an index in the range [1, kNumNetworkImages - 1].
  // This logic is equivalent to network_icon.js:strengthToIndex_().
  int zero_based_index = (strength - 1) * (kNumNetworkImages - 1) / 100;
  return zero_based_index + 1;
}

Badge BadgeForNetworkTechnology(const NetworkStateProperties* network,
                                SkColor color) {
  DCHECK(network->type == NetworkType::kCellular);
  Badge badge = {nullptr, color};
  const std::string& technology =
      network->type_state->get_cellular()->network_technology;
  if (technology == onc::cellular::kTechnologyEvdo) {
    badge.icon = &kNetworkBadgeTechnologyEvdoIcon;
  } else if (technology == onc::cellular::kTechnologyCdma1Xrtt) {
    badge.icon = &kNetworkBadgeTechnology1xIcon;
  } else if (technology == onc::cellular::kTechnologyGprs ||
             technology == onc::cellular::kTechnologyGsm) {
    badge.icon = &kNetworkBadgeTechnologyGprsIcon;
  } else if (technology == onc::cellular::kTechnologyEdge) {
    badge.icon = &kNetworkBadgeTechnologyEdgeIcon;
  } else if (technology == onc::cellular::kTechnologyUmts) {
    badge.icon = &kNetworkBadgeTechnology3gIcon;
  } else if (technology == onc::cellular::kTechnologyHspa) {
    badge.icon = &kNetworkBadgeTechnologyHspaIcon;
  } else if (technology == onc::cellular::kTechnologyHspaPlus) {
    badge.icon = &kNetworkBadgeTechnologyHspaPlusIcon;
  } else if (technology == onc::cellular::kTechnologyLte) {
    badge.icon = &kNetworkBadgeTechnologyLteIcon;
  } else if (technology == onc::cellular::kTechnologyLteAdvanced) {
    badge.icon = &kNetworkBadgeTechnologyLteAdvancedIcon;
  } else if (technology == onc::cellular::kTechnology5gNr) {
    badge.icon = &kNetworkBadgeTechnology5gIcon;
  } else {
    return {};
  }
  return badge;
}

gfx::ImageSkia GetIcon(const ui::ColorProvider* color_provider,
                       const NetworkStateProperties* network,
                       IconType icon_type,
                       int strength_index) {
  if (network->type == NetworkType::kEthernet) {
    // The system tray uses a smaller icon.
    return gfx::CreateVectorIcon(
        IsTrayIcon(icon_type) ? kNetworkEthernetIcon
                              : vector_icons::kEthernetIcon,
        GetDefaultColorForIconType(color_provider, icon_type));
  }
  if (network->type == NetworkType::kVPN) {
    DCHECK(!IsTrayIcon(icon_type));
    return gfx::CreateVectorIcon(
        kNetworkVpnIcon,
        GetDefaultColorForIconType(color_provider, ICON_TYPE_LIST));
  }
  DCHECK_GE(strength_index, 0)
      << "Strength not set for type: " << network->type;
  DCHECK_LT(strength_index, kNumNetworkImages);
  return GetImageForIndex(ImageTypeForNetworkType(network->type),
                          GetDefaultColorForIconType(color_provider, icon_type),
                          strength_index);
}

gfx::ImageSkia GetConnectingVpnImage(IconType icon_type) {
  double animation = NetworkIconAnimation::GetInstance()->GetAnimation();
  gfx::ImageSkia icon = ConnectingVpnImage(animation);
  return CreateNetworkIconImage(icon, Badges());
}

}  // namespace

//------------------------------------------------------------------------------
// NetworkIconImpl

NetworkIconImpl::NetworkIconImpl(const ui::ColorProvider* color_provider,
                                 const std::string& guid,
                                 IconType icon_type,
                                 NetworkType network_type)
    : color_provider_(color_provider),
      icon_type_(icon_type),
      color_(GetDefaultColorForIconType(color_provider, icon_type)) {
  // Default image is null.
}

void NetworkIconImpl::Update(const ui::ColorProvider* color_provider,
                             const NetworkStateProperties* network,
                             bool show_vpn_badge) {
  color_provider_ = color_provider;

  // Determine whether or not we need to update the icon.
  bool dirty = image_.isNull();

  if (network->connection_state != connection_state_) {
    VLOG(2) << "Update connection state: "
            << static_cast<int>(network->connection_state);
    connection_state_ = network->connection_state;
    dirty = true;
  }

  NetworkType type = network->type;
  if (chromeos::network_config::NetworkTypeMatchesType(
          type, NetworkType::kWireless)) {
    dirty |= UpdateWirelessStrengthIndex(network);
  }

  if (type == NetworkType::kCellular)
    dirty |= UpdateCellularState(network);

  bool new_show_vpn_badge = show_vpn_badge && IconTypeHasVPNBadge(icon_type_);
  if (new_show_vpn_badge != show_vpn_badge_) {
    VLOG(2) << "Update VPN badge: " << new_show_vpn_badge;
    show_vpn_badge_ = new_show_vpn_badge;
    dirty = true;
  }

  // Check if the desired color has changed.
  const SkColor new_color =
      GetDefaultColorForIconType(color_provider, icon_type_);
  if (color_ != new_color) {
    dirty = true;
  }

  color_ = new_color;

  if (dirty) {
    // Set the icon and badges based on the network and generate the image.
    GenerateImage(network);
  }
}

bool NetworkIconImpl::UpdateWirelessStrengthIndex(
    const NetworkStateProperties* network) {
  int index = StrengthIndex(
      chromeos::network_config::GetWirelessSignalStrength(network));
  if (index != strength_index_) {
    VLOG(2) << "New strength index: " << index;
    strength_index_ = index;
    return true;
  }
  return false;
}

bool NetworkIconImpl::UpdateCellularState(
    const NetworkStateProperties* network) {
  bool dirty = false;
  if (!features::IsSeparateNetworkIconsEnabled()) {
    const Badge technology_badge = BadgeForNetworkTechnology(
        network, GetDefaultColorForIconType(color_provider_, icon_type_));
    if (technology_badge != technology_badge_) {
      VLOG(2) << "New technology badge.";
      technology_badge_ = technology_badge;
      dirty = true;
    }
  }

  bool roaming = network->type_state->get_cellular()->roaming;
  if (roaming != is_roaming_) {
    VLOG(2) << "New is_roaming: " << roaming;
    is_roaming_ = roaming;
    dirty = true;
  }
  return dirty;
}

void NetworkIconImpl::GetBadges(const NetworkStateProperties* network,
                                Badges* badges) {
  const NetworkType type = network->type;
  const SkColor icon_color =
      GetDefaultColorForIconType(color_provider_, icon_type_);
  bool is_connected =
      chromeos::network_config::StateIsConnected(network->connection_state);
  if (type == NetworkType::kWiFi) {
    if (network->type_state->get_wifi()->security != SecurityType::kNone &&
        !IsTrayIcon(icon_type_)) {
      badges->bottom_right = {&kUnifiedNetworkBadgeSecureIcon, icon_color};
    }
  } else if (type == NetworkType::kCellular) {
    // technology_badge_ is set in UpdateCellularState.
    if (is_connected && network->type_state->get_cellular()->roaming) {
      badges->top_left = {&kNetworkBadgeRoamingIcon, icon_color};
    } else if (is_connected && !features::IsSeparateNetworkIconsEnabled()) {
      // Only show technology badge when connected and roaming is not active.
      badges->top_left = technology_badge_;
    }
  }

  if (show_vpn_badge_)
    badges->bottom_left = {&kUnifiedNetworkBadgeVpnIcon, icon_color};
  if (connection_state_ == ConnectionStateType::kPortal)
    badges->bottom_right = {&kUnifiedNetworkBadgeCaptivePortalIcon, icon_color};
}

void NetworkIconImpl::GenerateImage(const NetworkStateProperties* network) {
  gfx::ImageSkia icon =
      GetIcon(color_provider_, network, icon_type_, strength_index_);
  Badges badges;
  GetBadges(network, &badges);
  image_ = CreateNetworkIconImage(icon, badges);
}

namespace {

NetworkIconImpl* FindAndUpdateImageImpl(const ui::ColorProvider* color_provider,
                                        const NetworkStateProperties* network,
                                        IconType icon_type,
                                        bool show_vpn_badge) {
  // Find or add the icon.
  NetworkIconMap* icon_map = GetIconMap(icon_type);
  NetworkIconImpl* icon;
  NetworkIconMap::iterator iter = icon_map->find(network->guid);
  if (iter == icon_map->end()) {
    VLOG(1) << "new NetworkIconImpl: " << network->name;
    icon = new NetworkIconImpl(color_provider, network->guid, icon_type,
                               network->type);
    icon_map->insert(std::make_pair(network->guid, icon));
  } else {
    VLOG(1) << "found NetworkIconImpl: " << network->name;
    icon = iter->second;
  }

  // Update and return the icon's image.
  icon->Update(color_provider, network, show_vpn_badge);
  return icon;
}

}  // namespace

//------------------------------------------------------------------------------
// Public interface

SkColor GetDefaultColorForIconType(const ui::ColorProvider* color_provider,
                                   IconType icon_type) {
  // If |color_provider| is null, AshColorProvider will be used
  // to fetch the color instead.
  bool use_color_provider =
      chromeos::features::IsJellyrollEnabled() && color_provider;

  auto* ash_color_provider = AshColorProvider::Get();
  switch (icon_type) {
    case ICON_TYPE_TRAY_OOBE:
      return kIconColorInOobe;
    case ICON_TYPE_TRAY_REGULAR:
    case ICON_TYPE_FEATURE_POD:
    case ICON_TYPE_LIST:
      return use_color_provider
                 ? color_provider->GetColor(cros_tokens::kCrosSysOnSurface)
                 : ash_color_provider->GetContentLayerColor(
                       AshColorProvider::ContentLayerType::kButtonIconColor);
    case ICON_TYPE_TRAY_ACTIVE:
      return use_color_provider
                 ? color_provider->GetColor(
                       cros_tokens::kCrosSysSystemOnPrimaryContainer)
                 : ash_color_provider->GetContentLayerColor(
                       AshColorProvider::ContentLayerType::kButtonIconColor);
    case ICON_TYPE_FEATURE_POD_TOGGLED:
      return use_color_provider
                 ? color_provider->GetColor(
                       cros_tokens::kCrosSysSystemOnPrimaryContainer)
                 : ash_color_provider->GetContentLayerColor(
                       AshColorProvider::ContentLayerType::
                           kButtonIconColorPrimary);
    case ICON_TYPE_FEATURE_POD_DISABLED:
      return use_color_provider
                 ? color_provider->GetColor(cros_tokens::kCrosSysDisabled)
                 : color_utils::GetResultingPaintColor(
                       ColorUtil::GetDisabledColor(GetDefaultColorForIconType(
                           color_provider, ICON_TYPE_FEATURE_POD)),
                       ash_color_provider->GetBackgroundColor());
    default:
      return use_color_provider
                 ? color_provider->GetColor(cros_tokens::kCrosSysPrimary)
                 : ash_color_provider->GetContentLayerColor(
                       AshColorProvider::ContentLayerType::kIconColorPrimary);
  }
}

const gfx::ImageSkia GetBasicImage(const ui::ColorProvider* color_provider,
                                   IconType icon_type,
                                   NetworkType network_type,
                                   bool connected) {
  DCHECK_NE(NetworkType::kVPN, network_type);
  return GetImageForIndex(ImageTypeForNetworkType(network_type),
                          GetDefaultColorForIconType(color_provider, icon_type),
                          connected ? kNumNetworkImages - 1 : 0);
}

gfx::ImageSkia GetImageForNonVirtualNetwork(
    const ui::ColorProvider* color_provider,
    const NetworkStateProperties* network,
    IconType icon_type,
    bool show_vpn_badge,
    bool* animating) {
  DCHECK_NE(NetworkType::kVPN, network->type);
  NetworkType network_type = network->type;

  if (network->connection_state == ConnectionStateType::kConnecting) {
    if (animating)
      *animating = true;
    return GetConnectingImageForNetworkType(color_provider, network_type,
                                            icon_type);
  }

  NetworkIconImpl* icon = FindAndUpdateImageImpl(color_provider, network,
                                                 icon_type, show_vpn_badge);
  if (animating)
    *animating = false;
  return icon->image();
}

gfx::ImageSkia GetImageForVPN(const ui::ColorProvider* color_provider,
                              const NetworkStateProperties* vpn,
                              IconType icon_type,
                              bool* animating) {
  DCHECK_EQ(NetworkType::kVPN, vpn->type);
  if (vpn->connection_state == ConnectionStateType::kConnecting) {
    if (animating)
      *animating = true;
    return GetConnectingVpnImage(icon_type);
  }

  NetworkIconImpl* icon = FindAndUpdateImageImpl(color_provider, vpn, icon_type,
                                                 false /* show_vpn_badge */);
  if (animating)
    *animating = false;
  return icon->image();
}

gfx::ImageSkia GetImageForWiFiNoConnections(
    const ui::ColorProvider* color_provider,
    IconType icon_type) {
  return gfx::CreateVectorIcon(
      kUnifiedMenuWifiNoConnectionIcon,
      GetDefaultColorForIconType(color_provider, icon_type));
}

gfx::ImageSkia GetImageForPSimPendingActivationWhileLoggedOut(
    const ui::ColorProvider* color_provider,
    IconType icon_type) {
  return gfx::CreateVectorIcon(
      kUnifiedMenuCellularUnactivatedIcon,
      GetDefaultColorForIconType(color_provider, icon_type));
}

gfx::ImageSkia GetImageForCarrierLockedNetwork(
    const ui::ColorProvider* color_provider,
    IconType icon_type) {
  return gfx::CreateVectorIcon(
      kCarrierLockedIcon,
      GetDefaultColorForIconType(color_provider, icon_type));
}

gfx::ImageSkia GetImageForWiFiEnabledState(
    const ui::ColorProvider* color_provider,
    bool enabled,
    IconType icon_type) {
  if (!enabled) {
    return gfx::CreateVectorIcon(
        kUnifiedMenuWifiOffIcon, kUnifiedTrayIconSize,
        GetDefaultColorForIconType(color_provider, icon_type));
  }

  gfx::ImageSkia image = GetBasicImage(
      color_provider, icon_type, NetworkType::kWiFi, true /* connected */);
  Badges badges;
  if (!enabled) {
    badges.center = {&kNetworkBadgeOffIcon,
                     GetDefaultColorForIconType(color_provider, icon_type)};
  }
  return CreateNetworkIconImage(image, badges);
}

ui::ImageModel GetImageModelForWiFiEnabledState(bool wifi_enabled,
                                                IconType icon_type) {
  return ui::ImageModel::FromImageGenerator(
      base::BindRepeating(
          [](bool wifi_enabled, IconType icon_type,
             const ui::ColorProvider* provider) {
            return GetImageForWiFiEnabledState(provider, wifi_enabled);
          },
          wifi_enabled, icon_type),
      gfx::Size(kUnifiedTrayIconSize, kUnifiedTrayIconSize));
}

gfx::ImageSkia GetConnectingImageForNetworkType(
    const ui::ColorProvider* color_provider,
    NetworkType network_type,
    IconType icon_type) {
  DCHECK_NE(NetworkType::kVPN, network_type);
  ImageType image_type = ImageTypeForNetworkType(network_type);
  double animation = NetworkIconAnimation::GetInstance()->GetAnimation();

  return CreateNetworkIconImage(
      ConnectingWirelessImage(color_provider, image_type, icon_type, animation),
      Badges());
}

gfx::ImageSkia GetConnectedNetworkWithConnectingVpnImage(
    const ui::ColorProvider* color_provider,
    const NetworkStateProperties* connected_network,
    IconType icon_type) {
  gfx::ImageSkia icon = GetImageForNonVirtualNetwork(
      color_provider, connected_network, icon_type, false /* show_vpn_badge */);
  double animation = NetworkIconAnimation::GetInstance()->GetAnimation();
  Badges badges;
  badges.bottom_left = {
      &kUnifiedNetworkBadgeVpnIcon,
      SkColorSetA(GetDefaultColorForIconType(color_provider, icon_type),
                  0xFF * animation)};
  return CreateNetworkIconImage(icon, badges);
}

gfx::ImageSkia GetDisconnectedImageForNetworkType(
    const ui::ColorProvider* color_provider,
    NetworkType network_type,
    IconType icon_type) {
  return GetBasicImage(color_provider, icon_type, network_type,
                       false /* connected */);
}

std::u16string GetLabelForNetworkList(const NetworkStateProperties* network) {
  if (network->type == NetworkType::kCellular) {
    ActivationStateType activation_state =
        network->type_state->get_cellular()->activation_state;
    if (activation_state == ActivationStateType::kActivating) {
      return l10n_util::GetStringFUTF16(
          IDS_ASH_STATUS_TRAY_NETWORK_LIST_ACTIVATING,
          base::UTF8ToUTF16(network->name));
    }
    if (activation_state == ActivationStateType::kNotActivated ||
        activation_state == ActivationStateType::kPartiallyActivated) {
      return base::UTF8ToUTF16(network->name);
    }
  }
  // Otherwise just show the network name or 'Ethernet'.
  if (network->type == NetworkType::kEthernet)
    return l10n_util::GetStringUTF16(IDS_ASH_STATUS_TRAY_ETHERNET);
  return base::UTF8ToUTF16(network->name);
}

void PurgeNetworkIconCache(const std::set<std::string>& network_guids) {
  PurgeIconMap(ICON_TYPE_TRAY_OOBE, network_guids);
  PurgeIconMap(ICON_TYPE_TRAY_REGULAR, network_guids);
  PurgeIconMap(ICON_TYPE_DEFAULT_VIEW, network_guids);
  PurgeIconMap(ICON_TYPE_LIST, network_guids);
  PurgeIconMap(ICON_TYPE_MENU_LIST, network_guids);
}

SignalStrength GetSignalStrength(int strength) {
  // Decide whether the signal is considered weak, medium or strong based on the
  // strength index. Each signal strength corresponds to a bucket which
  // attempted to be split evenly from |kNumNetworkImages| - 1. Remainders go
  // first to the lowest bucket and then the second lowest bucket.
  const int index = StrengthIndex(strength);
  if (index == 0)
    return SignalStrength::NONE;
  const int seperations = kNumNetworkImages - 1;
  const int bucket_size = seperations / 3;

  const int weak_max = bucket_size + static_cast<int>(seperations % 3 != 0);
  const int medium_max =
      weak_max + bucket_size + static_cast<int>(seperations % 3 == 2);
  if (index <= weak_max)
    return SignalStrength::WEAK;
  else if (index <= medium_max)
    return SignalStrength::MEDIUM;

  return SignalStrength::STRONG;
}

}  // namespace network_icon
}  // namespace ash
