// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/media/router/discovery/discovery_network_list_wifi.h"

#include <CoreWLAN/CoreWLAN.h>

#include <string>
#include <utility>

#include "base/check.h"
#include "base/strings/sys_string_conversions.h"

namespace media_router {
namespace {

bool GetWifiSSID(CWInterface* interface, std::string* ssid_out) {
  CHECK(interface);
  std::string ssid(base::SysNSStringToUTF8(interface.ssid));
  if (ssid.empty()) {
    return false;
  }
  ssid_out->assign(std::move(ssid));
  return true;
}

}  // namespace

bool MaybeGetWifiSSID(const std::string& if_name, std::string* ssid_out) {
  DCHECK(ssid_out);

  NSString* ns_ifname = base::SysUTF8ToNSString(if_name.data());
  NSArray<CWInterface*>* all_interfaces =
      [[CWWiFiClient sharedWiFiClient] interfaces];
  if (all_interfaces == nil) {
    return false;
  }
  for (CWInterface* interface in all_interfaces) {
    if (interface && [interface.interfaceName isEqualToString:ns_ifname]) {
      return GetWifiSSID(interface, ssid_out);
    }
  }
  return false;
}

}  // namespace media_router
