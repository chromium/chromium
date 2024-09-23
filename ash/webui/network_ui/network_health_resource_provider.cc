// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/webui/network_ui/network_health_resource_provider.h"

#include "ash/constants/ash_features.h"
#include "ash/webui/grit/ash_webui_common_resources.h"
#include "chromeos/strings/grit/chromeos_strings.h"
#include "content/public/browser/web_ui_data_source.h"
#include "ui/base/webui/web_ui_util.h"

namespace ash {
namespace network_health {

namespace {

constexpr webui::LocalizedString kLocalizedStrings[] = {
    // Network Health Summary Strings
    {"NetworkHealthState", IDS_NETWORK_HEALTH_STATE},
    {"NetworkHealthStateUninitialized", IDS_NETWORK_HEALTH_STATE_UNINITIALIZED},
    {"NetworkHealthStateDisabled", IDS_NETWORK_HEALTH_STATE_DISABLED},
    {"NetworkHealthStateProhibited", IDS_NETWORK_HEALTH_STATE_PROHIBITED},
    {"NetworkHealthStateNotConnected", IDS_NETWORK_HEALTH_STATE_NOT_CONNECTED},
    {"NetworkHealthStateConnecting", IDS_NETWORK_HEALTH_STATE_CONNECTING},
    {"NetworkHealthStatePortal", IDS_NETWORK_HEALTH_STATE_PORTAL},
    {"NetworkHealthStateConnected", IDS_NETWORK_HEALTH_STATE_CONNECTED},
    {"NetworkHealthStateOnline", IDS_NETWORK_HEALTH_STATE_ONLINE},
    {"OpenInSettings", IDS_NETWORK_HEALTH_OPEN_IN_SETTINGS},

    {"OncType", IDS_NETWORK_TYPE},
    {"OncName", IDS_ONC_NAME},
    {"OncTypeCellular", IDS_NETWORK_TYPE_CELLULAR},
    {"OncTypeEthernet", IDS_NETWORK_TYPE_ETHERNET},
    {"OncTypeMobile", IDS_NETWORK_TYPE_MOBILE_DATA},
    {"OncTypeTether", IDS_NETWORK_TYPE_TETHER},
    {"OncTypeVPN", IDS_NETWORK_TYPE_VPN},
    {"OncTypeWireless", IDS_NETWORK_TYPE_WIRELESS},
    {"OncTypeWiFi", IDS_NETWORK_TYPE_WIFI},
    {"OncWiFi-SignalStrength", IDS_ONC_WIFI_SIGNAL_STRENGTH},
    {"OncMacAddress", IDS_ONC_MAC_ADDRESS},
    {"OncIpv4Address", IDS_ONC_IPV4_ADDRESS},
    {"OncIpv6Address", IDS_ONC_IPV6_ADDRESS},
    {"OncPortalState", IDS_ONC_PORTAL_STATE},
    {"OncPortalStateOnline", IDS_ONC_PORTAL_STATE_ONLINE},
    {"OncPortalStateUnknown", IDS_ONC_PORTAL_STATE_UNKNOWN},
    {"OncPortalStateNoInternet", IDS_ONC_PORTAL_STATE_NO_INTERNET},
    {"OncPortalStatePortal", IDS_ONC_PORTAL_STATE_PORTAL},
    {"OncPortalStatePortalSuspected", IDS_ONC_PORTAL_STATE_PORTAL_SUSPECTED},
    {"OncPortalStateProxyAuthRequired", IDS_ONC_PORTAL_STATE_PROXY_AUTH},

};

struct WebUiResource {
  const char* name;
  int id;
};

constexpr WebUiResource kResources[] = {
    {"ethernet.svg", IDR_ASH_WEBUI_COMMON_NETWORK_ETHERNET_SVG},
    {"vpn.svg", IDR_ASH_WEBUI_COMMON_NETWORK_VPN_SVG},
    {"wifi_0.svg", IDR_ASH_WEBUI_COMMON_NETWORK_WIFI_0_SVG},
    {"cellular_0.svg", IDR_ASH_WEBUI_COMMON_NETWORK_CELLULAR_0_SVG},
};

}  // namespace

void AddResources(content::WebUIDataSource* html_source) {
  html_source->AddLocalizedStrings(kLocalizedStrings);

  html_source->AddLocalizedString(
      "OncTypeTether", ash::features::IsInstantHotspotRebrandEnabled()
                           ? IDS_NETWORK_TYPE_HOTSPOT
                           : IDS_NETWORK_TYPE_TETHER);

  for (const auto& resource : kResources)
    html_source->AddResourcePath(resource.name, resource.id);
}

}  // namespace network_health
}  // namespace ash
