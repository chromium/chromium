// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_SYSTEM_NETWORK_NETWORK_ICON_H_
#define ASH_SYSTEM_NETWORK_NETWORK_ICON_H_

#include <set>
#include <string>

#include "ash/ash_export.h"
#include "chromeos/services/network_config/public/mojom/cros_network_config.mojom-forward.h"
#include "chromeos/services/network_config/public/mojom/network_types.mojom-forward.h"
#include "ui/base/models/image_model.h"
#include "ui/color/color_provider.h"
#include "ui/gfx/canvas.h"
#include "ui/gfx/image/canvas_image_source.h"
#include "ui/gfx/image/image_skia.h"

namespace ash {
namespace network_icon {

// Type of icon which dictates color theme and VPN badging
enum IconType {
  ICON_TYPE_TRAY_OOBE,     // dark icons with VPN badges, used during OOBE
  ICON_TYPE_TRAY_REGULAR,  // light icons with VPN badges, used outside of OOBE
  ICON_TYPE_TRAY_ACTIVE,   // icons with VPN badges, used when the tray is
                           // active
  ICON_TYPE_DEFAULT_VIEW,  // dark icons with VPN badges
  ICON_TYPE_LIST,          // dark icons without VPN badges; in-line status
  ICON_TYPE_FEATURE_POD,   // icons in the network feature pod button in system
                           // menu
  ICON_TYPE_FEATURE_POD_TOGGLED,   // toggled icons in the network feature pod
                                   // button in system menu
  ICON_TYPE_FEATURE_POD_DISABLED,  // disabled icons in the network feature pod
                                   // button in system menu
  ICON_TYPE_MENU_LIST,  // dark icons without VPN badges; separate status
};

// Strength of a wireless signal.
enum class SignalStrength { NONE, WEAK, MEDIUM, STRONG };

// Returns the color of an icon on the given |icon_type|.
SkColor GetDefaultColorForIconType(const ui::ColorProvider* color_provider,
                                   IconType icon_type);

// Returns an image to represent either a fully connected network or a
// disconnected network.
const gfx::ImageSkia GetBasicImage(
    const ui::ColorProvider* color_provider,
    IconType icon_type,
    chromeos::network_config::mojom::NetworkType network_type,
    bool connected);

// Returns and caches an image for non VPN |network| which must not be null.
// Use this for non virtual networks and for the default (tray) icon.
// |color_provider| and |icon_type| are used to determine the color theme.
// |badge_vpn| should be true if a VPN is also connected and a badge is desired.
// |animating| is an optional out parameter that is set to true when the
// returned image should be animated.
ASH_EXPORT gfx::ImageSkia GetImageForNonVirtualNetwork(
    const ui::ColorProvider* color_provider,
    const chromeos::network_config::mojom::NetworkStateProperties* network,
    IconType icon_type,
    bool badge_vpn,
    bool* animating = nullptr);

// Similar to above but for displaying only VPN icons, e.g. for the VPN menu
// or Settings section.
ASH_EXPORT gfx::ImageSkia GetImageForVPN(
    const ui::ColorProvider* color_provider,
    const chromeos::network_config::mojom::NetworkStateProperties* vpn,
    IconType icon_type,
    bool* animating = nullptr);

// Returns an image for Wi-Fi with no connections available, Wi-Fi icon with
// a cross in the center.
ASH_EXPORT gfx::ImageSkia GetImageForWiFiNoConnections(
    const ui::ColorProvider* color_provider,
    IconType icon_type);

// Returns an image for an unactivated PSim when device is not logged in or in
// OOBE.
ASH_EXPORT gfx::ImageSkia GetImageForPSimPendingActivationWhileLoggedOut(
    const ui::ColorProvider* color_provider,
    IconType icon_type);

// Returns an image for an carrier locked cellular network.
ASH_EXPORT gfx::ImageSkia GetImageForCarrierLockedNetwork(
    const ui::ColorProvider* color_provider,
    IconType icon_type);

// Returns an image for a Wi-Fi network, either full strength or strike-through
// based on |enabled|.
ASH_EXPORT gfx::ImageSkia GetImageForWiFiEnabledState(
    const ui::ColorProvider* color_provider,
    bool enabled,
    IconType = ICON_TYPE_DEFAULT_VIEW);

// Returns an image for a Wi-Fi network, either full strength or strike-through
// based on |enabled|. Note that this method uses the window background color
// to color the image.
ASH_EXPORT ui::ImageModel GetImageModelForWiFiEnabledState(
    bool wifi_enabled,
    IconType icon_type = ICON_TYPE_DEFAULT_VIEW);

// Returns the connecting image for a shill network non-VPN type.
gfx::ImageSkia GetConnectingImageForNetworkType(
    const ui::ColorProvider* color_provider,
    chromeos::network_config::mojom::NetworkType network_type,
    IconType icon_type);

// Returns the connected image for |connected_network| and |network_type| with a
// connecting VPN badge.
gfx::ImageSkia GetConnectedNetworkWithConnectingVpnImage(
    const ui::ColorProvider* color_provider,
    const chromeos::network_config::mojom::NetworkStateProperties*
        connected_network,
    IconType icon_type);

// Returns the disconnected image for a shill network type.
gfx::ImageSkia GetDisconnectedImageForNetworkType(
    const ui::ColorProvider* color_provider,
    chromeos::network_config::mojom::NetworkType network_type,
    IconType icon_type);

// Returns the label for |network| when displayed in a list.
ASH_EXPORT std::u16string GetLabelForNetworkList(
    const chromeos::network_config::mojom::NetworkStateProperties* network);

// Called periodically with the current list of network guids. Removes cached
// entries that are no longer in the list.
ASH_EXPORT void PurgeNetworkIconCache(
    const std::set<std::string>& network_guids);

// Called by ChromeVox to give a verbal indication of the network icon. Returns
// a signal strength enum for |strength| value 0-100.
ASH_EXPORT SignalStrength GetSignalStrength(int strength);

}  // namespace network_icon
}  // namespace ash

#endif  // ASH_SYSTEM_NETWORK_NETWORK_ICON_H_
