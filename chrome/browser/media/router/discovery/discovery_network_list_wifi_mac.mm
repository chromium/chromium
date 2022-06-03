// Copyright (c) 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/media/router/discovery/discovery_network_list_wifi.h"

#include <CoreWLAN/CoreWLAN.h>

#include <string>
#include <utility>

#include "base/check.h"
#include "base/strings/sys_string_conversions.h"

// TODO(crbug.com/841631): This file uses the deprecated CWInterface interface;
// it needs to be migrated to CWWiFiClient, which is unfortunately not
// compatible.
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wdeprecated-declarations"

namespace media_router {
namespace {

bool GetWifiSSID(NSString* ns_ifname, std::string* ssid_out) {
  CWInterface* interface = [CWInterface interfaceWithName:ns_ifname];
  if (interface == nil) {
    return false;
  }
  std::string ssid(base::SysNSStringToUTF8([interface ssid]));
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
  NSSet* all_ifnames = [CWInterface interfaceNames];
  for (NSString* ifname in all_ifnames)
    if ([ifname isEqualToString:ns_ifname])
      return GetWifiSSID(ns_ifname, ssid_out);
  return false;
}

}  // namespace media_router

#pragma clang diagnostic pop
