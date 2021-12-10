// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/media/router/discovery/access_code/access_code_media_sink_util.h"

#include <map>

#include "base/command_line.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_split.h"
#include "base/strings/string_util.h"
#include "base/strings/stringprintf.h"
#include "chrome/browser/media/router/discovery/mdns/media_sink_util.h"
#include "components/cast_channel/cast_socket.h"
#include "components/media_router/common/mojom/media_router.mojom.h"
#include "net/base/host_port_pair.h"
#include "net/base/ip_address.h"
#include "net/base/port_util.h"
#include "net/base/url_util.h"

namespace media_router {

namespace {

uint8_t ConvertDeviceCapabilitiesToInt(DeviceCapabilities proto) {
  // Meaning of capacity value for each bit:
  // NONE: 0,
  // VIDEO_OUT: 1 << 0,
  // VIDEO_IN: 1 << 1,
  // AUDIO_OUT: 1 << 2,
  // AUDIO_IN: 1 << 3,
  // DEV_MODE: 1 << 4,
  uint8_t bool_sum = 0;
  if (proto.video_out())
    bool_sum += 1 << 0;
  if (proto.video_in())
    bool_sum += 1 << 1;
  if (proto.audio_out())
    bool_sum += 1 << 2;
  if (proto.audio_in())
    bool_sum += 1 << 3;
  if (proto.dev_mode())
    bool_sum += 1 << 4;
  return bool_sum;
}

absl::optional<net::IPAddress> GetIPAddress(NetworkInfo network_info) {
  net::IPAddress ip_address;
  const std::string& ip_v6_address = network_info.ip_v6_address();
  const std::string& ip_v4_address = network_info.ip_v4_address();

  // Prioritize using IP_V6 over IP_V4
  if (!ip_v6_address.empty() && ip_address.AssignFromIPLiteral(ip_v6_address)) {
    return ip_address;
  } else if (!ip_v4_address.empty() &&
             ip_address.AssignFromIPLiteral(ip_v4_address)) {
    return ip_address;
  }
  return absl::nullopt;
}
}  // namespace

std::pair<absl::optional<MediaSinkInternal>, CreateCastMediaSinkResult>
CreateAccessCodeMediaSink(const DiscoveryDevice& discovery_device) {
  if (!discovery_device.has_network_info()) {
    return std::make_pair(absl::nullopt,
                          CreateCastMediaSinkResult::kMissingNetworkInfo);
  }
  absl::optional<net::IPAddress> ip_address =
      GetIPAddress(discovery_device.network_info());
  if (!ip_address.has_value()) {
    return std::make_pair(
        absl::nullopt, CreateCastMediaSinkResult::kMissingOrInvalidIPAddress);
  }

  const std::string& unique_id = discovery_device.id();
  if (unique_id.empty()) {
    return std::make_pair(absl::nullopt, CreateCastMediaSinkResult::kMissingID);
  }

  const std::string& display_name = discovery_device.display_name();
  if (display_name.empty()) {
    return std::make_pair(absl::nullopt,
                          CreateCastMediaSinkResult::kMissingFriendlyName);
  }

  CastSinkExtraData extra_data;
  const std::string& port = discovery_device.network_info().port();
  int port_value = 0;
  // Convert port from string to int
  if (port.empty() || !base::StringToInt(port, &port_value)) {
    return std::make_pair(absl::nullopt,
                          CreateCastMediaSinkResult::kMissingOrInvalidPort);
  }
  if (!net::IsPortValid(port_value)) {
    return std::make_pair(absl::nullopt,
                          CreateCastMediaSinkResult::kMissingOrInvalidPort);
  }
  uint16_t valid_port = static_cast<uint16_t>(port_value);
  extra_data.ip_endpoint = net::IPEndPoint(ip_address.value(), valid_port);
  if (!discovery_device.has_device_capabilities()) {
    return std::make_pair(
        absl::nullopt, CreateCastMediaSinkResult::kMissingDeviceCapabilities);
  }
  extra_data.capabilities =
      ConvertDeviceCapabilitiesToInt(discovery_device.device_capabilities());

  const std::string& processed_uuid =
      MediaSinkInternal::ProcessDeviceUUID(unique_id);
  const std::string& sink_id =
      base::StringPrintf("cast:<%s>", processed_uuid.c_str());
  MediaSink sink(sink_id, display_name,
                 GetCastSinkIconType(extra_data.capabilities),
                 mojom::MediaRouteProviderId::CAST);

  MediaSinkInternal cast_sink;
  cast_sink.set_sink(sink);
  cast_sink.set_cast_data(extra_data);

  return std::make_pair(cast_sink, CreateCastMediaSinkResult::kOk);
}
}  // namespace media_router
