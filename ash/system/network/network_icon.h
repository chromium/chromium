// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_SYSTEM_NETWORK_NETWORK_ICON_H_
#define ASH_SYSTEM_NETWORK_NETWORK_ICON_H_

#include <set>
#include <string>

#include "ash/ash_export.h"
#include "base/strings/string16.h"
#include "chromeos/services/network_config/public/mojom/cros_network_config.mojom-forward.h"
#include "ui/gfx/canvas.h"
#include "ui/gfx/image/canvas_image_source.h"
#include "ui/gfx/image/image_skia.h"

namespace ash {
namespace network_icon {

// Type of icon which dictates color theme and VPN badging
enum IconType {
  ICON_TYPE_TRAY_OOBE,     // dark icons with VPN badges, used during OOBE
  ICON_TYPE_TRAY_REGULAR,  // light icons with VPN badges, used outside of OOBE
  ICON_TYPE_DEFAULT_VIEW,  // dark icons with VPN badges
  ICON_TYPE_LIST,          // dark icons without VPN badges; in-line status
  ICON_TYPE_MENU_LIST,     // dark icons without VPN badges; separate status
};

// Strength of a wireless signal.
enum class SignalStrength { NONE, WEAK, MEDIUM, STRONG };

// Returns the color of an icon on the given |icon_type|.
SkColor GetDefaultColorForIconType(IconType icon_type);

// Returns an image to represent either a fully connected network or a
// disconnected network.
const gfx::ImageSkia GetBasicImage(
    IconType icon_type,
    chromeos::network_config::mojom::NetworkType network_type,
    bool connected);

// Returns and caches an image for non VPN |network| which must not be null.
// Use this for non virtual networks and for the default (tray) icon.
// |icon_type| determines the color theme.
// |badge_vpn| should be true if a VPN is also connected and a badge is desired.
// |animating| is an optional out parameter that is set to true when the
// returned image should be animated.
ASH_EXPORT gfx::ImageSkia GetImageForNonVirtualNetwork(
    const chromeos::network_config::mojom::NetworkStateProperties* network,
    IconType icon_type,
    bool badge_vpn,
    bool* animating = nullptr);

// Similar to above but for displaying only VPN icons, e.g. for the VPN menu
// or Settings section.
ASH_EXPORT gfx::ImageSkia GetImageForVPN(
    const chromeos::network_config::mojom::NetworkStateProperties* vpn,
    IconType icon_type,
    bool* animating = nullptr);

// Returns an image for a Wi-Fi network, either full strength or strike-through
// based on |enabled|.
ASH_EXPORT gfx::ImageSkia GetImageForWiFiEnabledState(
    bool enabled,
    IconType = ICON_TYPE_DEFAULT_VIEW);

// Returns the connecting image for a shill network non-VPN type.
gfx::ImageSkia GetConnectingImageForNetworkType(
    chromeos::network_config::mojom::NetworkType network_type,
    IconType icon_type);

// Returns the connected image for |connected_network| and |network_type| with a
// connecting VPN badge.
gfx::ImageSkia GetConnectedNetworkWithConnectingVpnImage(
    const chromeos::network_config::mojom::NetworkStateProperties*
        connected_network,
    IconType icon_type);

// Returns the disconnected image for a shill network type.
gfx::ImageSkia GetDisconnectedImageForNetworkType(
    chromeos::network_config::mojom::NetworkType network_type);

// Returns the label for |network| when displayed in a list.
ASH_EXPORT base::string16 GetLabelForNetworkList(
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
