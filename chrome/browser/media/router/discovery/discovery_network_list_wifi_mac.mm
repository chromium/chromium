// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/media/router/discovery/discovery_network_list_wifi.h"

#include <CoreWLAN/CoreWLAN.h>

#include <optional>
#include <string>
#include <string_view>

#include "base/check.h"
#include "base/strings/sys_string_conversions.h"

namespace media_router {
namespace {

std::optional<std::string> GetWifiSSID(CWInterface* interface) {
  CHECK(interface);
  std::string ssid(base::SysNSStringToUTF8(interface.ssid));
  if (ssid.empty()) {
    return std::nullopt;
  }
  return ssid;
}

}  // namespace

std::optional<std::string> MaybeGetWifiSSID(std::string_view if_name) {
  NSString* ns_ifname = base::SysUTF8ToNSString(if_name);
  NSArray<CWInterface*>* all_interfaces =
      [[CWWiFiClient sharedWiFiClient] interfaces];
  if (all_interfaces == nil) {
    return std::nullopt;
  }
  for (CWInterface* interface in all_interfaces) {
    if (interface && [interface.interfaceName isEqualToString:ns_ifname]) {
      return GetWifiSSID(interface);
    }
  }
  return std::nullopt;
}

}  // namespace media_router
