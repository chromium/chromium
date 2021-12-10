// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_MEDIA_ROUTER_DISCOVERY_ACCESS_CODE_ACCESS_CODE_TEST_UTIL_H_
#define CHROME_BROWSER_MEDIA_ROUTER_DISCOVERY_ACCESS_CODE_ACCESS_CODE_TEST_UTIL_H_

#include "chrome/browser/media/router/discovery/access_code/discovery_resources.pb.h"
#include "chrome/browser/media/router/discovery/mdns/media_sink_util.h"
#include "components/media_router/common/discovery/media_sink_internal.h"

namespace media_router {

const char kExpectedDisplayName[] = "test_device";
const char kExpectedSinkId[] = "1234";
const char kExpectedPort[] = "666";
const char kExpectedIpV4[] = "192.0.2.146";
const char kExpectedIpV6[] = "2001:0db8:85a3:0000:0000:8a2e:0370:7334";

using DiscoveryDevice = chrome_browser_media::proto::DiscoveryDevice;
using NetworkInfo = chrome_browser_media::proto::NetworkInfo;

// This function is used for testing across the access_code directory
DiscoveryDevice BuildDiscoveryDeviceProto(
    const char* display_name = kExpectedDisplayName,
    const char* sink_id = kExpectedSinkId,
    const char* port = kExpectedPort,
    const char* ip_v4 = kExpectedIpV4,
    const char* ip_v6 = kExpectedIpV6,
    bool set_device_capabilities = true,
    bool set_network_info = true);

}  // namespace media_router

#endif  // CHROME_BROWSER_MEDIA_ROUTER_DISCOVERY_ACCESS_CODE_ACCESS_CODE_TEST_UTIL_H_
