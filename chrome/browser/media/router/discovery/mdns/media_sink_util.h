// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_MEDIA_ROUTER_DISCOVERY_MDNS_MEDIA_SINK_UTIL_H_
#define CHROME_BROWSER_MEDIA_ROUTER_DISCOVERY_MDNS_MEDIA_SINK_UTIL_H_

#include <vector>

#include "components/media_router/common/discovery/media_sink_internal.h"
#include "components/media_router/common/providers/cast/channel/cast_device_capability.h"

namespace media_router {

struct DnsSdService;

// The DNS-SD service type for Cast devices.
static constexpr char kCastServiceType[] = "_googlecast._tcp.local";

// Returns the icon type to use according to |capabilities|. |capabilities| is
// a bit set of cast_channel::CastDeviceCapabilities in CastSinkExtraData.
SinkIconType GetCastSinkIconType(
    cast_channel::CastDeviceCapabilitySet capabilities);

enum CreateCastMediaSinkResult {
  kOk,
  kNotCastDevice,
  kMissingID,
  kMissingFriendlyName,
  kMissingOrInvalidIPAddress,
  kMissingOrInvalidPort,
  kMissingDeviceCapabilities,
  kMissingNetworkInfo
};

// Creates a MediaSinkInternal from |service| and assigns the result to
// |cast_sink|. |cast_sink| is only valid if the returned result is |kOk|.
CreateCastMediaSinkResult CreateCastMediaSink(const DnsSdService& service,
                                              MediaSinkInternal* cast_sink);

// Command line flag for a list of Cast device IPs to connect to at startup.
// The value should be a comma-separated list of IP endpoints.
static constexpr char kFixedCastDeviceIps[] = "media-router-cast-device-ips";

// Returns a list of Cast sinks whose IPs were specified in the command line
// flag |kFixedCastDeviceIps|.
std::vector<MediaSinkInternal> GetFixedIPSinksFromCommandLine();

}  // namespace media_router

#endif  // CHROME_BROWSER_MEDIA_ROUTER_DISCOVERY_MDNS_MEDIA_SINK_UTIL_H_
